#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "json.hpp" // nlohmann/json
#include <map>
#include <string>
#include <vector>


// Using nlohmann::json alias for convenience
using json_t = nlohmann::json;

namespace json {

// Helper: Parse string to map (kept for compatibility if needed, but better to
// use json object directly)
std::map<std::string, std::string>
parse_simple_object(const std::string &input);

// Extract "content" field from a standard Ollama/OpenAI chat response
std::string extract_response_content(const std::string &json_response);

// Extract model names from Ollama tags response
std::vector<std::string> extract_model_names(const std::string &json_response);

// Simple Builder Wrapper to minimize changes in main.cpp, but internally uses
// nlohmann/json
class Builder {
public:
  void add(const std::string &key, const std::string &value);
  void add(const std::string &key, int value);
  void add(const std::string &key, bool value);
  void add_message(const std::string &role, const std::string &content);

  std::string build() const;

private:
  json_t j_obj;
  json_t j_messages = json_t::array();
};

} // namespace json

#endif // JSON_UTILS_H
