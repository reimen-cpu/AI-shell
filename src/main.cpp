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

  std::string user_request = join(args, " ");
  // Load External Prompt
  std::string system_prompt = load_system_prompt(exe_dir, ctx.env_block);

  // MEMORY RETRIEVAL
  MemoryManager mem(exe_dir + "terminal_memory.jsonl");
  // Try to find context based on the *last* failure or just similarity to
  // current request? User said: "Embedding of current command + error
  // detected" Since we don't have an error *yet* for the *current* request,
  // we only use memory if the user is asking about a previous failure (which
  // is in transcript) OR if the current request is similar to a past known
  // failure. For V1, let's inject any fixes relevant to the user request
  // keywords.
  std::string mem_context = mem.retrieve_relevant_context(user_request, "");

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

  chat_builder.add_message("system", system_prompt);

  // Limit transcript to last 5 exchanges (10 messages: user + assistant)
  // to prevent slow responses from bloated context
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
    if (!resp.body.empty()) {
      std::cerr << GRAY << "Body: " << resp.body.substr(0, 200) << RESET
                << "\n";
    }
    return 1;
  }
  std::string command = json::extract_response_content(resp.body);

  // Trim whitespace
  const char *ws = " \t\n\r\f\v";
  command.erase(0, command.find_first_not_of(ws));
  command.erase(command.find_last_not_of(ws) + 1);

  // Post-process: Convert hardcoded Start-Process paths to search pattern
  command = post_process_command(command);

  // Cleanup markdown artifacts
  if (command.find("```") != std::string::npos) {
    command.erase(std::remove(command.begin(), command.end(), '`'),
                  command.end());
  }

  std::cout << "\r\033[K" << GREEN << "> " << command << RESET << "\n";
  std::cout << "\nExecute? [Y/n] ";

  std::string ans;
  std::flush(std::cout); // flush
  std::getline(std::cin, ans);

  if (ans.empty() || ans == "y" || ans == "Y") {
    // AUTO-DETECT INTERACTIVE
    if (is_interactive_tool(command)) {
      std::cout
          << MAGENTA
          << "Interactive tool detected. Launch in AI Wrapper Mode? [Y/n] "
          << RESET;
      std::string wrap_ans;
      std::getline(std::cin, wrap_ans);
      if (wrap_ans.empty() || wrap_ans == "y" || wrap_ans == "Y") {
        // Extract tool name roughly (first word)
        std::string tool_name = command.substr(0, command.find(' '));

        ctx.transcript += "USER: " + user_request +
                          " ||| ASSISTANT: " + command +
                          " ||| RESULT: [WRAPPER MODE] ||| ";
        cm.save_context(ctx);

        run_in_wrapper(command, tool_name);
        return 0;
      }
    }

    // Use sanitized execution from module
    int ret = execute_command_safely(command, exe_dir + "terminal_stderr.log");
    std::cout << GRAY << "[DEBUG] Exit Code: " << ret << RESET << "\n";

    // Read stderr file always
    std::string stderr_content = "";
    std::ifstream ef(exe_dir + "terminal_stderr.log");
    if (ef.is_open()) {
      std::stringstream buffer;
      buffer << ef.rdbuf();
      stderr_content = buffer.str();
      ef.close();
    }

    // Only analyze if stderr has actual content
    // Many Windows GUI apps (explorer.exe, etc.) return non-zero exit codes
    // even when working
    if (!stderr_content.empty()) {
      // Synthesize stderr if exit code is non-zero but stderr is empty
      // (unusual case)
      if (ret != 0 && stderr_content.empty()) {
        stderr_content = "Command failed with Exit Code " +
                         std::to_string(ret) +
                         " but produced no stderr output.";
      }

      // Distill: Ask AI to analyze failure
      std::cout << YELLOW << "[Memory] Analyzing failure..." << RESET
                << std::endl;

      json::Builder distill_builder;
      distill_builder.add("model", ctx.model_name);
      distill_builder.add("stream", false);
      // Note: "format": "json" removed - causes HTTP 500 on some models

      std::string distill_prompt =
          "Analyze this terminal failure.\n"
          "COMMAND: " +
          command +
          "\n"
          "EXIT CODE: " +
          std::to_string(ret) +
          "\n"
          "STDERR: " +
          stderr_content +
          "\n"
          "\n"
          "TASK: Output a JSON object with: { \"status\": \"success\" or "
          "\"fail\", \"error_signature\": \"short unique error string\", "
          "\"summary\": \"causal explanation\", "
          "\"fix\": \"ACTIONABLE ALTERNATIVE COMMAND. If command missing, "
          "suggest substitute (e.g. 'Use PowerShell Select-String'). DO NOT "
          "suggest installing.\" }.\n"
          "NOTE: If Exit Code is 0 and STDERR contains only progress info or "
          "warnings, set status to \"success\".\n"
          "RULES: JSON ONLY. No markdown. Fix field MUST contain a tool "
          "name.";

      distill_builder.add_message("system", "You are a system analyzer.");
      distill_builder.add_message("user", distill_prompt);

      http::Client d_client("localhost", 11434);
      http::Response d_resp =
          d_client.post("/api/chat", distill_builder.build());

      std::cout << GRAY
                << "[DEBUG] Analysis HTTP Status: " << d_resp.status_code
                << RESET << std::endl;

      if (d_resp.status_code == 200) {
        // Extract values directly from d_resp.body (Ollama raw JSON)
        // Body format: {"message":{"content":"..."}, ...}

        // Helper lambda to extract a JSON string value from raw body
        auto extract_from_body = [&](const std::string &body,
                                     const std::string &key) -> std::string {
          std::string pattern = "\"" + key + "\":\"";
          size_t pos = body.find(pattern);
          if (pos == std::string::npos) {
            // Try alternate: "key": " (with space after colon)
            pattern = "\"" + key + "\": \"";
            pos = body.find(pattern);
            if (pos == std::string::npos)
              return "";
          }

          size_t start = pos + pattern.length();
          std::string result;

          // Walk through extracting chars, handling escapes
          for (size_t i = start; i < body.length(); ++i) {
            char c = body[i];
            if (c == '\\' && i + 1 < body.length()) {
              char next = body[i + 1];
              if (next == 'n') {
                result += '\n';
                i++;
              } else if (next == '"') {
                result += '"';
                i++;
              } else if (next == '\\') {
                result += '\\';
                i++;
              } else if (next == 't') {
                result += '\t';
                i++;
              } else if (next == 'r') {
                result += '\r';
                i++;
              } else {
                result += c;
              }
            } else if (c == '"') {
              break; // End of string
            } else {
              result += c;
            }
          }
          return result;
        };

        // First get the content field from Ollama response
        std::string content = extract_from_body(d_resp.body, "content");
        std::cout << GRAY << "[DEBUG] Extracted Content:\n"
                  << content << RESET << "\n";

        // Now extract the analysis fields from the content (which should be
        // JSON)
        std::string error_sig = extract_from_body(content, "error_signature");
        std::string summary = extract_from_body(content, "summary");
        std::string fix = extract_from_body(content, "fix");
        std::string status = extract_from_body(content, "status");

        std::cout << GRAY << "[DEBUG] error_signature=" << error_sig << RESET
                  << "\n";

        MemoryManager mem(exe_dir + "terminal_memory.jsonl");
        MemoryEntry entry;
        entry.user_request = user_request;
        entry.command = command;
        entry.exit_code = ret;
        entry.status = "fail";
        entry.error_signature = error_sig.empty() ? "Unknown Error" : error_sig;
        entry.summary = summary;
        entry.fix = fix;

        if (status == "success") {
          std::cout << GRAY << "[Memory] Analyzed stderr, determined harmless."
                    << RESET << "\n";
        } else {
          mem.log_execution(entry);
          std::cout << YELLOW << "[Memory] Learned from failure: "
                    << entry.error_signature << RESET << "\n";
        }
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
