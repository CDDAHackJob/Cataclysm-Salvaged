#include "terminal_backdrop.h"

#if defined(TILES)

#include <algorithm>
#include <cmath>

#include "cata_tiles.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"

namespace
{

// Source art at its own size, kept so a resize can rebuild without hitting disk.
SDL_Texture_Ptr source;
int source_w = 0;
int source_h = 0;
terminal_backdrop::fit source_fit = terminal_backdrop::fit::cover;
int source_dim = 100;

// Exactly render-target sized, so a fill copies a rect onto itself.
SDL_Texture_Ptr canvas;
int canvas_w = 0;
int canvas_h = 0;

// Where the backdrop gives way to draw_window()'s black, in render-target
// pixels. Tested per fill, not baked into the canvas - it is set mid-redraw.
// Zero width or height means none.
SDL_Rect opaque = { 0, 0, 0, 0 };

// Depth of scoped_enable nesting. Zero means loaded but not drawing.
int enabled = 0;

void rebuild_canvas()
{
    canvas.reset();
    if( !source || canvas_w <= 0 || canvas_h <= 0 ) {
        return;
    }
    const SDL_Renderer_Ptr &renderer = get_sdl_renderer();
    if( !renderer ) {
        return;
    }
    canvas = CreateTexture( renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                            canvas_w, canvas_h );
    if( !canvas ) {
        return;
    }
    // Required, not incidental: art with alpha would leave the canvas
    // translucent and make every fill blend.
    SetTextureBlendMode( canvas, SDL_BLENDMODE_NONE );

    SetRenderTarget( renderer, canvas );
    // Opaque black first, so a letterboxed edge needs no second code path.
    SetRenderDrawColor( renderer, 0, 0, 0, 255 );
    RenderClear( renderer );

    // Every fit centres; overflow is cropped by the render target.
    const double fit_x = static_cast<double>( canvas_w ) / source_w;
    const double fit_y = static_cast<double>( canvas_h ) / source_h;
    double scale = 1.0;
    switch( source_fit ) {
        case terminal_backdrop::fit::contain:
            // Smaller ratio fits both axes; the shortfall stays black, which is
            // where the menu column sits. NOT snapped to a whole factor unlike
            // loading_ui - at 2560x1440 that takes 1.41 down to 1.00, covering
            // 37% of the screen instead of 75%.
            scale = std::min( fit_x, fit_y );
            break;
        case terminal_backdrop::fit::cover:
            // Larger ratio fills both axes; excess runs off two edges. Not
            // snapped either - rounding down would uncover an edge.
            scale = std::max( fit_x, fit_y );
            break;
        case terminal_backdrop::fit::native:
            break;
    }
    SDL_Rect dst;
    dst.w = static_cast<int>( std::lround( source_w * scale ) );
    dst.h = static_cast<int>( std::lround( source_h * scale ) );
    dst.x = ( canvas_w - dst.w ) / 2;
    dst.y = ( canvas_h - dst.h ) / 2;

    // Baked in once rather than per fill; the mod is restored so it cannot leak
    // into whatever draws with this texture next.
    const Uint32 v = static_cast<Uint32>( 255 * source_dim / 100 );
    SetTextureColorMod( source, v, v, v );
    RenderCopy( renderer, source, nullptr, &dst );
    SetTextureColorMod( source, 255, 255, 255 );

    set_displaybuffer_rendertarget();
}

void copy_region( const SDL_Rect &r )
{
    if( r.w <= 0 || r.h <= 0 ) {
        return;
    }
    // Raw, not the wrapper: this runs per touched line per frame and the wrapper
    // logs on failure. A backdrop that cannot draw should leave the black alone,
    // not fill the log.
    SDL_RenderCopy( get_sdl_renderer().get(), canvas.get(), &r, &r );
}

} // namespace

bool terminal_backdrop::set( const std::string &path, const fit how, const int dim_percent )
{
    clear();
    if( path.empty() ) {
        return false;
    }
    // Not load_image(): it throws, and a missing backdrop is an ordinary
    // outcome. Clearing the SDL error keeps it off whatever fails next.
    SDL_Surface_Ptr surf( IMG_Load( path.c_str() ) );
    if( !surf ) {
        SDL_ClearError();
        return false;
    }
    if( surf->w <= 0 || surf->h <= 0 ) {
        return false;
    }
    source = CreateTextureFromSurface( get_sdl_renderer(), surf );
    if( !source ) {
        return false;
    }
    source_w = surf->w;
    source_h = surf->h;
    source_fit = how;
    source_dim = std::clamp( dim_percent, 0, 100 );
    rebuild_canvas();
    return static_cast<bool>( canvas );
}

void terminal_backdrop::clear()
{
    canvas.reset();
    source.reset();
    source_w = 0;
    source_h = 0;
    source_dim = 100;
    opaque = { 0, 0, 0, 0 };
}

terminal_backdrop::scoped_enable::scoped_enable()
{
    ++enabled;
}

terminal_backdrop::scoped_enable::~scoped_enable()
{
    --enabled;
}

bool terminal_backdrop::active()
{
    return canvas && enabled > 0;
}

bool terminal_backdrop::loaded()
{
    return static_cast<bool>( canvas );
}

void terminal_backdrop::set_opaque_cells( const point &pos, const int width, const int height )
{
    // Recorded only - called mid-redraw, so it must not rebuild the canvas and
    // switch the render target. fill() pays a rect test per line instead.
    opaque = { pos.x * fontwidth, pos.y * fontheight,
               width * fontwidth, height *fontheight
             };
}

void terminal_backdrop::clear_opaque_cells()
{
    opaque = { 0, 0, 0, 0 };
}

void terminal_backdrop::fill( const point &pos, const int width, const int height )
{
    if( !active() || width <= 0 || height <= 0 ) {
        return;
    }
    // src and dst are the same rect throughout - see copy_region.
    const SDL_Rect r = { pos.x, pos.y, width, height };
    if( opaque.w <= 0 || opaque.h <= 0 || SDL_HasIntersection( &r, &opaque ) == SDL_FALSE ) {
        copy_region( r );
        return;
    }
    // r minus opaque, as up to four bands: strips above and below, then left and
    // right of the row it sits in. Split rather than skipped because r is a whole
    // terminal line - skipping would blank the backdrop clear across the screen.
    const int r_bottom = r.y + r.h;
    const int r_right = r.x + r.w;
    const int o_bottom = opaque.y + opaque.h;
    const int o_right = opaque.x + opaque.w;
    if( opaque.y > r.y ) {
        copy_region( { r.x, r.y, r.w, opaque.y - r.y } );
    }
    if( o_bottom < r_bottom ) {
        copy_region( { r.x, o_bottom, r.w, r_bottom - o_bottom } );
    }
    const int band_top = std::max( r.y, opaque.y );
    const int band_h = std::min( r_bottom, o_bottom ) - band_top;
    if( opaque.x > r.x ) {
        copy_region( { r.x, band_top, opaque.x - r.x, band_h } );
    }
    if( o_right < r_right ) {
        copy_region( { o_right, band_top, r_right - o_right, band_h } );
    }
}

void terminal_backdrop::notify_render_target_size( const int width, const int height )
{
    // No same-size early return: SetupRenderTarget() also runs on
    // SDL_RENDER_TARGETS_RESET, which destroys this target texture's contents
    // without changing its size.
    canvas_w = width;
    canvas_h = height;
    rebuild_canvas();
}

#endif // TILES
