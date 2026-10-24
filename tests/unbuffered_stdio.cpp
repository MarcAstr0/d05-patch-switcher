// Linked into every test executable (see d05_add_test). On Windows a crash loses Qt Test's
// buffered stdout, leaving a CI log with no trace of the failing test. Unbuffered streams
// keep everything printed up to the crash.
#include <cstdio>

namespace {
struct UnbufferedStdio {
    UnbufferedStdio()
    {
#ifdef _WIN32
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
#endif
    }
} unbufferedStdio;
}  // namespace
