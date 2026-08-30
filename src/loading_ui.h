#pragma once
#ifndef CATA_SRC_LOADING_UI_H
#define CATA_SRC_LOADING_UI_H

#include <string>

namespace loading_ui
{
void show( const std::string &context, const std::string &step );
/**
 * Redraw the loading screen without changing what it says, for use inside a
 * loading step that takes long enough to be worth interrupting.
 *
 * The screen is only drawn when show() is called, which is once per step, so a
 * step lasting seconds leaves it frozen for seconds - long enough that an
 * animated splash stops animating and the whole screen looks hung. Calling this
 * periodically from within such a step gives it somewhere to redraw.
 *
 * Cheap to call and safe to call often: it rate-limits itself and does nothing
 * at all when no loading screen is up.
 *
 * Does nothing whatsoever in a curses build. The splash there is static text
 * with no frames to advance, so a redraw would buy a terminal repaint and change
 * nothing on screen. The declaration is deliberately shared rather than guarded,
 * so the call sites inside loading steps need no #ifdef of their own.
 */
void tick();
void done();
} // namespace loading_ui

#endif // CATA_SRC_LOADING_UI_H
