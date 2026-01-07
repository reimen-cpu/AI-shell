#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <map>
#include <sstream>
#include <string>
#include <vector>


// Very simple JSON helper to avoid external dependencies
namespace json {

// Escapes a string for JSON
std::string escape(const std::string &str);

// Parses a simple JSON object (flat, no nested arrays support for now except
// specific use cases) NOTE: This is a tailored parser for this specific tool's
// needs.
std::map<std::string, std::string>
parse_simple_object(const std::string &input);

// Extracts the "content" field from Ollama's response (which is nested)
std::string extract_response_content(const std::string &json_response);

// Extracts model names from /api/tags response
std::vector<std::string> extract_model_names(const std::string &json_response);

class Builder {
public:
  void add(const std::string &key, const std::string &value);
  void add(const std::string &key, int value);
  void add(const std::string &key, bool value);
  void
  add_message(const std::string &role,
              const std::string &content); // tailored for Ollama messages array
  std::string build() const;

private:
  std::map<std::string, std::string> simple_fields;
  std::vector<std::pair<std::string, std::string>> messages;
};

} // namespace json

#endif // JSON_UTILS_H
