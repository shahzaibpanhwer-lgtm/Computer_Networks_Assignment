// select_server.cpp - Tutorial 9: one thread serves many clients with select()
// Build (Linux): g++ -std=c++17 -Wall -Wextra -o select_server select_server.cpp
// Build (MinGW): g++ -std=c++17 -o select_server.exe select_server.cpp -lws2_32
#include <iostream>
#include <map>
#include <vector>
#include "net.hpp"

int main(int argc, char** argv) {
  const char* port = (argc > 1) ? argv[1] : "5000";
  try {
    net::Runtime runtime;
    net::Socket listener = net::tcp_listen(port);
    std::cout << "select() echo server on port " << port << "\n";

    std::map<socket_t, net::Socket> clients;   // one Socket per connected client

    for (;;) {
      // Build the read set: the listener plus every client.
      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(listener.get(), &readset);
      int maxfd = static_cast<int>(listener.get());
      for (auto& kv : clients) {
        FD_SET(kv.first, &readset);
        if (static_cast<int>(kv.first) > maxfd) maxfd = static_cast<int>(kv.first);
      }

      if (select(maxfd + 1, &readset, nullptr, nullptr, nullptr) < 0) {
        std::cerr << "select failed: " << net::error_text(net::last_error()) << "\n";
        break;
      }

      // A new client is waiting.
      if (FD_ISSET(listener.get(), &readset)) {
        sockaddr_storage peer{};
        sock_len peer_len = sizeof peer;
        net::Socket c(accept(listener.get(),
                      reinterpret_cast<sockaddr*>(&peer), &peer_len));
        if (c.valid()) {
          socket_t fd = c.get();
          std::cout << "Client "
                    << net::peer_name(reinterpret_cast<sockaddr*>(&peer), peer_len)
                    << " connected\n";
          clients.emplace(fd, std::move(c));
        }
      }

      // Echo from any client that has data.
      std::vector<socket_t> closed;
      for (auto& kv : clients) {
        socket_t fd = kv.first;
        if (!FD_ISSET(fd, &readset)) continue;
        char buf[1024];
        long long n = net::recv_some(fd, buf, sizeof buf);
        if (n <= 0) { closed.push_back(fd); continue; }
        net::send_all(fd, buf, static_cast<size_t>(n));
      }
      for (socket_t fd : closed) {
        clients.erase(fd);               // Socket destructor closes the handle
        std::cout << "Client disconnected\n";
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
