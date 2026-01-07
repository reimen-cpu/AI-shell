#include "http_client.h"
#include <iostream>
#include <vector>
#include <windows.h>
#include <winhttp.h>

namespace http {

Client::Client(const std::string &host, int port) : host(host), port(port) {}

Client::~Client() {}

Response Client::post(const std::string &path, const std::string &json_body) {
  Response response = {0, ""};

  HINTERNET hSession =
      WinHttpOpen(L"AI-Shell-Agent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
    return response;

  // Set timeout to 5 minutes (300000 ms) for LLM responses
  WinHttpSetTimeouts(hSession, 300000, 300000, 300000, 300000);

  std::wstring wHost(host.begin(), host.end());
  HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return response;
  }

  std::wstring wPath(path.begin(), path.end());
  HINTERNET hRequest =
      WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(), NULL,
                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
  }

  std::wstring headers = L"Content-Type: application/json\r\n";
  BOOL bResults =
      WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(),
                         (LPVOID)json_body.c_str(), (DWORD)json_body.length(),
                         (DWORD)json_body.length(), 0);

  if (bResults) {
    bResults = WinHttpReceiveResponse(hRequest, NULL);
  }

  if (bResults) {
    DWORD dwStatusCode = 0;
    DWORD dwSize = sizeof(dwStatusCode);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
                        WINHTTP_NO_HEADER_INDEX);
    response.status_code = dwStatusCode;

    DWORD dwSizeAvailable = 0;
    std::vector<char> buffer;
    do {
      dwSizeAvailable = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &dwSizeAvailable))
        break;
      if (dwSizeAvailable == 0)
        break;

      std::vector<char> chunk(dwSizeAvailable + 1);
      DWORD dwDownloaded = 0;
      if (WinHttpReadData(hRequest, &chunk[0], dwSizeAvailable,
                          &dwDownloaded)) {
        buffer.insert(buffer.end(), chunk.begin(),
                      chunk.begin() + dwDownloaded);
      }
    } while (dwSizeAvailable > 0);

    if (!buffer.empty()) {
      response.body.assign(buffer.begin(), buffer.end());
    }
  } else {
    // std::cerr << "WinHTTP Error: " << GetLastError() << std::endl;
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  return response;
}

Response Client::get(const std::string &path) {
  Response response = {0, ""};

  HINTERNET hSession =
      WinHttpOpen(L"AI-Shell-Agent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
    return response;

  std::wstring wHost(host.begin(), host.end());
  HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return response;
  }

  std::wstring wPath(path.begin(), path.end());
  HINTERNET hRequest =
      WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL,
                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
  }

  BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

  if (bResults) {
    bResults = WinHttpReceiveResponse(hRequest, NULL);
  }

  if (bResults) {
    DWORD dwStatusCode = 0;
    DWORD dwSize = sizeof(dwStatusCode);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
                        WINHTTP_NO_HEADER_INDEX);
    response.status_code = dwStatusCode;

    DWORD dwSizeAvailable = 0;
    std::vector<char> buffer;
    do {
      dwSizeAvailable = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &dwSizeAvailable))
        break;
      if (dwSizeAvailable == 0)
        break;

      std::vector<char> chunk(dwSizeAvailable + 1);
      DWORD dwDownloaded = 0;
      if (WinHttpReadData(hRequest, &chunk[0], dwSizeAvailable,
                          &dwDownloaded)) {
        buffer.insert(buffer.end(), chunk.begin(),
                      chunk.begin() + dwDownloaded);
      }
    } while (dwSizeAvailable > 0);

    if (!buffer.empty()) {
      response.body.assign(buffer.begin(), buffer.end());
    }
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  return response;
}

bool Client::is_reachable() { return true; }

} // namespace http
