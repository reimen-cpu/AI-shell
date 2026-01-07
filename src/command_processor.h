#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <string>

// Checks if the command is for an interactive tool (sqlite, python, etc.)
bool is_interactive_tool(const std::string &cmd);

// Checks if the command looks like a PowerShell command
bool is_likely_powershell(const std::string &cmd);

// Wraps a command in powershell -c "..." handling quoting
std::string wrap_powershell(const std::string &cmd);

// Main sanitization function to clean up AI output
// Strips outer quotes, handles nested powershell wrapping, etc.
std::string sanitize_command(const std::string &raw_cmd);

// Helper to safely execute the command (wrapping if needed)
// Returns exit code
int execute_command_safely(
    const std::string &cmd,
    const std::string &stderr_path = "terminal_stderr.log");

#endif // COMMAND_PROCESSOR_H
