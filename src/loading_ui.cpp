#include "loading_ui.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <numeric>
#include <vector>

#include "cached_options.h"
#include "cata_scope_helpers.h"
#include "input.h"
#include "output.h"
#include "ui_manager.h"

#if defined(TILES)
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#undef IMGUI_DEFINE_MATH_OPERATORS
#include "mod_manager.h"
#include "path_info.h"
#include "sdltiles.h"
#include "sdl_wrappers.h"
#include "worldfactory.h"
#else
#include "cursesdef.h"
#endif // TILES

#if defined(TILES)
/**
 * Whether SDL_image can decode an animation. IMG_LoadAnimation arrived in
 * SDL_image 2.6.0, and the build only asks for libsdl2-image-dev with no minimum
 * version, so this cannot be assumed - Ubuntu 22.04 still ships 2.0.5. Where it
 * is missing an animated splash loads as its first frame, which is what every
 * build did before.
 *
 * Kept local to this file on purpose. Declaring it in sdl_wrappers.h would put it
 * behind the precompiled header, which pulls that header in and is not rebuilt
 * when it changes, so the declaration would go missing on any incremental build.
 */
#if defined(SDL_IMAGE_MAJOR_VERSION) && \
    ( SDL_IMAGE_MAJOR_VERSION > 2 || \
      ( SDL_IMAGE_MAJOR_VERSION == 2 && SDL_IMAGE_MINOR_VERSION >= 6 ) )
#define CATA_LOADING_ANIMATION 1
#else
#define CATA_LOADING_ANIMATION 0
#endif

#if CATA_LOADING_ANIMATION
struct IMG_Animation_deleter {
    void operator()( IMG_Animation *const ptr ) const {
        IMG_FreeAnimation( ptr );
    }
};
using IMG_Animation_Ptr = std::unique_ptr<IMG_Animation, IMG_Animation_deleter>;
#endif
#endif // TILES

struct ui_state {
    ui_adaptor *ui;
    background_pane *bg;
#ifdef TILES
    ImVec2 splash_size;
    // One entry per frame, with the time each is shown.
    std::vector<SDL_Texture_Ptr> splash_frames;
    std::vector<int> splash_delays;
    int splash_loop_ms = 0;
    std::chrono::steady_clock::time_point splash_start;
    cata_path chosen_load_img;
#else
    int splash_width = 0;
    std::vector<std::string> splash;
    std::string blanks;
#endif
    std::string context;
    std::string step;
    std::chrono::steady_clock::time_point last_present;
    // How often tick() is willing to redraw.
    // Taken from the art file once it is loaded - see set_present_interval
    int present_interval_ms = 50;
};

static ui_state *gLUI = nullptr;

// Bounds on the redraw interval derived in set_present_interval.
// The base is a little over one frame at 60Hz,
// since redrawing faster than the display cannot show anything.
static constexpr int MIN_PRESENT_INTERVAL_MS = 8;
static constexpr int MAX_PRESENT_INTERVAL_MS = 50;

#ifdef TILES
/**
 * Choose how often to redraw, from the animation that was actually loaded.
 *
 * Sampling a frame sequence at its own period is the one interval guaranteed to
 * look wrong: each frame gets one sample at best, and any jitter drops it
 * outright. Frames long enough to span several samples survive that, short ones
 * do not, so an animation with mixed timings skips only in its fast passages.
 * Halving the shortest frame gives every frame at least two chances to be drawn.
 */
static void set_present_interval()
{
    int shortest = MAX_PRESENT_INTERVAL_MS;
    for( const int d : gLUI->splash_delays ) {
        if( d > 0 && d < shortest ) {
            shortest = d;
        }
    }
    gLUI->present_interval_ms = std::max( MIN_PRESENT_INTERVAL_MS,
                                          std::min( MAX_PRESENT_INTERVAL_MS, shortest / 2 ) );
}
#endif // TILES

#ifdef TILES
/**
 * Turn vsync off for the duration of the load, and back on afterwards.
 *
 * The renderer is created with SDL_RENDERER_PRESENTVSYNC, so every present waits
 * for the next vertical blank. Measured on a real load that is a median of 16ms
 * per present - a full refresh, every time - which is the right trade for the
 * game and the wrong one for a loading screen. Redrawing often enough to keep an
 * animation alive would otherwise cost about a third of the time of every step it
 * runs inside. Tearing on a splash costs nothing.
 *
 * This must be undone whenever the loading screen goes away, including when it
 * goes away because a load threw - leaving it off would run the rest of the
 * session unsynced with nothing to indicate why. That path is covered without
 * extra work: game.cpp catches a failed load and reports it with debugmsg, and
 * realDebugmsg calls loading_ui::done() to get the splash off the screen before
 * it prompts.
 *
 * SDL_RenderSetVSync arrived in SDL 2.0.18. On anything older this does nothing
 * and redraws simply keep costing a refresh each.
 */
static void set_loading_vsync( const bool on )
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
    const SDL_Renderer_Ptr &renderer = get_sdl_renderer();
    if( renderer ) {
        // A software renderer has no vsync to set and will fail here.
        SDL_RenderSetVSync( renderer.get(), on ? 1 : 0 );
    }
#else
    static_cast<void>( on );
#endif
}
#endif // TILES

#ifdef TILES
static SDL_Texture *current_splash_frame()
{
    if( gLUI->splash_frames.empty() ) {
        return nullptr;
    }
    if( gLUI->splash_frames.size() == 1 || gLUI->splash_loop_ms <= 0 ) {
        return gLUI->splash_frames.front().get();
    }
    // The frame comes from the clock, not from a counter that advances once per
    // redraw. Redraws happen when a load step finishes, which is irregular and
    // occasionally seconds apart, so a counter would play the animation at the
    // speed of the loading instead of the speed it was authored at. Going by
    // elapsed time means a slow step drops frames rather than stretching them.
    const auto elapsed = std::chrono::steady_clock::now() - gLUI->splash_start;
    int ms = static_cast<int>(
                 std::chrono::duration_cast<std::chrono::milliseconds>( elapsed ).count()
                 % gLUI->splash_loop_ms );
    for( size_t i = 0; i < gLUI->splash_frames.size(); ++i ) {
        ms -= gLUI->splash_delays[i];
        if( ms < 0 ) {
            return gLUI->splash_frames[i].get();
        }
    }
    return gLUI->splash_frames.back().get();
}
#endif // TILES

static void redraw()
{
#ifdef TILES
    // Fit the splash to the window rather than drawing it at native size.
    // old splash was too big and shoved progress bar off screen
    const ImVec2 viewport = ImGui::GetMainViewport()->Size;
    const float text_h = 2.0f * ImGui::GetTextLineHeightWithSpacing();
    // The window's usable area is its size less WindowPadding on each side, so
    // the padding has to come off the budget before fitting and go back on when
    // sizing the window - otherwise the image is clipped by exactly that much.
    const ImVec2 pad = ImGui::GetStyle().WindowPadding;
    ImVec2 img = gLUI->splash_size;
    if( img.x > 0.0f && img.y > 0.0f ) {
        const float avail_x = std::max( viewport.x * 0.98f - pad.x * 2.0f, 1.0f );
        const float avail_y = std::max( viewport.y * 0.98f - pad.y * 2.0f - text_h, 1.0f );
        const float fit = std::min( avail_x / img.x, avail_y / img.y );
        // Enlarging snaps image to grid to keep it even, shrinking unneeded
        // recalulated each frame in case of resize
        const float scale = fit >= 1.0f ? static_cast<float>( static_cast<int>( fit ) ) : fit;
        img = { img.x * scale, img.y * scale };
    }

    ImVec2 pos = { 0.5f, 0.5f };
    ImGui::SetNextWindowPos( viewport * pos, ImGuiCond_Always, { 0.5f, 0.5f } );
    ImGui::SetNextWindowSize( ImVec2{ img.x + pad.x * 2.0f, img.y + text_h + pad.y * 2.0f } );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, { 0.0f, 0.0f, 0.0f, 1.0f } );
    if( ImGui::Begin( "Loading…", nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings ) ) {
        ImGui::Image( static_cast<void *>( current_splash_frame() ), img );
        // extra stuff to make sure an image smaller than the progress bar
        // doesn't cause issues and shove the text offscreen via negative cursor
        ImGui::SetCursorPosX( std::max( ( img.x / 2.0f ) - 120.0f, 0.0f ) );
        ImGui::TextUnformatted( gLUI->context.c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( gLUI->step.c_str() );
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
#else
    // art wider than the terminal would give a negative x,
    // then print_colored_text skips its wmove
    int x = std::max( 0, ( TERMX - gLUI->splash_width ) / 2 );
    int y = 0;
    nc_color white = c_white;
    for( const std::string &line : gLUI->splash ) {
        if( !line.empty() && line[0] == '#' ) {
            continue;
        }
        print_colored_text( catacurses::stdscr, point( x, y++ ), white,
                            white, line );
    }
    mvwprintz( catacurses::stdscr, point( 0, TERMY - 1 ), c_black, gLUI->blanks );
    center_print( catacurses::stdscr, TERMY - 1, c_white, string_format( "%s %s",
                  gLUI->context.c_str(), gLUI->step.c_str() ) );
#endif
}

static void resize()
{
}

static void update_state( const std::string &context, const std::string &step )
{
    if( gLUI == nullptr ) {
        gLUI = new struct ui_state;
#ifdef TILES
        set_loading_vsync( false );
#endif
        gLUI->bg = new background_pane;
        gLUI->ui = new ui_adaptor;
        gLUI->ui->is_imgui = true;
        gLUI->ui->on_redraw( []( ui_adaptor & ) {
            redraw();
        } );
        gLUI->ui->on_screen_resize( []( ui_adaptor & ) {
            resize();
        } );

#ifdef TILES
        std::vector<cata_path> imgs;
        std::vector<mod_id> &active_mod_list = world_generator->active_world->active_mod_order;
        for( mod_id &some_mod : active_mod_list ) {
            const MOD_INFORMATION &mod = *some_mod;
            for( const std::string &img_name : mod.loading_images ) {
                // There may be more than one file matching the name, so we need to get all of them
                for( cata_path &img_path : get_files_from_path( img_name, mod.path, true ) ) {
                    imgs.emplace_back( img_path );
                }
            }
        }
        if( gLUI->chosen_load_img == cata_path() ) {
            if( imgs.empty() ) {
                gLUI->chosen_load_img = PATH_INFO::gfxdir() / "splash-animated.gif"; //default load screen
            } else {
                gLUI->chosen_load_img = random_entry( imgs );
            }
        }
        const std::string img_path = gLUI->chosen_load_img.get_unrelative_path().u8string();
        gLUI->splash_start = std::chrono::steady_clock::now();
#if CATA_LOADING_ANIMATION
        // Animated splash meant to be gif for SDL
        // A still image would generate an error so we erase it
        // so that if something else fails its not blocked by the still.
        IMG_Animation_Ptr anim( IMG_LoadAnimation( img_path.c_str() ) );
        if( anim && anim->count > 0 && anim->frames && anim->delays ) {
            gLUI->splash_size = { static_cast<float>( anim->w ), static_cast<float>( anim->h ) };
            for( int i = 0; i < anim->count; ++i ) {
                SDL_Texture_Ptr frame( SDL_CreateTextureFromSurface( get_sdl_renderer().get(),
                                       anim->frames[i] ) );
                if( !frame ) {
                    // animation should be a small gif but just in case have a fallback
                    // if we would run out of texture memory, fall back to still image
                    gLUI->splash_frames.clear();
                    gLUI->splash_delays.clear();
                    break;
                }
                // SDL_image forces pauses in gifs of <20ms to 100ms
                gLUI->splash_delays.push_back( std::max( 0, anim->delays[i] ) );
                gLUI->splash_frames.emplace_back( std::move( frame ) );
            }
        }
        anim.reset();
        SDL_ClearError();
#endif
        if( gLUI->splash_frames.empty() ) {
            SDL_Surface_Ptr surf = load_image( img_path.c_str() );
            gLUI->splash_size = { static_cast<float>( surf->w ), static_cast<float>( surf->h ) };
            gLUI->splash_frames.emplace_back( CreateTextureFromSurface( get_sdl_renderer(), surf ) );
            gLUI->splash_delays.push_back( 0 );
        }
        // Zero if every delay was zero, which leaves current_splash_frame() on the
        // first frame instead of dividing by it.
        gLUI->splash_loop_ms = std::accumulate( gLUI->splash_delays.begin(),
                                                gLUI->splash_delays.end(), 0 );
        set_present_interval();
        // No window size is cached here any more: redraw() derives it from the
        // viewport each frame, so the splash follows a resize.
#else
        std::string splash = read_whole_file( PATH_INFO::title( get_holiday_from_time() ) ).value_or(
                                 _( "Cataclysm: Salvaged" ) );
        gLUI->splash = string_split( splash, '\n' );
        gLUI->blanks = std::string( TERMX, ' ' );
        for( const std::string &line : gLUI->splash ) {
            if( !line.empty() && line[0] == '#' ) {
                continue;
            }
            // Display columns, not bytes: multi-byte art (en.halloween) would
            // otherwise measure several times its true width and shift left.
            gLUI->splash_width = std::max( gLUI->splash_width, utf8_width( line, true ) );
        }
#endif
    }
    gLUI->context = std::string( context );
    gLUI->step = std::string( step );
}

static void present()
{
    // take timestamp early so redraw cannot cause issues
    // from debugmsg's that would remove gLUI
    if( gLUI == nullptr ) {
        return;
    }
    gLUI->last_present = std::chrono::steady_clock::now();
    ui_manager::redraw();
    refresh_display();
    inp_mngr.pump_events();
}

void loading_ui::show( const std::string &context, const std::string &step )
{
    if( test_mode ) {
        return;
    }
    update_state( context, step );
    present();
}

void loading_ui::tick()
{
    // Nothing to interrupt if no loading screen is up.
    if( test_mode || gLUI == nullptr ) {
        return;
    }
    // Presenting pumps events, pumping events ticks, so this re-enters once.
    // dont want to rely on the rate limit to stop it.
    static bool in_tick = false;
    if( in_tick ) {
        return;
    }
    const on_out_of_scope clear_in_tick( [] {
        in_tick = false;
    } );
    in_tick = true;
    // A present is not free, so this is capped.
    // The interval comes from the loaded animation - see set_present_interval
    // and is half its shortest frame because sampling at the frame period
    // drops frames whenever the timing jitters.
    const auto now = std::chrono::steady_clock::now();
    if( now - gLUI->last_present < std::chrono::milliseconds( gLUI->present_interval_ms ) ) {
        return;
    }
    present();
}

void loading_ui::done()
{
    if( gLUI != nullptr ) {
#ifdef TILES
        set_loading_vsync( true );
        gLUI->chosen_load_img = cata_path();
#endif
        delete gLUI->ui;
        delete gLUI->bg;
        delete gLUI;
        gLUI = nullptr;
    }
}
