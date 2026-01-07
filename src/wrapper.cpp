#include "wrapper.h"
#include "context_manager.h"
#include "http_client.h"
#include "json_utils.h"
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

// Global context/client reused or re-instantiated?
// We will call direct helpers for AI.

// Helper to ask AI
std::string query_ai_for_tool(const std::string &user_request,
                              const std::string &tool_name) {
  ContextManager cm("context.json");
  AiContext ctx;
  if (!cm.load_context(ctx))
    return "";

  std::string system_prompt =
      "ROLE: You are an expert for the tool '" + tool_name +
      "'. "
      "TASK: Translate the USER REQUEST into a valid command/query for this "
      "tool. "
      "CONTEXT: " +
      ctx.env_block +
      ". "
      "RULES: Return ONLY the code/query. No markdown. No explanation.";

  json::Builder b;
  b.add("model", ctx.model_name);
  b.add("stream", false);
  b.add_message("system", system_prompt);
  // TODO: Add history? Wrapper history is tricky.
  b.add_message("user", user_request);

  http::Client c("localhost", 11434);
  http::Response r = c.post("/api/chat", b.build());
  if (r.status_code != 200)
    return "";

  std::string cmd = json::extract_response_content(r.body);
  // Clean
  std::string cleanCmd;
  for (char c : cmd) {
    if (c != '`')
      cleanCmd += c;
  }
  return cleanCmd;
}

// Windows Pipe Handling
HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

void WriteToPipe(const std::string &data) {
  DWORD dwWritten;
  WriteFile(g_hChildStd_IN_Wr, data.c_str(), data.length(), &dwWritten, NULL);
}

void ReadFromPipe() {
  DWORD dwRead, dwWritten;
  CHAR chBuf[4096];
  HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

  while (true) {
    BOOL bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, 4096, &dwRead, NULL);
    if (!bSuccess || dwRead == 0)
      break;

    WriteFile(hParentStdOut, chBuf, dwRead, &dwWritten, NULL);
  }
}

void run_in_wrapper(const std::string &command_line,
                    const std::string &tool_name) {
  SECURITY_ATTRIBUTES saAttr;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  // Create a pipe for the child process's STDOUT.
  if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
    return;
  SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);

  // Create a pipe for the child process's STDIN.
  if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
    return;
  SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

  PROCESS_INFORMATION piProcInfo;
  STARTUPINFOA siStartInfo;
  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
  siStartInfo.cb = sizeof(STARTUPINFO);
  siStartInfo.hStdError = g_hChildStd_OUT_Wr;
  siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
  siStartInfo.hStdInput = g_hChildStd_IN_Rd;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  // Create child process
  std::string cmd = command_line;
  BOOL bSuccess =
      CreateProcessA(NULL, const_cast<char *>(cmd.c_str()), NULL, NULL, TRUE, 0,
                     NULL, NULL, &siStartInfo, &piProcInfo);

  if (!bSuccess)
    return;

  // Output thread
  std::thread outputThread(ReadFromPipe);
  outputThread.detach();

  // Input Loop (Interceptor)
  // We use std::getline which blocks. This means we might lose single-keystroke
  // interactivity (tabs/arrows) UNLESS we use low-level console read. For this
  // prototype, we accept "Line Mode" wrapping which is common for "Wrapper
  // Mode". Sqlite3 works fine with lines.

  std::cout
      << "[AI Wrapper Active. Type 'ai <request>' to generate commands.]\n";

  std::string line;
  while (std::getline(std::cin, line)) {
    // Intercept
    if (line.rfind("ai ", 0) == 0) {
      std::cout << "[Thinking...]\r";
      std::string req = line.substr(3);
      std::string generated = query_ai_for_tool(req, tool_name);

      // Show generated
      // Move cursor up?
      std::cout << "\r> " << generated << "     \n";
      std::cout << "Execute? [Y/n] ";
      std::string ans;
      std::getline(std::cin, ans); // Pause

      if (ans.empty() || ans == "y" || ans == "Y") {
        generated += "\n"; // Enter
        WriteToPipe(generated);
      }
    } else {
      line += "\n";
      WriteToPipe(line);
    }

    // Check if child dead?
    if (WaitForSingleObject(piProcInfo.hProcess, 0) == WAIT_OBJECT_0)
      break;
  }

  CloseHandle(g_hChildStd_OUT_Wr);
  CloseHandle(g_hChildStd_IN_Rd);
  CloseHandle(g_hChildStd_IN_Wr); // Close write end to signal EOF to child

  WaitForSingleObject(piProcInfo.hProcess, INFINITE);
  CloseHandle(piProcInfo.hProcess);
  CloseHandle(piProcInfo.hThread);
}
