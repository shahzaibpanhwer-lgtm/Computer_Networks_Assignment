// daytime_server2.cpp - Tutorial 1 again, now with net.hpp
#include <cstring>
#include <ctime>
#include <iostream>
#include "net.hpp"

int main(int argc, char** argv) {
  const char* port = (argc > 1) ? argv[1] : "1313";
  try {
    net::Runtime runtime;                            // WSAStartup on Windows
    net::Socket listener = net::tcp_listen(port);    // socket + bind + listen
    std::cout << "Daytime server on port " << port << std::endl;
    for (;;) {
      sockaddr_storage peer{};
      sock_len peer_len = sizeof peer;
      net::Socket client(accept(listener.get(),
                         reinterpret_cast<sockaddr*>(&peer), &peer_len));
      if (!client.valid()) continue;
      std::cout << "Client "
                << net::peer_name(reinterpret_cast<sockaddr*>(&peer), peer_len)
                << std::endl;
      std::tm local = net::local_time(std::time(nullptr));
      char text[64];
      std::strftime(text, sizeof text, "%a %b %d %H:%M:%S %Y\r\n", &local);
      net::send_all(client.get(), text, std::strlen(text));
    } // the Socket destructor closes the client socket here
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
