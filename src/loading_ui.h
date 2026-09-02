#pragma once
#ifndef CATA_SRC_LOADING_UI_H
#define CATA_SRC_LOADING_UI_H

#include <string>

namespace loading_ui
{
void show( const std::string &context, const std::string &step );
/**
 * Redraws the loading screen during a loading step to allow animations to play in the background.
 * Works by having longer loading steps deliberately call it periodically.
 * Rate-limits itself and effectively does nothing in curses builds other than repaint the terminal.
 * Declaration is shared so call-sites inside loading steps dont need an #ifdef.
 */
void tick();
void done();
} // namespace loading_ui

#endif // CATA_SRC_LOADING_UI_H
