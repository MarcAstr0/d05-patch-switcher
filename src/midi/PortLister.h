#pragma once

namespace d05 {

// Prints the MIDI output and input ports to stdout. Returns 0, or 1 if RtMidi
// throws (message on stderr). Used by --list-ports.
int listPorts();

} // namespace d05
