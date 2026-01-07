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

#include <set>

// Helper to extract value from JSON line by key
std::string extract_json_field(const std::string &line,
                               const std::string &key) {
  std::string pattern = "\"" + key + "\":\"";
  size_t pos = line.find(pattern);
  if (pos == std::string::npos)
    return "";
  size_t start = pos + pattern.length();
  std::string res;
  for (size_t i = start; i < line.length(); ++i) {
    if (line[i] == '"' && line[i - 1] != '\\')
      break;
    if (line[i] == '\\' && i + 1 < line.length()) {
      char next = line[i + 1];
      if (next == '"')
        res += '"';
      else if (next == '\\')
        res += '\\';
      else
        res += next; // simplistic unescape for others
      i++;
    } else {
      res += line[i];
    }
  }
  return res;
}

std::string
MemoryManager::retrieve_relevant_context(const std::string &current_cmd,
                                         const std::string &current_error) {
  std::ifstream file(filepath);
  if (!file.is_open())
    return "";

  std::vector<std::string> relevant_lines;
  std::string line;

  // Collect all lines first (to search in reverse if needed, or just scan all)
  // For V1 simple scan is fine.
  while (std::getline(file, line)) {
    if (current_cmd.empty())
      continue;
    // Check if line contains user request keywords (simplistic)
    if (line.find(current_cmd) != std::string::npos) {
      relevant_lines.push_back(line);
    }
  }

  if (relevant_lines.empty())
    return "";

  std::string context_block = "PREVIOUS MISTAKES & FIXES:\n";
  std::set<std::string> seen_fixes;
  int count = 0;

  // Iterate in reverse to get most recent relevant failures first
  for (auto it = relevant_lines.rbegin(); it != relevant_lines.rend(); ++it) {
    std::string cmd = extract_json_field(*it, "cmd");
    std::string fix = extract_json_field(*it, "fix");
    std::string err = extract_json_field(*it, "error_signature");

    // Skip if fix is empty or "Unknown" or just repeating
    if (fix.empty())
      continue;

    // Deduplicate based on the fix suggestion
    if (seen_fixes.find(fix) != seen_fixes.end())
      continue;
    seen_fixes.insert(fix);

    context_block += "- Failed: " + cmd + "\n  Fix: " + fix + "\n";
    count++;
    if (count >= 2)
      break; // Limit to 2 most recent distinct relevant items
  }

  if (count == 0)
    return "";
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
