// echo_server.cpp - iterative TCP echo server (one client at a time) - Tutorial 4
#include <iostream>
#include "net.hpp"

int main(int argc, char** argv) {
  const char* port = (argc > 1) ? argv[1] : "5000";
  try {
    net::Runtime runtime;
    net::Socket listener = net::tcp_listen(port);
    std::cout << "Echo server on port " << port << "\n";
    for (;;) {
      sockaddr_storage peer{};
      sock_len peer_len = sizeof peer;
      net::Socket client(accept(listener.get(),
                         reinterpret_cast<sockaddr*>(&peer), &peer_len));
      if (!client.valid()) {
        std::cerr << "accept failed: " << net::error_text(net::last_error()) << "\n";
        continue;
      }
      std::cout << "Client " << net::peer_name(reinterpret_cast<sockaddr*>(&peer), peer_len)
                << " connected\n";
      char buf[1024];
      for (;;) {
        long long n = net::recv_some(client.get(), buf, sizeof buf);
        if (n <= 0) break; // closed or error
        if (!net::send_all(client.get(), buf, static_cast<size_t>(n))) break;
      }
      std::cout << "Client disconnected\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
