#pragma once
#ifndef CATA_SRC_TERMINAL_BACKDROP_H
#define CATA_SRC_TERMINAL_BACKDROP_H

// Tiles only. Not stubbed for curses on purpose: a call from shared code should
// fail to compile rather than cost anything on a backend that can't use it.
#if defined(TILES)

#include <string>

#include "point.h"

/**
 * An image drawn where the terminal would otherwise be black.
 *
 * Not a layer behind the terminal - there is no such place, since curses windows
 * paint their lines black and ImGui renders after them. Instead sdltiles draws a
 * slice of this canvas over that black, and glyphs land on it normally. A cell
 * only paints its own background when that background is not black, so black
 * already means "leave alone".
 *
 * No blending anywhere: the canvas *is* the ground, which keeps glyph edges
 * compositing exactly as they do against black and avoids sRGB trouble.
 *
 * The canvas is render-target sized, so a fill is a 1:1 copy - no scaling, no
 * mapping. All fitting is decided once, when the canvas is built.
 */
namespace terminal_backdrop
{

/** How the source image is mapped onto a terminal that is a different shape. */
enum class fit {
    /**
     * Scale until the whole image fits, preserving aspect ratio; the leftover at
     * two edges stays black. Never crops. Exact factor, not snapped to a whole
     * one - snapping costs most of the screen at common resolutions.
     */
    contain,
    /** Scale to fill, preserving aspect ratio; the overflow is cropped. */
    cover,
    /** No scaling at all; centred, cropped to whatever fits. */
    native,
};

/**
 * Load `path` and make it the backdrop. Returns false, setting nothing, when the
 * file is missing or unreadable - not an error, just a black terminal.
 *
 * `dim_percent` is applied once when the canvas is built; 100 is as authored,
 * lower darkens the art so text over it stays legible.
 */
bool set( const std::string &path, fit how, int dim_percent );

/** Drop the backdrop and free its textures. Safe when none is set. */
void clear();

/**
 * Draw the loaded backdrop for as long as this object lives.
 *
 * Loading and drawing are separate because drawing is synchronous - wnoutrefresh()
 * calls curses_drawwindow() directly - so a merely *loaded* backdrop would show
 * behind every screen reachable from the menu. Nesting is counted.
 *
 * Wrap the owner's DRAWING, not the call that asks for a redraw: an unrelated
 * screen can invalidate the owner's region and repaint it as part of its own
 * redraw, which would then land outside the scope.
 */
class scoped_enable
{
    public:
        scoped_enable();
        ~scoped_enable();
        scoped_enable( const scoped_enable & ) = delete;
        scoped_enable &operator=( const scoped_enable & ) = delete;
};

/** True when a backdrop is loaded and a scoped_enable is in effect. */
bool active();

/**
 * True when a backdrop is loaded at all, whether or not it is drawing now.
 *
 * active() answers "is a picture on screen this instant"; this answers "did the
 * art load", which is what callers deciding how to lay the menu out need, since
 * they run outside any redraw. See PATH_INFO::title() and use_column_menu().
 */
bool loaded();

/**
 * A region, in terminal cells, left plain black instead of art - for popups that
 * must stay readable over whatever the backdrop is. One region; a new one
 * replaces it.
 *
 * Only recorded; fill() skips it. Touches no renderer state on purpose: these
 * run mid-redraw, between two windows drawing, and rebuilding the canvas there
 * would switch the render target and reset the viewport and clip rect.
 */
void set_opaque_cells( const point &pos, int width, int height );
void clear_opaque_cells();

/**
 * Draw the backdrop over this rect of the render target.
 *
 * **The caller clears to black first; this draws on top.** An uncovered part -
 * an opaque popup region - is then already the right colour, so there is no
 * second code path. Does nothing unless a backdrop is loaded and drawing, which
 * is what makes it safe on the per-line clear in draw_window().
 */
void fill( const point &pos, int width, int height );

/**
 * Render target size in pixels; sdltiles calls this whenever it creates one.
 * Rebuilds the canvas to match, so a resize re-fits the art.
 */
void notify_render_target_size( int width, int height );

} // namespace terminal_backdrop

#endif // TILES

#endif // CATA_SRC_TERMINAL_BACKDROP_H
