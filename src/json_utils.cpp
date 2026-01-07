#include "json_utils.h"
#include <iomanip>
#include <iostream>
#include <sstream>

namespace json {

std::string escape(const std::string &str) {
  std::ostringstream ss;
  for (char c : str) {
    switch (c) {
    case '"':
      ss << "\\\"";
      break;
    case '\\':
      ss << "\\\\";
      break;
    case '\b':
      ss << "\\b";
      break;
    case '\f':
      ss << "\\f";
      break;
    case '\n':
      ss << "\\n";
      break;
    case '\r':
      ss << "\\r";
      break;
    case '\t':
      ss << "\\t";
      break;
    default:
      if ('\x00' <= c && c <= '\x1f') {
        ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
      } else {
        ss << c;
      }
    }
  }
  return ss.str();
}

std::map<std::string, std::string>
parse_simple_object(const std::string &input) {
  std::map<std::string, std::string> result;
  size_t pos = 0;
  while (pos < input.length()) {
    size_t key_start = input.find('"', pos);
    if (key_start == std::string::npos)
      break;
    size_t key_end = input.find('"', key_start + 1);
    if (key_end == std::string::npos)
      break;

    std::string key = input.substr(key_start + 1, key_end - key_start - 1);

    size_t colon = input.find(':', key_end);
    if (colon == std::string::npos)
      break;

    size_t val_start = input.find('"', colon);
    if (val_start == std::string::npos)
      break;

    size_t val_end = val_start + 1;
    while (val_end < input.length()) {
      if (input[val_end] == '"' && input[val_end - 1] != '\\') {
        break;
      }
      val_end++;
    }
    if (val_end >= input.length())
      break;

    std::string val_raw = input.substr(val_start + 1, val_end - val_start - 1);
    std::string val_clean;
    for (size_t i = 0; i < val_raw.size(); ++i) {
      if (val_raw[i] == '\\' && i + 1 < val_raw.size()) {
        char next = val_raw[i + 1];
        if (next == '"')
          val_clean += '"';
        else if (next == 'n')
          val_clean += '\n';
        else if (next == '\\')
          val_clean += '\\';
        else
          val_clean += next;
        i++;
      } else {
        val_clean += val_raw[i];
      }
    }

    result[key] = val_clean;
    pos = val_end + 1;
  }
  return result;
}

std::string extract_response_content(const std::string &json_response) {
  std::string search_key = "\"content\":";
  size_t key_pos = json_response.find(search_key);
  if (key_pos == std::string::npos)
    return "";

  size_t val_start = json_response.find('"', key_pos + search_key.length());
  if (val_start == std::string::npos)
    return "";

  size_t val_end = val_start + 1;
  while (val_end < json_response.length()) {
    if (json_response[val_end] == '"' && json_response[val_end - 1] != '\\') {
      break;
    }
    val_end++;
  }

  std::string raw =
      json_response.substr(val_start + 1, val_end - val_start - 1);
  std::string clean;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\\' && i + 1 < raw.size()) {
      char next = raw[i + 1];
      if (next == 'u' && i + 5 < raw.size()) {
        std::string hex_str = raw.substr(i + 2, 4);
        try {
          int codepoint = std::stoi(hex_str, nullptr, 16);
          if (codepoint <= 0x7F) {
            clean += (char)codepoint;
          } else {
            // Simple robust fallback for non-ASCII
            // (Minimal UTF-8 encoding if needed, or just skip)
            if (codepoint <= 0x7FF) {
              clean += (char)(0xC0 | ((codepoint >> 6) & 0x1F));
              clean += (char)(0x80 | (codepoint & 0x3F));
            } else {
              clean += (char)(0xE0 | ((codepoint >> 12) & 0x0F));
              clean += (char)(0x80 | ((codepoint >> 6) & 0x3F));
              clean += (char)(0x80 | (codepoint & 0x3F));
            }
          }
          i += 5;
        } catch (...) {
          clean += "\\u";
          i++;
        }
      } else if (next == '"')
        clean += '"';
      else if (next == 'n')
        clean += '\n';
      else if (next == 'r')
        clean += '\r';
      else if (next == 't')
        clean += '\t';
      else if (next == '\\')
        clean += '\\';
      else {
        clean += next;
        i++;
      }
    } else {
      clean += raw[i];
    }
  }
  return clean;
}

std::vector<std::string> extract_model_names(const std::string &json_response) {
  std::vector<std::string> models;
  size_t pos = 0;
  while (pos < json_response.length()) {
    size_t name_key = json_response.find("\"name\":", pos);
    if (name_key == std::string::npos)
      break;

    size_t val_start = json_response.find('"', name_key + 7);
    if (val_start == std::string::npos)
      break;

    size_t val_end = val_start + 1;
    while (val_end < json_response.length()) {
      if (json_response[val_end] == '"' && json_response[val_end - 1] != '\\') {
        break;
      }
      val_end++;
    }
    if (val_end >= json_response.length())
      break;

    std::string model =
        json_response.substr(val_start + 1, val_end - val_start - 1);
    models.push_back(model);

    pos = val_end + 1;
  }
  return models;
}

void Builder::add(const std::string &key, const std::string &value) {
  simple_fields[key] = "\"" + escape(value) + "\"";
}

void Builder::add(const std::string &key, int value) {
  simple_fields[key] = std::to_string(value);
}

void Builder::add(const std::string &key, bool value) {
  simple_fields[key] = value ? "true" : "false";
}

void Builder::add_message(const std::string &role, const std::string &content) {
  messages.push_back({role, content});
}

std::string Builder::build() const {
  std::ostringstream ss;
  ss << "{";
  bool first = true;
  for (std::map<std::string, std::string>::const_iterator it =
           simple_fields.begin();
       it != simple_fields.end(); ++it) {
    if (!first)
      ss << ", ";
    ss << "\"" << it->first << "\": " << it->second;
    first = false;
  }

  if (!messages.empty()) {
    if (!first)
      ss << ", ";
    ss << "\"messages\": [";
    for (size_t i = 0; i < messages.size(); ++i) {
      if (i > 0)
        ss << ", ";
      ss << "{ \"role\": \"" << messages[i].first << "\", \"content\": \""
         << escape(messages[i].second) << "\" }";
    }
    ss << "]";
  }

  ss << "}";
  return ss.str();
}

} // namespace json
