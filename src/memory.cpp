#include "memory.h"
#include <ctime>
#include <fstream>
#include <functional> // for std::hash
#include <iomanip>
#include <iostream>
#include <sstream>

MemoryManager::MemoryManager(const std::string &filepath)
    : filepath(filepath) {}

std::string MemoryManager::get_current_timestamp() {
  std::time_t now = std::time(nullptr);
  char buf[80];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
  return std::string(buf);
}

std::string MemoryManager::compute_hash(const std::string &data) {
  std::hash<std::string> hasher;
  size_t hash = hasher(data);
  std::stringstream ss;
  ss << std::hex << hash;
  return ss.str();
}

void MemoryManager::log_execution(const MemoryEntry &entry) {
  // Escaping helper
  auto escape = [](const std::string &s) {
    std::string res;
    for (char c : s) {
      if (c == '"')
        res += "\\\"";
      else if (c == '\\')
        res += "\\\\";
      else if (c == '\n')
        res += "\\n";
      else
        res += c;
    }
    return res;
  };

  std::stringstream json_line;
  json_line << "{";
  json_line << "\"ts\":\""
            << (entry.timestamp.empty() ? get_current_timestamp()
                                        : entry.timestamp)
            << "\",";
  json_line << "\"user_req\":\"" << escape(entry.user_request) << "\",";
  json_line << "\"cmd\":\"" << escape(entry.command) << "\",";
  json_line << "\"cwd\":\"" << escape(entry.cwd) << "\",";
  json_line << "\"exit_code\":" << entry.exit_code << ",";
  json_line << "\"status\":\"" << escape(entry.status) << "\",";
  json_line << "\"error_signature\":\"" << escape(entry.error_signature)
            << "\",";
  json_line << "\"summary\":\"" << escape(entry.summary) << "\",";
  json_line << "\"fix\":\"" << escape(entry.fix) << "\"";
  json_line << "}\n";

  std::ofstream file(filepath, std::ios::app);
  if (file.is_open()) {
    file << json_line.str();
    file.close();
  } else {
    std::cerr << "[Memory] Failed to write to " << filepath << "\n";
  }
}

std::string
MemoryManager::retrieve_relevant_context(const std::string &current_cmd,
                                         const std::string &current_error) {
  std::ifstream file(filepath);
  if (!file.is_open())
    return "";

  std::string line;
  std::string context_block = "";
  int match_count = 0;
  std::vector<std::string> matches;

  // Simple fuzzy match for V1
  // If we have a user request (current_cmd acting as user request proxy in this
  // call from main), use it.

  while (std::getline(file, line)) {
    bool meaningful_match = false;

    // Check for user request overlap (simple substring)
    // In main.cpp we pass user_request as first arg
    if (!current_cmd.empty()) {
      // Very basic semantic match: check for words?
      // For now, let's just check if the stored user_req contains significant
      // parts of current_req or vice versa. Actually, main.cpp uses this arg
      // name "current_cmd" but passes "user_request". Let's rely on finding
      // standard keywords.

      // Match if line contains the user request (exact or substring) is too
      // strict Let's just do: Is the current request inside the stored request
      // or vice versa?
      if (line.find(current_cmd) != std::string::npos) {
        meaningful_match = true;
      }
    }

    // Also match error signature if provided
    if (!current_error.empty() &&
        line.find(current_error) != std::string::npos) {
      meaningful_match = true;
    }

    if (meaningful_match) {
      matches.push_back(line);
      match_count++;
      if (match_count >= 3)
        break;
    }
  }

  if (matches.empty())
    return "";

  context_block += "IMPORTANT WARNING: THE FOLLOWING COMMANDS FAILED "
                   "PREVIOUSLY. DO NOT REPEAT THEM:\n";
  for (const auto &m : matches) {
    context_block += m + "\n";
  }
  return context_block;
}

void MemoryManager::optimize() {
  std::ifstream file(filepath);
  if (!file.is_open())
    return;

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty())
      lines.push_back(line);
  }
  file.close();

  // Simple deduplication based on exact string match (preserving latest)
  // For V1, we just rewrite unique lines, kept in order.
  // If we want to clean specific errors or "irrelevant", we'd need more logic.
  // For now: Unique.

  std::vector<std::string> unique_lines;
  for (const auto &l : lines) {
    bool duplicate = false;
    for (const auto &u : unique_lines) {
      if (l == u) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate)
      unique_lines.push_back(l);
  }

  std::ofstream outfile(filepath, std::ios::trunc);
  for (const auto &l : unique_lines) {
    outfile << l << "\n";
  }
}
