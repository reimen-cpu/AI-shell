#ifndef CONTEXT_MANAGER_H
#define CONTEXT_MANAGER_H

#include <string>
#include <vector>

struct AiContext {
  std::string operating_mode;
  std::string model_name;
  std::string env_block;
  std::string transcript;
};

class ContextManager {
public:
  ContextManager(const std::string &file_path);

  bool load_context(AiContext &context);
  bool save_context(const AiContext &context);
  void clear_context();
  bool exists();

private:
  std::string file_path;
};

#endif // CONTEXT_MANAGER_H
