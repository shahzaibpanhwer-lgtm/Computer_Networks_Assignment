// daytime_client.cpp - Tutorial 2
// Connect to a daytime server. Show the text. Exit.
#include <cstdio>
#include <cstring>
// ---- Platform block ----
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  typedef SOCKET sock_t;
  typedef int sock_len_t;
  #define BAD_SOCKET INVALID_SOCKET
  #define CLOSE_SOCKET closesocket
  #define LAST_ERROR() WSAGetLastError()
#else
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <unistd.h>
  #include <cerrno>
  typedef int sock_t;
  typedef socklen_t sock_len_t;
  #define BAD_SOCKET (-1)
  #define CLOSE_SOCKET close
  #define LAST_ERROR() errno
#endif

int main(int argc, char** argv) {
  const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
  const char* port = (argc > 2) ? argv[2] : "1313";
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    std::fprintf(stderr, "WSAStartup failed\n");
    return 1;
  }
#endif
  // Step 1: find the address (or addresses) of the server.
  addrinfo hints;
  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;        // IPv4
  hints.ai_socktype = SOCK_STREAM;  // TCP
  addrinfo* list = nullptr;
  int rc = getaddrinfo(host, port, &hints, &list);
  if (rc != 0) {
    std::fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(rc));
    return 1;
  }
  // Step 2: try each address until one connects.
  sock_t s = BAD_SOCKET;
  int last_error = 0;
  for (addrinfo* p = list; p != nullptr; p = p->ai_next) {
    s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (s == BAD_SOCKET) { last_error = LAST_ERROR(); continue; }
    if (connect(s, p->ai_addr, static_cast<sock_len_t>(p->ai_addrlen)) == 0) break;
    last_error = LAST_ERROR();
    CLOSE_SOCKET(s);
    s = BAD_SOCKET;
  }
  freeaddrinfo(list);
  if (s == BAD_SOCKET) {
    std::fprintf(stderr, "Cannot connect to %s:%s (error %d)\n", host, port, last_error);
    return 1;
  }
  // Step 3: read until the server closes the connection.
  char buf[256];
  for (;;) {
    int n = static_cast<int>(recv(s, buf, sizeof buf, 0));
    if (n <= 0) break; // 0 = closed, < 0 = error
    std::fwrite(buf, 1, static_cast<size_t>(n), stdout);
  }
  // Step 4: clean up.
  CLOSE_SOCKET(s);
#ifdef _WIN32
  WSACleanup();
#endif
  return 0;
}
