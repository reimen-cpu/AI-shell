#include "command_cache.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>

CommandCache::CommandCache(const std::string &filepath) : filepath(filepath) {}

std::string CommandCache::get_current_timestamp() {
  std::time_t now = std::time(nullptr);
  char buf[80];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
  return std::string(buf);
}

std::string CommandCache::compute_hash(const std::string &data) {
  std::hash<std::string> hasher;
  size_t hash = hasher(data);
  std::stringstream ss;
  ss << std::hex << hash;
  return ss.str();
}

// Normalize user request for better matching
std::string CommandCache::normalize_request(const std::string &request) {
  std::string normalized = request;

  // Convert to lowercase
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  // Remove extra whitespace
  std::string result;
  bool last_was_space = false;
  for (char c : normalized) {
    if (std::isspace(c)) {
      if (!last_was_space && !result.empty()) {
        result += ' ';
        last_was_space = true;
      }
    } else {
      result += c;
      last_was_space = false;
    }
  }

  // Trim trailing space
  if (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }

  return result;
}

// Similarity with Intent Boosting (First word emphasis)
double CommandCache::compute_similarity(const std::string &str1,
                                        const std::string &str2) {
  std::string norm1 = normalize_request(str1);
  std::string norm2 = normalize_request(str2);

  if (norm1 == norm2)
    return 1.0;

  // Split into words
  std::vector<std::string> words1, words2;
  std::istringstream iss1(norm1), iss2(norm2);
  std::string word;
  while (iss1 >> word)
    words1.push_back(word);
  while (iss2 >> word)
    words2.push_back(word);

  if (words1.empty() || words2.empty())
    return 0.0;

  // Intent Bonus: If first words match (e.g., "abre" == "abre")
  double score = 0.0;
  bool intent_match = (words1[0] == words2[0]);

  std::set<std::string> set1(words1.begin(), words1.end());
  std::set<std::string> set2(words2.begin(), words2.end());

  int common = 0;
  for (const auto &w : set1) {
    if (set2.count(w))
      common++;
  }

  // Jaccard Base
  int total = set1.size() + set2.size() - common;
  double jaccard = total > 0 ? (double)common / total : 0.0;

  // Boost if intent matches
  if (intent_match) {
    score = 0.4 + (0.6 * jaccard);
  } else {
    score = jaccard;
  }

  return score;
}

// Helper to escape JSON strings
static std::string escape_json(const std::string &s) {
  std::string res;
  for (char c : s) {
    if (c == '"')
      res += "\\\"";
    else if (c == '\\')
      res += "\\\\";
    else if (c == '\n')
      res += "\\n";
    else if (c == '\r')
      res += "\\r";
    else if (c == '\t')
      res += "\\t";
    else
      res += c;
  }
  return res;
}

// Extract JSON field value
static std::string extract_field(const std::string &line,
                                 const std::string &key) {
  std::string pattern = "\"" + key + "\":\"";
  size_t pos = line.find(pattern);
  if (pos == std::string::npos) {
    // Try with space after colon
    pattern = "\"" + key + "\": \"";
    pos = line.find(pattern);
    if (pos == std::string::npos)
      return "";
  }

  size_t start = pos + pattern.length();
  std::string result;

  for (size_t i = start; i < line.length(); ++i) {
    if (line[i] == '\\' && i + 1 < line.length()) {
      char next = line[i + 1];
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
        result += next;
        i++;
      }
    } else if (line[i] == '"') {
      break;
    } else {
      result += line[i];
    }
  }

  return result;
}

// Extract integer field
static int extract_int_field(const std::string &line, const std::string &key) {
  std::string pattern = "\"" + key + "\":";
  size_t pos = line.find(pattern);
  if (pos == std::string::npos) {
    pattern = "\"" + key + "\": ";
    pos = line.find(pattern);
    if (pos == std::string::npos)
      return 0;
  }

  size_t start = pos + pattern.length();
  std::string num_str;

  for (size_t i = start; i < line.length(); ++i) {
    if (std::isdigit(line[i]) || line[i] == '-') {
      num_str += line[i];
    } else if (line[i] == ',' || line[i] == '}') {
      break;
    }
  }

  try {
    return std::stoi(num_str);
  } catch (...) {
    return 0;
  }
}

// JSONL format: one JSON object per line
std::vector<CachedCommand> CommandCache::load_all_entries() {
  std::vector<CachedCommand> entries;
  std::ifstream file(filepath);

  if (!file.is_open()) {
    return entries;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Skip empty lines
    if (line.empty() ||
        line.find_first_not_of(" \t\r\n") == std::string::npos) {
      continue;
    }

    CachedCommand cmd;
    cmd.user_request = extract_field(line, "user_request");
    cmd.command = extract_field(line, "command");
    cmd.timestamp = extract_field(line, "timestamp");
    cmd.context_hash = extract_field(line, "context_hash");
    cmd.last_error = extract_field(line, "last_error");
    cmd.usage_count = extract_int_field(line, "usage_count");
    cmd.success_count = extract_int_field(line, "success_count");
    cmd.failure_count = extract_int_field(line, "failure_count");
    cmd.is_reliable = cmd.success_count > cmd.failure_count;

    if (!cmd.user_request.empty() && !cmd.command.empty()) {
      entries.push_back(cmd);
    }
  }

  file.close();
  return entries;
}

// JSONL format: one JSON object per line (append-only friendly)
void CommandCache::save_all_entries(const std::vector<CachedCommand> &entries) {
  std::ofstream file(filepath, std::ios::trunc);

  if (!file.is_open()) {
    std::cerr << "[Cache] Failed to write to " << filepath << "\n";
    return;
  }

  for (const auto &cmd : entries) {
    file << "{";
    file << "\"user_request\":\"" << escape_json(cmd.user_request) << "\",";
    file << "\"command\":\"" << escape_json(cmd.command) << "\",";
    file << "\"timestamp\":\"" << escape_json(cmd.timestamp) << "\",";
    file << "\"context_hash\":\"" << escape_json(cmd.context_hash) << "\",";
    file << "\"last_error\":\"" << escape_json(cmd.last_error) << "\",";
    file << "\"usage_count\":" << cmd.usage_count << ",";
    file << "\"success_count\":" << cmd.success_count << ",";
    file << "\"failure_count\":" << cmd.failure_count;
    file << "}\n";
  }

  file.close();
}

std::string
CommandCache::find_cached_command(const std::string &user_request,
                                  const std::string &current_context) {
  std::vector<CachedCommand> entries = load_all_entries();
  std::string ctx_hash = compute_hash(current_context);

  double best_similarity = 0.0;
  std::string best_command = "";

  for (const auto &entry : entries) {
    // Must match context (OS + Shell)
    if (entry.context_hash != ctx_hash) {
      continue;
    }

    // Skip unreliable commands
    if (!entry.is_reliable && entry.failure_count > 0) {
      continue;
    }

    double similarity = compute_similarity(user_request, entry.user_request);

    // Require high similarity for cache hit (0.8 threshold)
    if (similarity >= 0.8 && similarity > best_similarity) {
      best_similarity = similarity;
      best_command = entry.command;
    }
  }

  return best_command;
}

void CommandCache::cache_command(const std::string &user_request,
                                 const std::string &command,
                                 const std::string &current_context) {
  std::vector<CachedCommand> entries = load_all_entries();
  std::string ctx_hash = compute_hash(current_context);
  std::string norm_request = normalize_request(user_request);

  // Check if already exists
  bool found = false;
  for (auto &entry : entries) {
    if (normalize_request(entry.user_request) == norm_request &&
        entry.context_hash == ctx_hash && entry.command == command) {
      // Update existing entry
      entry.timestamp = get_current_timestamp();
      entry.usage_count++;
      entry.success_count++;
      entry.is_reliable = entry.success_count > entry.failure_count;
      entry.last_error = ""; // Clear error on success
      found = true;
      break;
    }
  }

  if (!found) {
    // Add new entry
    CachedCommand new_cmd;
    new_cmd.user_request = user_request;
    new_cmd.command = command;
    new_cmd.timestamp = get_current_timestamp();
    new_cmd.context_hash = ctx_hash;
    new_cmd.usage_count = 1;
    new_cmd.success_count = 1;
    new_cmd.failure_count = 0;
    new_cmd.is_reliable = true;
    new_cmd.last_error = "";
    entries.push_back(new_cmd);
  }

  save_all_entries(entries);
}

void CommandCache::mark_command_failed(const std::string &user_request,
                                       const std::string &command,
                                       const std::string &error_msg,
                                       const std::string &current_context) {
  std::vector<CachedCommand> entries = load_all_entries();
  std::string ctx_hash = compute_hash(current_context);
  std::string norm_request = normalize_request(user_request);

  // Check if already exists
  bool found = false;
  for (auto &entry : entries) {
    if (normalize_request(entry.user_request) == norm_request &&
        entry.context_hash == ctx_hash && entry.command == command) {
      // Update existing entry
      entry.timestamp = get_current_timestamp();
      entry.usage_count++;
      entry.failure_count++;
      entry.is_reliable = entry.success_count > entry.failure_count;
      entry.last_error = error_msg;
      found = true;
      break;
    }
  }

  if (!found) {
    // Add new entry marked as failed
    CachedCommand new_cmd;
    new_cmd.user_request = user_request;
    new_cmd.command = command;
    new_cmd.timestamp = get_current_timestamp();
    new_cmd.context_hash = ctx_hash;
    new_cmd.usage_count = 1;
    new_cmd.success_count = 0;
    new_cmd.failure_count = 1;
    new_cmd.is_reliable = false;
    new_cmd.last_error = error_msg;
    entries.push_back(new_cmd);
  }

  save_all_entries(entries);
}

void CommandCache::increment_usage(const std::string &user_request) {
  std::vector<CachedCommand> entries = load_all_entries();
  std::string norm_request = normalize_request(user_request);

  for (auto &entry : entries) {
    if (normalize_request(entry.user_request) == norm_request) {
      entry.usage_count++;
      break;
    }
  }

  save_all_entries(entries);
}

std::string
CommandCache::get_similar_commands(const std::string &user_request) {
  std::vector<CachedCommand> entries = load_all_entries();

  // Find top 3 similar commands
  std::vector<std::pair<double, CachedCommand>> scored;

  for (const auto &entry : entries) {
    double similarity = compute_similarity(user_request, entry.user_request);
    if (similarity > 0.3) { // Minimum threshold
      scored.push_back({similarity, entry});
    }
  }

  // Sort by similarity (descending)
  std::sort(scored.begin(), scored.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  if (scored.empty()) {
    return "";
  }

  std::string context = "CACHED SUCCESSFUL COMMANDS (similar requests):\n";
  int count = 0;
  for (const auto &pair : scored) {
    if (count >= 3)
      break;
    context += "- Request: \"" + pair.second.user_request + "\"\n";
    context += "  Command: " + pair.second.command + "\n";
    context += "  Success: " + std::to_string(pair.second.success_count) +
               ", Failures: " + std::to_string(pair.second.failure_count) +
               "\n";
    count++;
  }

  return context;
}

std::string
CommandCache::get_reliable_commands(const std::string &user_request) {
  std::vector<CachedCommand> entries = load_all_entries();

  // Find top 3 similar RELIABLE commands
  std::vector<std::pair<double, CachedCommand>> scored;

  for (const auto &entry : entries) {
    // Only include reliable commands
    if (!entry.is_reliable) {
      continue;
    }

    double similarity = compute_similarity(user_request, entry.user_request);
    if (similarity > 0.3) { // Minimum threshold
      scored.push_back({similarity, entry});
    }
  }

  // Sort by similarity (descending)
  std::sort(scored.begin(), scored.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  if (scored.empty()) {
    return "";
  }

  std::string context = "RELIABLE CACHED COMMANDS (proven to work):\n";
  int count = 0;
  for (const auto &pair : scored) {
    if (count >= 3)
      break;
    context += "- Request: \"" + pair.second.user_request + "\"\n";
    context += "  Command: " + pair.second.command + "\n";
    context += "  Success rate: " + std::to_string(pair.second.success_count) +
               "/" + std::to_string(pair.second.usage_count) + "\n";
    count++;
  }

  return context;
}

void CommandCache::optimize() {
  std::vector<CachedCommand> entries = load_all_entries();

  // Remove unreliable commands with low usage
  std::vector<CachedCommand> filtered;

  for (const auto &entry : entries) {
    // Keep if reliable
    if (entry.is_reliable) {
      filtered.push_back(entry);
      continue;
    }

    // Keep if used frequently (even if unreliable, for learning)
    if (entry.usage_count > 3) {
      filtered.push_back(entry);
      continue;
    }

    // Otherwise discard
  }

  // Remove duplicates (same command for same request)
  std::vector<CachedCommand> unique;
  std::set<std::string> seen;

  for (const auto &entry : filtered) {
    std::string key = normalize_request(entry.user_request) + "|" +
                      entry.context_hash + "|" + entry.command;
    if (seen.find(key) == seen.end()) {
      seen.insert(key);
      unique.push_back(entry);
    }
  }

  save_all_entries(unique);
}
