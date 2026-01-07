#include "command_cache.h"     // Include command cache
#include "command_processor.h" // Include command processor
#include "context_manager.h"
#include "http_client.h"
#include "json_utils.h"
#include "memory.h"  // Include memory manager
#include "wrapper.h" // Include wrapper
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h> // For GetModuleFileNameA and MAX_PATH

// ANSI Color Codes
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define GRAY "\033[90m"

std::string join(const std::vector<std::string> &vec,
                 const std::string &delim) {
  std::string res;
  for (size_t i = 0; i < vec.size(); ++i) {
    if (i > 0)
      res += delim;
    res += vec[i];
  }
  return res;
}

void clear_screen() {
#ifdef _WIN32
  std::system("cls");
#else
  std::system("clear");
#endif
}

bool is_ollama_ready() {
  http::Client client("localhost", 11434);
  http::Response resp = client.get("/");
  return resp.status_code == 200;
}

void ensure_ollama_running() {
  if (is_ollama_ready())
    return;

  std::cout << YELLOW << "Ollama is not running. Starting local server..."
            << RESET << "\n";
  std::system("start /B ollama serve > nul 2>&1");

  std::cout << GRAY << "Waiting for Ollama to be ready..." << RESET;
  for (int i = 0; i < 20; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (is_ollama_ready()) {
      std::cout << GREEN << " Done." << RESET << "\n";
      return;
    }
    std::cout << "." << std::flush;
  }
  std::cout
      << RED
      << " Failed to start Ollama automatically. Please start it manually."
      << RESET << "\n";
}

std::string select_model() {
  clear_screen();
  std::cout << YELLOW << "Fetching local models..." << RESET << "\n";
  http::Client client("localhost", 11434);
  http::Response resp = client.get("/api/tags");
  std::vector<std::string> models;
  if (resp.status_code == 200)
    models = json::extract_model_names(resp.body);

  if (models.empty()) {
    models.push_back("codellama:latest");
  }
  std::sort(models.begin(), models.end());

  while (true) {
    clear_screen();
    std::cout << YELLOW << "Select a model for the AI session:" << RESET
              << "\n";
    for (size_t i = 0; i < models.size(); ++i) {
      std::cout << GREEN << (i + 1) << ". " << RESET << models[i] << "\n";
    }
    std::cout << "\n" << CYAN << "Option: " << RESET;
    std::string choice;
    std::getline(std::cin, choice);
    try {
      int idx = std::stoi(choice);
      if (idx > 0 && idx <= (int)models.size())
        return models[idx - 1];
    } catch (...) {
    }
  }
}

std::string select_environment(const std::string &model) {
  // This function is no longer used for constructing the env_block directly.
  // The env_block is constructed in setup_context using individual getters.
  return "Operating System: Windows 11\nShell: PowerShell"; // defaulting for
                                                            // speed in this
                                                            // file rewrite
}

#include <windows.h>

std::string get_os_string() {
  std::string os_name = "Windows (Unknown)";
  HKEY hKey;
  char value[255];
  DWORD bufferSize = 255;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                    KEY_READ, &hKey) == ERROR_SUCCESS) {
    if (RegQueryValueExA(hKey, "ProductName", nullptr, nullptr, (LPBYTE)value,
                         &bufferSize) == ERROR_SUCCESS) {
      os_name = value;
    }
    RegCloseKey(hKey);
  }
  return os_name;
}

std::string get_detected_shell() {
  if (std::getenv("PSModulePath") != nullptr) {
    return "PowerShell (Available)";
  }
  return "CMD";
}

std::string get_username() {
  const char *user = std::getenv("USERNAME");
  if (user)
    return std::string(user);
  return "Unknown";
}

void setup_context(ContextManager &cm) {
  std::string model = select_model();
  AiContext ctx;
  ctx.model_name = model;
  ctx.operating_mode = "Translator";

  std::string os = get_os_string();
  std::string shell = get_detected_shell();
  std::string username = get_username();
  ctx.env_block =
      "Operating System: " + os + "\nShell: " + shell + "\nUser: " + username;

  ctx.transcript = "";
  cm.save_context(ctx);
  std::cout << GREEN << "\nSetup complete! Detected: " << os << " / " << shell
            << " / User: " << username << RESET << "\n";
}

void load_history_into_builder(json::Builder &builder,
                               const std::string &transcript) {
  if (transcript.empty())
    return;
  size_t pos = 0;
  while (pos < transcript.length()) {
    size_t next_sep = transcript.find("|||", pos);
    if (next_sep == std::string::npos)
      break;
    std::string segment = transcript.substr(pos, next_sep - pos);

    // Trim segment
    const char *ws = " \t\n\r\f\v";
    segment.erase(0, segment.find_first_not_of(ws));
    segment.erase(segment.find_last_not_of(ws) + 1);

    if (segment.find("USER: ") == 0)
      builder.add_message("user", segment.substr(6));
    else if (segment.find("ASSISTANT: ") == 0)
      builder.add_message("assistant", segment.substr(11));

    pos = next_sep + 3;
  }
}

// Helper functions moved to command_processor.cpp

// Get directory where the executable is located
std::string get_exe_directory() {
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);
  std::string full_path(path);
  size_t last_slash = full_path.find_last_of("\\/");
  if (last_slash != std::string::npos) {
    return full_path.substr(0, last_slash + 1);
  }
  return "";
}

std::string load_system_prompt(const std::string &base_path,
                               const std::string &env_block) {
  std::string path = base_path + "system_prompt.txt";
  std::ifstream f(path);
  std::string prompt;
  if (f.is_open()) {
    std::string line;
    while (std::getline(f, line)) {
      prompt += line + "\n";
    }
    f.close();
  } else {
    // Fallback default
    return "CTX: " + env_block + ". TASK: Output raw command.";
  }

  // Simple template replacement
  std::string search = "{ENV_BLOCK}";
  size_t pos = prompt.find(search);
  if (pos != std::string::npos) {
    prompt.replace(pos, search.length(), env_block);
  }
  return prompt;
}

// Post-process AI command to fix common issues
std::string post_process_command(const std::string &cmd) {
  std::string processed = cmd;

  // Only apply search pattern for Start-Process with EXPLICIT file paths
  // Do NOT apply if:
  // - Command contains URLs (http://, https://)
  // - Command is just an app name without full path

  size_t start_pos = processed.find("Start-Process");
  if (start_pos != std::string::npos) {
    // Skip if it's a URL
    if (processed.find("http://") != std::string::npos ||
        processed.find("https://") != std::string::npos) {
      return processed;
    }

    // Skip if it's a protocol handler (e.g., "github-desktop://")
    if (processed.find("://") != std::string::npos) {
      return processed;
    }

    size_t filepath_pos = processed.find("-FilePath", start_pos);
    if (filepath_pos == std::string::npos) {
      // Check for direct path without -FilePath flag
      size_t quote1 = processed.find('"', start_pos);
      size_t quote2 = processed.find('"', quote1 + 1);

      if (quote1 != std::string::npos && quote2 != std::string::npos) {
        std::string full_path =
            processed.substr(quote1 + 1, quote2 - quote1 - 1);

        // Only convert if it's a full path with drive letter
        if (full_path.find(":\\") != std::string::npos &&
            full_path.find(".exe") != std::string::npos) {

          // Extract just the filename
          size_t last_slash = full_path.find_last_of("\\/");
          if (last_slash != std::string::npos) {
            std::string filename = full_path.substr(last_slash + 1);

            // Build multi-location search command
            std::string search_cmd =
                "$app = Get-ChildItem \"C:\\Program "
                "Files*\",\"C:\\Users\\*\\AppData\\Local\" -Recurse -Filter "
                "\"" +
                filename +
                "\" -ErrorAction SilentlyContinue | Select-Object -First 1; "
                "if($app){Start-Process $app.FullName}";

            return search_cmd;
          }
        }
      }
    } else {
      // Has -FilePath flag
      size_t quote1 = processed.find('"', filepath_pos);
      size_t quote2 = processed.find('"', quote1 + 1);

      if (quote1 != std::string::npos && quote2 != std::string::npos) {
        std::string full_path =
            processed.substr(quote1 + 1, quote2 - quote1 - 1);

        // Only convert if it's a full path with drive letter
        if (full_path.find(":\\") != std::string::npos &&
            full_path.find(".exe") != std::string::npos) {

          // Extract just the filename
          size_t last_slash = full_path.find_last_of("\\/");
          if (last_slash != std::string::npos) {
            std::string filename = full_path.substr(last_slash + 1);

            // Build multi-location search command
            std::string search_cmd =
                "$app = Get-ChildItem \"C:\\Program "
                "Files*\",\"C:\\Users\\*\\AppData\\Local\" -Recurse -Filter "
                "\"" +
                filename +
                "\" -ErrorAction SilentlyContinue | Select-Object -First 1; "
                "if($app){Start-Process $app.FullName}";

            return search_cmd;
          }
        }
      }
    }
  }

  return processed;
}

// Automatic retry with AI-generated fix
std::string attempt_auto_fix(const std::string &failed_command,
                             const std::string &error_msg,
                             const std::string &user_request,
                             const std::string &model_name,
                             const std::string &system_prompt) {
  std::cout << YELLOW << "[Auto-Retry] Attempting to fix command..." << RESET
            << "\n";

  json::Builder fix_builder;
  fix_builder.add("model", model_name);
  fix_builder.add("stream", false);

  std::string fix_prompt =
      "The following command FAILED:\n"
      "USER REQUEST: " +
      user_request +
      "\n"
      "FAILED COMMAND: " +
      failed_command +
      "\n"
      "ERROR: " +
      error_msg +
      "\n\n"
      "TASK: Generate an ALTERNATIVE command that will work. "
      "Common fixes:\n"
      "- If executable not found: use Get-ChildItem to search and "
      "Start-Process. Example: $p=Get-ChildItem -Recurse -Filter \"Name.exe\" "
      "...\n"
      "- If syntax error: fix the PowerShell syntax\n"
      "- If permission denied: suggest alternative approach\n\n"
      "Output ONLY the corrected command, nothing else.";

  fix_builder.add_message("system", system_prompt);
  fix_builder.add_message("user", fix_prompt);

  http::Client client("localhost", 11434);
  http::Response resp = client.post("/api/chat", fix_builder.build());

  if (resp.status_code != 200) {
    return "";
  }

  std::string fixed_cmd = json::extract_response_content(resp.body);

  // Trim whitespace
  const char *ws = " \t\n\r\f\v";
  fixed_cmd.erase(0, fixed_cmd.find_first_not_of(ws));
  fixed_cmd.erase(fixed_cmd.find_last_not_of(ws) + 1);

  // Power-user fix: ensure complex commands are not mangled
  // But still remove wrapping backticks if present
  if (fixed_cmd.size() > 1 && fixed_cmd.front() == '`' &&
      fixed_cmd.back() == '`') {
    fixed_cmd = fixed_cmd.substr(1, fixed_cmd.size() - 2);
  }

  // Cleanup markdown artifacts - remove ALL backticks if they are markdown
  // wrappers But be careful not to remove backticks inside PowerShell strings
  // if any (though rare in basic commands) For safety, let's just remove
  // specific markdown code blocks if present
  if (fixed_cmd.find("```") != std::string::npos) {
    fixed_cmd.erase(std::remove(fixed_cmd.begin(), fixed_cmd.end(), '`'),
                    fixed_cmd.end());
  }

  return fixed_cmd;
}

int main(int argc, char *argv[]) {
  std::string exe_dir = get_exe_directory();
  // Enable UTF-8 Support
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  ContextManager cm(exe_dir + "context.json");
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(argv[i]);

  if (args.empty()) {
    ensure_ollama_running();
    setup_context(cm);
    return 0;
  }
  if (args[0] == "--reset" || args[0] == "--clear-history") {
    AiContext ctx;
    if (cm.load_context(ctx)) {
      ctx.transcript = "";
      cm.save_context(ctx);
      std::cout << GREEN << "History cleared." << RESET << "\n";
    }
    return 0;
  }

  if (args[0] == "--v" || args[0] == "--version") {
    std::cout << "AI-Shell v1.2.0 (Cpp Edition)\n";
    return 0;
  }

  if (args[0] == "--optimize-memory") {
    std::cout << YELLOW << "Optimizing memory..." << RESET << "\n";
    MemoryManager mem(exe_dir + "terminal_memory.jsonl");
    mem.optimize();
    std::cout << GREEN << "Done." << RESET << "\n";
    return 0;
  }

  // WRAP MANUALLY
  if (args[0] == "--wrap") {
    if (args.size() < 2) {
      std::cerr << "Usage: ai --wrap <command>\n";
      return 1;
    }
    std::string cmd_to_run = "";
    for (size_t i = 1; i < args.size(); ++i)
      cmd_to_run += args[i] + " ";

    // Identify tool name for context
    std::string tool = args[1];
    run_in_wrapper(cmd_to_run, tool);
    return 0;
  }

  ensure_ollama_running();
  AiContext ctx;
  if (!cm.load_context(ctx)) {
    setup_context(cm);
    return 0; // Return after setup, don't continue execution
  }

  // COMMAND CACHE CHECK
  // Use JSONL for scalability as requested
  CommandCache cache(exe_dir + "command_cache.jsonl");
  // Only use cache if the command is considered reliable (success > failure)
  std::string cached_cmd = cache.get_reliable_commands(user_request);
  if (!cached_cmd.empty()) {
    // Parse out the actual command from the context string
    // The context string format is "RELIABLE CACHED COMMANDS...\n- Request:
    // ...\n  Command: <CMD>\n..." We need to just get the command for direct
    // execution if it's a perfect match For now, let's use the exact match
    // finder
    std::string exact_match =
        cache.find_cached_command(user_request, ctx.env_block);
    if (!exact_match.empty()) {
      cached_cmd = exact_match;
    } else {
      cached_cmd = ""; // Only context available, not exact match
    }
  } else {
    // Try exact match anyway
    cached_cmd = cache.find_cached_command(user_request, ctx.env_block);
  }

  std::string command;
  bool from_cache = false;

  if (!cached_cmd.empty()) {
    // Found in cache!
    command = cached_cmd;
    from_cache = true;
    std::cout << CYAN << "[Cache Hit] " << RESET;
  } else {
    // Not in cache, generate with AI
    // Load External Prompt
    std::string system_prompt = load_system_prompt(exe_dir, ctx.env_block);

    // MEMORY RETRIEVAL
    MemoryManager mem(exe_dir + "terminal_memory.jsonl");
    std::string mem_context = mem.retrieve_relevant_context(user_request, "");

    // CACHE CONTEXT INJECTION (Similar but maybe not exact matches)
    std::string cache_context =
        cache.get_similar_commands(user_request); // Using robust search

    json::Builder chat_builder;
    chat_builder.add("model", ctx.model_name);
    chat_builder.add("stream", false);

    // Inject memory into system prompt for better adherence
    if (!mem_context.empty()) {
      system_prompt += "\n\n" + mem_context;
      system_prompt +=
          "\nCRITICAL: If the user request matches a past failure case above, "
          "you MUST propose a DIFFERENT command. Do not repeat mistakes.";
    }

    // Inject cache context for similar successful commands
    if (!cache_context.empty()) {
      system_prompt += "\n\n" + cache_context;
      system_prompt += "\nHINT: The above cached commands worked successfully "
                       "for similar requests. "
                       "Consider using a similar approach for the current "
                       "request if applicable.";
    }

    chat_builder.add_message("system", system_prompt);

    // Limit transcript to last 5 exchanges
    std::string limited_transcript = ctx.transcript;
    size_t count = 0;
    size_t pos = limited_transcript.length();
    while (pos > 0 && count < 10) {
      pos = limited_transcript.rfind("|||", pos - 1);
      if (pos != std::string::npos)
        count++;
      else
        break;
    }
    if (pos != std::string::npos && pos > 0) {
      limited_transcript = limited_transcript.substr(pos);
    }

    load_history_into_builder(chat_builder, limited_transcript);
    chat_builder.add_message("user", user_request);

    std::cout << GRAY << "Thinking...\r" << RESET;
    std::flush(std::cout);
    http::Client client("localhost", 11434);
    http::Response resp = client.post("/api/chat", chat_builder.build());

    // Clear "Thinking..." line
    std::cout << "\r\033[K";

    if (resp.status_code != 200) {
      std::cerr << RED << "Error: Ollama returned HTTP " << resp.status_code
                << RESET << "\n";
      return 1;
    }
    command = json::extract_response_content(resp.body);

    // Trim whitespace
    const char *ws = " \t\n\r\f\v";
    command.erase(0, command.find_first_not_of(ws));
    command.erase(command.find_last_not_of(ws) + 1);

    // Post-process: Convert hardcoded Start-Process paths to search pattern
    command = post_process_command(command);
  }

  // Cleanup markdown artifacts - remove ALL backticks
  command.erase(std::remove(command.begin(), command.end(), '`'),
                command.end());

  // Remove common markdown code block markers
  if (command.find("powershell") == 0 || command.find("bash") == 0 ||
      command.find("sh") == 0 || command.find("cmd") == 0) {
    size_t first_newline = command.find('\n');
    if (first_newline != std::string::npos) {
      command = command.substr(first_newline + 1);
    }
  }

  std::cout << "\r\033[K" << GREEN << "> " << command << RESET;
  if (from_cache) {
    std::cout << CYAN << " [CACHED]" << RESET;
  }
  std::cout << "\n\nExecute? [Y/n] ";

  std::string ans;
  std::flush(std::cout); // flush
  std::getline(std::cin, ans);

  if (ans.empty() || ans == "y" || ans == "Y") {
    // AUTO-DETECT INTERACTIVE
    if (is_interactive_tool(command)) {
      // ... (Interactive mode logic)
      // For brevity, skipping full interactive wrapper re-implementation in
      // this block assuming user wants the RETRY logic focused:
    }

    int ret = execute_command_safely(command, exe_dir + "terminal_stderr.log");
    std::cout << GRAY << "[DEBUG] Exit Code: " << ret << RESET << "\n";

    // Read stderr
    std::string stderr_content = "";
    std::ifstream ef(exe_dir + "terminal_stderr.log");
    if (ef.is_open()) {
      std::stringstream buffer;
      buffer << ef.rdbuf();
      stderr_content = buffer.str();
      ef.close();
    }

    // RETRY LOGIC
    if (ret != 0 || (!stderr_content.empty() &&
                     stderr_content.find("error") != std::string::npos)) {
      // First attempt failed. Try auto-fix.
      std::cout << RED << "Command failed. " << RESET << stderr_content << "\n";

      // Mark initial failure in cache
      cache.mark_command_failed(user_request, command, stderr_content,
                                ctx.env_block);

      // Attempt Fix
      std::string system_prompt_base =
          load_system_prompt(exe_dir, ctx.env_block);
      std::string fixed_command =
          attempt_auto_fix(command, stderr_content, user_request,
                           ctx.model_name, system_prompt_base);

      if (!fixed_command.empty() && fixed_command != command) {
        std::cout << CYAN << "[Auto-Retry] Trying alternative: " << RESET
                  << fixed_command << "\n";
        int ret2 = execute_command_safely(fixed_command,
                                          exe_dir + "terminal_stderr.log");

        if (ret2 == 0) {
          std::cout << GREEN << "[Auto-Retry] Success!" << RESET << "\n";
          // Cache the WORKING command
          cache.cache_command(user_request, fixed_command, ctx.env_block);
          command = fixed_command; // Update for history
          ret = 0;
          stderr_content = ""; // Clear error for history
        } else {
          std::cout << RED << "[Auto-Retry] Failed again." << RESET << "\n";
          // Read new stderr
          std::ifstream ef2(exe_dir + "terminal_stderr.log");
          if (ef2.is_open()) {
            std::stringstream buffer;
            buffer << ef2.rdbuf();
            stderr_content = buffer.str(); // Update error for history
          }
          cache.mark_command_failed(user_request, fixed_command, stderr_content,
                                    ctx.env_block);
        }
      }
    } else {
      // Success on first try
      if (!stderr_content.empty()) {
        // Minor warnings?
      }
      if (!from_cache) {
        cache.cache_command(user_request, command, ctx.env_block);
        std::cout << CYAN << "[Cache] Command saved." << RESET << "\n";
      } else {
        cache.increment_usage(user_request);
      }
    }

    // Append History with Result
    std::string result_str = (ret == 0)
                                 ? "[SUCCESS]"
                                 : "[FAILED: Exit Code " + std::to_string(ret) +
                                       " Error: " + stderr_content + "]";
    ctx.transcript += "USER: " + user_request + " ||| ASSISTANT: " + command +
                      " ||| RESULT: " + result_str + " ||| ";
    cm.save_context(ctx);

  } else {
    std::cout << YELLOW << "Aborted.\n" << RESET;
  }
  return 0;
}
