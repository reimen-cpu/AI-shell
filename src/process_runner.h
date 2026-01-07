#ifndef PROCESS_RUNNER_H
#define PROCESS_RUNNER_H

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

  // Run a command and return the result
  // command: The command line string (e.g., "cmd /c dir")
  static Result run(const std::string &command);
};

#endif // PROCESS_RUNNER_H
