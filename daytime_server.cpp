// daytime_server.cpp - Tutorial 1
// Send the date and time to each client. Then close the connection.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
// ---- Platform block: the only part that is different on each system ----
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
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  #include <cerrno>
  typedef int sock_t;
  typedef socklen_t sock_len_t;
  #define BAD_SOCKET (-1)
  #define CLOSE_SOCKET close
  #define LAST_ERROR() errno
#endif

// Write the local date and time in text.
static void make_time_text(char* text, size_t size) {
  std::time_t now = std::time(nullptr);
  std::tm local;
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  std::strftime(text, size, "%a %b %d %H:%M:%S %Y\r\n", &local);
}

int main(int argc, char** argv) {
  unsigned short port = (argc > 1) ? static_cast<unsigned short>(std::atoi(argv[1])) : 1313;
#ifdef _WIN32
  WSADATA wsa; // Step 0: start Winsock
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    std::fprintf(stderr, "WSAStartup failed\n");
    return 1;
  }
#endif
  // Step 1: make a TCP socket.
  sock_t listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener == BAD_SOCKET) {
    std::fprintf(stderr, "socket failed (error %d)\n", LAST_ERROR());
    return 1;
  }
  // Step 2 (Linux only): allow a quick restart of the server.
#ifndef _WIN32
  int on = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
#endif
  // Step 3: attach the socket to the port, on all network interfaces.
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
    std::fprintf(stderr, "bind failed (error %d). Is the port in use?\n", LAST_ERROR());
    return 1;
  }
  // Step 4: start to listen. Up to 5 clients can wait in the queue.
  if (listen(listener, 5) != 0) {
    std::fprintf(stderr, "listen failed (error %d)\n", LAST_ERROR());
    return 1;
  }
  std::printf("Daytime server on port %u. Press Ctrl+C to stop.\n", port);
  for (;;) {
    // Step 5: wait for client. accept() returns a NEW socket for client.
    sockaddr_in peer;
    sock_len_t peer_len = sizeof peer;
    sock_t client = accept(listener, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (client == BAD_SOCKET) {
      std::fprintf(stderr, "accept failed (error %d)\n", LAST_ERROR());
      continue;
    }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof ip);
    std::printf("Client %s:%u\n", ip, static_cast<unsigned>(ntohs(peer.sin_port)));
    // Step 6: make the text and send it.
    char text[64];
    make_time_text(text, sizeof text);
    send(client, text, static_cast<int>(std::strlen(text)), 0);
    // Step 7: close the client socket. The client sees the end of the data.
    CLOSE_SOCKET(client);
  }
}
