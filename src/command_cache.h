#ifndef COMMAND_CACHE_H
#define COMMAND_CACHE_H

#include <string>
#include <vector>

struct CachedCommand {
  std::string user_request;
  std::string command;
  std::string timestamp;
  int usage_count;
  int success_count;        // Number of successful executions
  int failure_count;        // Number of failed executions
  std::string context_hash; // Hash of OS + Shell for environment matching
  std::string last_error;   // Last error message if failed
  bool is_reliable;         // true if success_count > failure_count
};

class CommandCache {
public:
  CommandCache(const std::string &filepath);

  // Find a reliable cached command for the given request and context
  // Returns empty string if no suitable command found
  std::string find_cached_command(const std::string &user_request,
                                  const std::string &current_context);

  // Store a successful command in the cache
  void cache_command(const std::string &user_request,
                     const std::string &command,
                     const std::string &current_context);

  // Mark a command as failed
  void mark_command_failed(const std::string &user_request,
                           const std::string &command,
                           const std::string &error_msg,
                           const std::string &current_context);

  // Update usage count for a cached command
  void increment_usage(const std::string &user_request);

  // Cleanup old or unused entries
  void optimize();

  // Get similar cached commands for AI context injection
  std::string get_similar_commands(const std::string &user_request);

  // Get only reliable commands (success_count > failure_count)
  std::string get_reliable_commands(const std::string &user_request);

private:
  std::string filepath;

  std::string compute_hash(const std::string &data);
  std::string get_current_timestamp();
  std::vector<CachedCommand> load_all_entries();
  void save_all_entries(const std::vector<CachedCommand> &entries);
  std::string normalize_request(const std::string &request);
  double compute_similarity(const std::string &str1, const std::string &str2);
};

#endif // COMMAND_CACHE_H
