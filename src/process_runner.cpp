#include "process_runner.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>


ProcessRunner::Result ProcessRunner::run(const std::string &command,
                                         StreamCallback callback) {
  Result result;
  result.exit_code = -1;

  SECURITY_ATTRIBUTES sa_attr;
  sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa_attr.bInheritHandle = TRUE;
  sa_attr.lpSecurityDescriptor = NULL;

  HANDLE h_out_read = NULL;
  HANDLE h_out_write = NULL;
  HANDLE h_err_read = NULL;
  HANDLE h_err_write = NULL;

  // Create pipes for stdout
  if (!CreatePipe(&h_out_read, &h_out_write, &sa_attr, 0)) {
    result.stderr_output = "Failed to create stdout pipe.";
    return result;
  }
  SetHandleInformation(h_out_read, HANDLE_FLAG_INHERIT,
                       0); // Ensure read handle is NOT inherited

  // Create pipes for stderr
  if (!CreatePipe(&h_err_read, &h_err_write, &sa_attr, 0)) {
    CloseHandle(h_out_read);
    CloseHandle(h_out_write);
    result.stderr_output = "Failed to create stderr pipe.";
    return result;
  }
  SetHandleInformation(h_err_read, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.hStdError = h_err_write;
  si.hStdOutput = h_out_write;
  si.dwFlags |= STARTF_USESTDHANDLES;

  ZeroMemory(&pi, sizeof(pi));

  // CreateProcess modifies the command line, so we need a mutable buffer
  std::vector<char> cmd_buf(command.begin(), command.end());
  cmd_buf.push_back(0);

  if (!CreateProcessA(NULL, cmd_buf.data(), NULL, NULL,
                      TRUE, // Inherit handles
                      0, NULL, NULL, &si, &pi)) {

    CloseHandle(h_out_read);
    CloseHandle(h_out_write);
    CloseHandle(h_err_read);
    CloseHandle(h_err_write);
    result.stderr_output =
        "CreateProcess failed (" + std::to_string(GetLastError()) + ")";
    return result;
  }

  // Close write ends in this process
  CloseHandle(h_out_write);
  CloseHandle(h_err_write);

  // READ LOOP using PeekNamedPipe
  const int BUFSIZE = 4096;
  char buffer[BUFSIZE];
  bool process_running = true;

  while (process_running) {
    bool did_read = false;
    DWORD bytes_read = 0;
    DWORD bytes_avail = 0;

    // Check stdout
    if (PeekNamedPipe(h_out_read, NULL, 0, NULL, &bytes_avail, NULL) &&
        bytes_avail > 0) {
      if (ReadFile(h_out_read, buffer, BUFSIZE, &bytes_read, NULL) &&
          bytes_read > 0) {
        result.stdout_output.append(buffer, bytes_read);
        if (callback)
          callback(buffer, bytes_read, false);
        did_read = true;
      }
    }

    // Check stderr
    if (PeekNamedPipe(h_err_read, NULL, 0, NULL, &bytes_avail, NULL) &&
        bytes_avail > 0) {
      if (ReadFile(h_err_read, buffer, BUFSIZE, &bytes_read, NULL) &&
          bytes_read > 0) {
        result.stderr_output.append(buffer, bytes_read);
        if (callback)
          callback(buffer, bytes_read, true);
        did_read = true;
      }
    }

    // Check process status
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
      process_running = false;
    } else {
      if (exit_code != STILL_ACTIVE) {
        // Even if process exited, pipes might still have data
        // Run one last check loop until pipes empty
        // For simplicity, we just switch flag and let the loop run once more
        // for data But strictly, we should peek until 0.

        // Let's do a strict drain
        while (true) {
          bool drained_any = false;
          if (PeekNamedPipe(h_out_read, NULL, 0, NULL, &bytes_avail, NULL) &&
              bytes_avail > 0) {
            if (ReadFile(h_out_read, buffer, BUFSIZE, &bytes_read, NULL) &&
                bytes_read > 0) {
              result.stdout_output.append(buffer, bytes_read);
              if (callback)
                callback(buffer, bytes_read, false);
              drained_any = true;
            }
          }
          if (PeekNamedPipe(h_err_read, NULL, 0, NULL, &bytes_avail, NULL) &&
              bytes_avail > 0) {
            if (ReadFile(h_err_read, buffer, BUFSIZE, &bytes_read, NULL) &&
                bytes_read > 0) {
              result.stderr_output.append(buffer, bytes_read);
              if (callback)
                callback(buffer, bytes_read, true);
              drained_any = true;
            }
          }
          if (!drained_any)
            break;
        }

        result.exit_code = (int)exit_code;
        process_running = false;
      }
    }

    if (process_running && !did_read) {
      // Avoid CPU spin
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(h_out_read);
  CloseHandle(h_err_read);

  return result;
}
