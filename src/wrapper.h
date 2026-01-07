#ifndef WRAPPER_H
#define WRAPPER_H

#include <string>

// Simpler interaction:
// We just run the logical loop.
// Since implementing a transparent terminal proxy on Windows is complex
// (handling VT sequences, formatting, ConPTY), we will abstract it here. For
// now, we will implement a "Basic Input Interceptor". It might lose some
// colors/arrow keys if we aren't careful, but it's step 1.

void run_in_wrapper(const std::string &command_line,
                    const std::string &tool_name);

#endif
