#pragma once

#include "app/Cli.h"

namespace d05 {

// --screenshot FILE.png [--screenshot-state S] [--screenshot-size WxH] (SPEC FR-15): builds the window with
// the fake MIDI backend and a canned library (in a temporary config dir unless
// --config-dir is given), renders it offscreen-capable via QWidget::grab and writes
// the PNG. Needs a QApplication and Theme::install() done. Returns an Exit code:
// 0 written, 1 the file could not be written.
int runScreenshot(const CliOptions &opts);

}  // namespace d05
