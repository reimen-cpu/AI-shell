#include "json_utils.h"
#include <iostream>

namespace json {

std::map<std::string, std::string>
parse_simple_object(const std::string &input) {
  std::map<std::string, std::string> result;
  try {
    auto j = json_t::parse(input);
    if (j.is_object()) {
      for (auto &el : j.items()) {
        if (el.value().is_string()) {
          result[el.key()] = el.value().get<std::string>();
        } else {
          result[el.key()] = el.value().dump();
        }
      }
    }
  } catch (...) {
    // Fallback or empty
  }
  return result;
}

std::string extract_response_content(const std::string &json_response) {
  try {
    auto j = json_t::parse(json_response);
    // Ollama /api/chat response structure
    // { "message": { "content": "..." }, ... }
    if (j.contains("message") && j["message"].contains("content")) {
      return j["message"]["content"].get<std::string>();
    }
    // Direct content (legacy or different endpoint)
    if (j.contains("content")) {
      return j["content"].get<std::string>();
    }
  } catch (const std::exception &e) {
    // std::cerr << "JSON Error: " << e.what() << "\n";
  }
  return "";
}

std::vector<std::string> extract_model_names(const std::string &json_response) {
  std::vector<std::string> models;
  try {
    auto j = json_t::parse(json_response);
    // { "models": [ { "name": "..." }, ... ] }
    if (j.contains("models") && j["models"].is_array()) {
      for (const auto &m : j["models"]) {
        if (m.contains("name")) {
          models.push_back(m["name"].get<std::string>());
        }
      }
    }
  } catch (...) {
  }
  return models;
}

void Builder::add(const std::string &key, const std::string &value) {
  j_obj[key] = value;
}

void Builder::add(const std::string &key, int value) { j_obj[key] = value; }

void Builder::add(const std::string &key, bool value) { j_obj[key] = value; }

void Builder::add_message(const std::string &role, const std::string &content) {
  j_messages.push_back({{"role", role}, {"content", content}});
}

std::string Builder::build() const {
  json_t current = j_obj;
  if (!j_messages.empty()) {
    current["messages"] = j_messages;
  }
  return current.dump();
}

} // namespace json
