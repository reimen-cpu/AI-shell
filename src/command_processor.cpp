#include "command_processor.h"
#include "process_runner.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

bool is_interactive_tool(const std::string &cmd) {
  std::string lower = cmd;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (lower.find("sqlite3") != std::string::npos)
    return true;
  if (lower.find("mysql") != std::string::npos)
    return true;
  if (lower.find("python") != std::string::npos)
    return true;
  if (lower.find("ssh") != std::string::npos &&
      lower.find("@") != std::string::npos)
    return true;
  return false;
}

bool is_likely_powershell(const std::string &cmd) {
  if (cmd.find("Get-") != std::string::npos ||
      cmd.find("Sort-") != std::string::npos ||
      cmd.find("Select-") != std::string::npos ||
      cmd.find("Set-") != std::string::npos ||
      cmd.find("New-") != std::string::npos ||
      cmd.find("Test-") != std::string::npos ||
      cmd.find("$env:") != std::string::npos ||
      cmd.find("Where-Object") != std::string::npos ||
      cmd.find("Select-String") != std::string::npos ||
      cmd.find("Write-Output") != std::string::npos ||
      cmd.find("$(") != std::string::npos) {
    return true;
  }
  return false;
}

std::string wrap_powershell(const std::string &cmd) {
  std::string escaped;
  for (char c : cmd) {
    if (c == '"')
      escaped += "\\\"";
    else
      escaped += c;
  }
  return "powershell -NoProfile -ExecutionPolicy Bypass -Command \"" + escaped +
         "\"";
}

std::string sanitize_command(const std::string &raw_cmd) {
  std::string sanitized = raw_cmd;
  std::string lower_cmd = raw_cmd;
  std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(),
                 ::tolower);

  // 1. Strip potential "powershell -c" prefix added by AI
  if (lower_cmd.find("powershell") == 0 || lower_cmd.find("pwsh") == 0) {
    size_t first_quote = raw_cmd.find('"');
    size_t last_quote = raw_cmd.rfind('"');
    if (first_quote != std::string::npos && last_quote != std::string::npos &&
        last_quote > first_quote) {
      sanitized = raw_cmd.substr(first_quote + 1, last_quote - first_quote - 1);
    }
  }

  // 2. Recursive strip outer quotes (e.g. ""cmd"")
  while (sanitized.size() >= 2 && sanitized.front() == '"' &&
         sanitized.back() == '"') {
    sanitized = sanitized.substr(1, sanitized.size() - 2);
  }

  // 3. Trim whitespace
  const char *ws = " \t\n\r\f\v";
  sanitized.erase(0, sanitized.find_first_not_of(ws));
  sanitized.erase(sanitized.find_last_not_of(ws) + 1);

  // 4. Echo guard: If command starts with "echo " and is long, it's likely an
  // explanation. The user explicitly asked to "not interfere". If it's just
  // "echo ...", technically it is a command. But if it contains quotes/pipes
  // that break shell, it's bad. Let's rely on the prompt fix primarily, but add
  // a safety check here.
  return sanitized;
}

int execute_command_safely(const std::string &cmd,
                           const std::string &stderr_path) {
  std::string sanitized = sanitize_command(cmd);

  // Filter "echo" explanations that look like failure messages
  if (sanitized.find("echo ") == 0 || sanitized.find("Write-Output ") == 0) {
    if ((sanitized.length() > 60 && sanitized.find(">") == std::string::npos) ||
        sanitized.find("Comando no") != std::string::npos ||
        sanitized.find("failed") != std::string::npos) {
      std::cerr << "\033[33m[AI Explanation suppressed]\033[0m\n";
      return 0;
    }
  }

  std::string final_cmd;
  if (is_likely_powershell(sanitized)) {
    final_cmd = wrap_powershell(sanitized);
  } else {
    // For ProcessRunner via CreateProcess, we usually need "cmd /c" if it's a
    // shell builtin or just the command if it's an exe. Since we want to
    // support everything safely: If it's NOT powershell, we assume cmd.exe for
    // broad compatibility (dir, type, echo, etc.)
    final_cmd = "cmd /c " + sanitized;
  }

  // Use ProcessRunner
  ProcessRunner::Result result = ProcessRunner::run(final_cmd);

  // Output Stdout live (well, buffered in this simple runner)
  std::cout << result.stdout_output << std::flush;

  // Write Stderr to file for Main's analysis (keep compatibility with main.cpp
  // logic)
  if (!result.stderr_output.empty()) {
    std::cerr << result.stderr_output << std::flush;
    // Helper: Write to file so main.cpp can read it (main expects file)
    // Ideally main should take the string directly, but adhering to minimal
    // refactor for now We can update main later or just write it here.
    FILE *f = fopen(stderr_path.c_str(), "w");
    if (f) {
      fwrite(result.stderr_output.data(), 1, result.stderr_output.size(), f);
      fclose(f);
    }
  } else {
    // Clear file
    FILE *f = fopen(stderr_path.c_str(), "w");
    if (f)
      fclose(f);
  }

  // Improved clip piping: Escape quotes for CMD echo
  std::string clip_payload = sanitized;
  std::string escaped_for_echo;
  for (char c : clip_payload) {
    if (c == '"')
      escaped_for_echo += "\\\"";
    else
      escaped_for_echo += c;
  }

  // Clip still uses system for simplicity, or we could use ProcessRunner too
  std::system(("echo \"" + escaped_for_echo + "\" | clip").c_str());

  return result.exit_code;
}
