// web_server.cpp - Tutorial 5: a small static web server (HTTP/1.0 style)
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "net.hpp"

constexpr size_t MAX_HEAD = 8192; // the largest request head that we accept

// Wait until the socket has data. Return false after "ms" milliseconds with no data.
static bool wait_readable(socket_t s, int ms) {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(s, &set);
  timeval tv{};
  tv.tv_sec = ms / 1000;
  tv.tv_usec = (ms % 1000) * 1000;
  return select(static_cast<int>(s) + 1, &set, nullptr, nullptr, &tv) > 0;
}

// Read the request head (request line + headers).
static bool read_head(socket_t s, std::string& head) {
  head.clear();
  char buf[1024];
  while (head.size() < MAX_HEAD) {
    if (!wait_readable(s, 5000)) return false;
    long long n = net::recv_some(s, buf, sizeof buf);
    if (n <= 0) return false;
    head.append(buf, static_cast<size_t>(n));
    if (head.find("\r\n\r\n") != std::string::npos ||
        head.find("\n\n") != std::string::npos)
      return true;
  }
  return false;
}

// Change %XX codes to characters.
static std::string url_decode(const std::string& in) {
  std::string out;
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%' && i + 2 < in.size() &&
        std::isxdigit(static_cast<unsigned char>(in[i + 1])) &&
        std::isxdigit(static_cast<unsigned char>(in[i + 2]))) {
      out += static_cast<char>(std::strtol(in.substr(i + 1, 2).c_str(), nullptr, 16));
      i += 2;
    } else {
      out += in[i];
    }
  }
  return out;
}

// Make a file path from the request target. Return false if the target is not safe.
static bool make_path(const std::string& target, const std::string& root,
                      std::string& path) {
  std::string t = url_decode(target.substr(0, target.find('?'))); // decode FIRST
  if (t.empty() || t[0] != '/') return false;
  if (t.find("..") != std::string::npos || t.find('\\') != std::string::npos ||
      t.find('\0') != std::string::npos)
    return false;
  if (t.back() == '/') t += "index.html";
  path = root + t;
  return true;
}

static bool ends_with(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string mime_type(const std::string& path) {
  if (ends_with(path, ".html") || ends_with(path, ".htm")) return "text/html; charset=utf-8";
  if (ends_with(path, ".css")) return "text/css";
  if (ends_with(path, ".js")) return "text/javascript";
  if (ends_with(path, ".txt")) return "text/plain; charset=utf-8";
  if (ends_with(path, ".png")) return "image/png";
  if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) return "image/jpeg";
  if (ends_with(path, ".gif")) return "image/gif";
  if (ends_with(path, ".svg")) return "image/svg+xml";
  if (ends_with(path, ".ico")) return "image/x-icon";
  return "application/octet-stream";
}

// Read a regular file into "out". Return false if it is not a regular file.
static bool read_file(const std::string& path, std::string& out) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) return false;
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream buffer;
  buffer << in.rdbuf();
  out = buffer.str();
  return true;
}

static void send_response(socket_t s, int code, const char* reason,
                          const std::string& type, const std::string& body,
                          bool head_only) {
  std::ostringstream head;
  head << "HTTP/1.0 " << code << ' ' << reason << "\r\n"
       << "Content-Type: " << type << "\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n"
       << "\r\n";
  const std::string h = head.str();
  net::send_all(s, h.data(), h.size());
  if (!head_only) net::send_all(s, body.data(), body.size());
}

static void send_error(socket_t s, int code, const char* reason) {
  const std::string body = "<html><body><h1>" + std::to_string(code) + " " + reason +
                           "</h1></body></html>\n";
  send_response(s, code, reason, "text/html", body, false);
}

// Handle one request. Return the status code. "what" gets the text for the log.
static int handle_client(socket_t s, const std::string& root, std::string& what) {
  std::string head;
  if (!read_head(s, head)) {
    what = "(no valid request)";
    send_error(s, 400, "Bad Request");
    return 400;
  }
  std::istringstream first_line(head.substr(0, head.find('\n')));
  std::string method, target, version;
  first_line >> method >> target >> version;
  what = method + " " + target;
  if (method.empty() || target.empty()) {
    send_error(s, 400, "Bad Request");
    return 400;
  }
  const bool head_only = (method == "HEAD");
  if (method != "GET" && !head_only) {
    send_error(s, 405, "Method Not Allowed");
    return 405;
  }
  std::string path;
  if (!make_path(target, root, path)) {
    send_error(s, 400, "Bad Request");
    return 400;
  }
  std::string body;
  if (!read_file(path, body)) {
    send_error(s, 404, "Not Found");
    return 404;
  }
  send_response(s, 200, "OK", mime_type(path), body, head_only);
  return 200;
}

int main(int argc, char** argv) {
  const char* port = (argc > 1) ? argv[1] : "8080";
  const std::string root = (argc > 2) ? argv[2] : "www";
  try {
    net::Runtime runtime;
    net::Socket listener = net::tcp_listen(port);
    std::cout << "Web server: http://localhost:" << port
              << "/ (files from the folder \"" << root << "\")" << std::endl;
    for (;;) {
      sockaddr_storage peer{};
      sock_len peer_len = sizeof peer;
      net::Socket client(accept(listener.get(),
                         reinterpret_cast<sockaddr*>(&peer), &peer_len));
      if (!client.valid()) continue;
      std::string who = net::peer_name(reinterpret_cast<sockaddr*>(&peer), peer_len);
      std::string what;
      int status = handle_client(client.get(), root, what);
      net::shutdown_send(client.get()); // tell the browser: no more data
      std::cout << who << " " << what << " -> " << status << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
