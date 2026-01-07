#include "context_manager.h"
#include "json_utils.h"
#include <fstream>
#include <sstream>

ContextManager::ContextManager(const std::string &file_path)
    : file_path(file_path) {}

bool ContextManager::exists() {
  std::ifstream f(file_path.c_str());
  return f.good();
}

bool ContextManager::load_context(AiContext &context) {
  if (!exists())
    return false;
  std::ifstream t(file_path);
  std::stringstream buffer;
  buffer << t.rdbuf();
  std::string content = buffer.str();

  auto map = json::parse_simple_object(content);
  context.operating_mode = map["operating_mode"];
  context.model_name = map["model_name"];
  context.env_block = map["env_block"];
  context.transcript = map["transcript"];

  return true;
}

bool ContextManager::save_context(const AiContext &context) {
  json::Builder builder;
  builder.add("operating_mode", context.operating_mode);
  builder.add("model_name", context.model_name);
  builder.add("env_block", context.env_block);
  builder.add("transcript", context.transcript);

  std::string content = builder.build();
  std::ofstream out(file_path);
  if (!out)
    return false;
  out << content;
  out.close();
  return true;
}

void ContextManager::clear_context() { std::remove(file_path.c_str()); }
