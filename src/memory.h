#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <vector>

struct MemoryEntry {
  std::string timestamp;
  std::string command;
  std::string cwd;
  std::string os;
  std::string shell;
  int exit_code;
  std::string status; // "success", "fail"
  std::string error_signature;
  std::string summary;
  std::string fix;
};

class MemoryManager {
public:
  MemoryManager(const std::string &filepath);

  // Core function: Append a log entry
  void log_execution(const MemoryEntry &entry);

  // Retrieval: Find similar past errors/fixes
  // Returns a JSON-formatted string of relevant past knowledge to inject into
  // context
  std::string
  retrieve_relevant_context(const std::string &cmd,
                            const std::string &current_error_signature);

private:
  std::string filepath;

  // Helpers
  std::string get_current_timestamp();
  std::string compute_hash(const std::string &data);
};

#endif // MEMORY_H
