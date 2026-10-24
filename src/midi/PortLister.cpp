#include "midi/PortLister.h"

#include <RtMidi.h>

#include <cstdio>

namespace d05 {

int listPorts()
{
    try {
        RtMidiOut out;
        const unsigned n = out.getPortCount();
        std::printf("MIDI outputs: %u\n", n);
        for (unsigned i = 0; i < n; ++i)
            std::printf("  [%u] %s\n", i, out.getPortName(i).c_str());
        RtMidiIn in;
        const unsigned m = in.getPortCount();
        std::printf("MIDI inputs: %u\n", m);
        for (unsigned i = 0; i < m; ++i)
            std::printf("  [%u] %s\n", i, in.getPortName(i).c_str());
    } catch (const RtMidiError &e) {
        std::fprintf(stderr, "RtMidi error: %s\n", e.getMessage().c_str());
        return 1;
    }
    return 0;
}

} // namespace d05
