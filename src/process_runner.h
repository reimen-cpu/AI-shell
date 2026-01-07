#ifndef PROCESS_RUNNER_H
#define PROCESS_RUNNER_H

#include <functional>
#include <string>


// A robust process runner for Windows using Native API
// Captures default output and error streams
class ProcessRunner {
public:
  struct Result {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
  };

  // Callback type for streaming output: data, length, is_stderr
  using StreamCallback = std::function<void(const char *, size_t, bool)>;

  // Run a command and return the result
  // command: The command line string (e.g., "cmd /c dir")
  // callback: Optional function to receive output chunks in real-time
  static Result run(const std::string &command,
                    StreamCallback callback = nullptr);
};

#endif // PROCESS_RUNNER_H
