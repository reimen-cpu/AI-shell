#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>

namespace http {

struct Response {
  int status_code;
  std::string body;
};

class Client {
public:
  Client(const std::string &host, int port);
  ~Client();

  Response post(const std::string &path, const std::string &json_body);
  Response get(const std::string &path);
  bool is_reachable();

private:
  std::string host;
  int port;
};

} // namespace http

#endif // HTTP_CLIENT_H
