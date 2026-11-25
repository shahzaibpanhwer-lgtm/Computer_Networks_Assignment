// net.hpp - portable socket layer for Windows and Linux (C++17)
// Core layer (Tutorial 3) + local_time (Tutorial 3) + framing (Tutorial 6).
#pragma once
#include <ctime>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  using socket_t = SOCKET;
  using sock_len = int;
  constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
  #define SHUT_WR SD_SEND
  #ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
  #endif
#else
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <netinet/in.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <unistd.h>
  #include <cerrno>
  #include <csignal>
  using socket_t = int;
  using sock_len = socklen_t;
  constexpr socket_t INVALID_SOCK = -1;
  #ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
  #endif
#endif

namespace net {

// ---- Errors ---------------------------------------------------------
inline int last_error() {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

inline std::string error_text(int code) {
#ifdef _WIN32
  char* buf = nullptr;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                 FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, static_cast<DWORD>(code), 0,
                 reinterpret_cast<LPSTR>(&buf), 0, nullptr);
  std::string s = buf ? buf : "unknown error";
  if (buf) LocalFree(buf);
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
    s.pop_back();
  return s;
#else
  return std::strerror(code);
#endif
}

[[noreturn]] inline void fail_with(const std::string& what, int code) {
  throw std::runtime_error(what + ": " + error_text(code));
}
[[noreturn]] inline void fail(const std::string& what) {
  fail_with(what, last_error());
}

// ---- Library start-up (RAII) -----------------------------------------
class Runtime {
public:
  Runtime() {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
      throw std::runtime_error("WSAStartup failed");
#else
    std::signal(SIGPIPE, SIG_IGN);
#endif
  }
  ~Runtime() {
#ifdef _WIN32
    WSACleanup();
#endif
  }
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;
};

// ---- Socket handle (RAII) ---------------------------------------------
inline void close_socket(socket_t s) {
#ifdef _WIN32
  closesocket(s);
#else
  close(s);
#endif
}

class Socket {
public:
  Socket() = default;
  explicit Socket(socket_t s) : s_(s) {}
  ~Socket() { reset(); }
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  Socket(Socket&& o) noexcept : s_(o.s_) { o.s_ = INVALID_SOCK; }
  Socket& operator=(Socket&& o) noexcept {
    if (this != &o) { reset(); s_ = o.s_; o.s_ = INVALID_SOCK; }
    return *this;
  }
  socket_t get() const { return s_; }
  bool valid() const { return s_ != INVALID_SOCK; }
  void reset() {
    if (valid()) { close_socket(s_); s_ = INVALID_SOCK; }
  }
private:
  socket_t s_ = INVALID_SOCK;
};

// ---- Basic transfer ----------------------------------------------------
inline long long recv_some(socket_t s, char* buf, size_t n) {
  return recv(s, buf, static_cast<int>(n), 0);
}

inline bool send_all(socket_t s, const char* data, size_t n) {
  size_t sent = 0;
  while (sent < n) {
    long long r = send(s, data + sent, static_cast<int>(n - sent), MSG_NOSIGNAL);
    if (r <= 0) return false;
    sent += static_cast<size_t>(r);
  }
  return true;
}

inline bool recv_all(socket_t s, char* buf, size_t n) {
  size_t got = 0;
  while (got < n) {
    long long r = recv_some(s, buf + got, n - got);
    if (r <= 0) return false;
    got += static_cast<size_t>(r);
  }
  return true;
}

inline void shutdown_send(socket_t s) { shutdown(s, SHUT_WR); }

// ---- Address helpers ----------------------------------------------------
inline std::string peer_name(const sockaddr* sa, sock_len len) {
  char host[NI_MAXHOST], serv[NI_MAXSERV];
  if (getnameinfo(sa, len, host, sizeof host, serv, sizeof serv,
                  NI_NUMERICHOST | NI_NUMERICSERV) != 0)
    return "unknown";
  return std::string(host) + ":" + serv;
}

// ---- TCP server and client ----------------------------------------------
inline Socket tcp_listen(const char* port, int backlog = 16) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  addrinfo* res = nullptr;
  int rc = getaddrinfo(nullptr, port, &hints, &res);
  if (rc != 0) throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(rc));
  Socket s(socket(res->ai_family, res->ai_socktype, res->ai_protocol));
  if (!s.valid()) { int e = last_error(); freeaddrinfo(res); fail_with("socket", e); }
  int on = 1;
#ifdef _WIN32
  setsockopt(s.get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
             reinterpret_cast<const char*>(&on), sizeof on);
#else
  setsockopt(s.get(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
#endif
  if (bind(s.get(), res->ai_addr, static_cast<sock_len>(res->ai_addrlen)) != 0) {
    int e = last_error(); freeaddrinfo(res); fail_with("bind", e);
  }
  freeaddrinfo(res);
  if (listen(s.get(), backlog) != 0) fail("listen");
  return s;
}

inline Socket tcp_connect(const char* host, const char* port) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  int rc = getaddrinfo(host, port, &hints, &res);
  if (rc != 0) throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(rc));
  int last = 0;
  for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
    Socket s(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
    if (!s.valid()) { last = last_error(); continue; }
    if (connect(s.get(), p->ai_addr, static_cast<sock_len>(p->ai_addrlen)) == 0) {
      freeaddrinfo(res);
      return s;
    }
    last = last_error();
  }
  freeaddrinfo(res);
  fail_with("connect", last);
}

// ---- Local time (safe on both systems) --------------------------------
inline std::tm local_time(std::time_t t) {
  std::tm result{};
#ifdef _WIN32
  localtime_s(&result, &t);
#else
  localtime_r(&t, &result);
#endif
  return result;
}

// ---- Length-prefixed messages (Tutorial 6) ----------------------------
constexpr uint32_t MAX_MSG = 1024 * 1024; // 1 MiB limit

inline bool send_msg(socket_t s, const std::string& msg) {
  if (msg.size() > MAX_MSG) return false;
  uint32_t len_net = htonl(static_cast<uint32_t>(msg.size()));
  return send_all(s, reinterpret_cast<const char*>(&len_net), sizeof len_net) &&
         send_all(s, msg.data(), msg.size());
}

inline bool recv_msg(socket_t s, std::string& out) {
  uint32_t len_net = 0;
  if (!recv_all(s, reinterpret_cast<char*>(&len_net), sizeof len_net)) return false;
  uint32_t len = ntohl(len_net);
  if (len > MAX_MSG) return false;
  out.assign(len, '\0');
  return len == 0 || recv_all(s, &out[0], len);
}

} // namespace net
