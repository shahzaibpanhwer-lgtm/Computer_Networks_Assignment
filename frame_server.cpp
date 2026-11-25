// frame_server.cpp - Tutorial 6: reads each message, returns it in upper case
#include <algorithm>
#include <cctype>
#include <iostream>
#include "net.hpp"

int main(int argc, char** argv) {
  const char* port = (argc > 1) ? argv[1] : "5001";
  try {
    net::Runtime runtime;
    net::Socket listener = net::tcp_listen(port);
    std::cout << "Frame server on port " << port << "\n";
    for (;;) {
      net::Socket client(accept(listener.get(), nullptr, nullptr));
      if (!client.valid()) continue;
      std::string msg;
      while (net::recv_msg(client.get(), msg)) {
        std::cout << "Got " << msg.size() << " bytes\n";
        std::transform(msg.begin(), msg.end(), msg.begin(),
          [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (!net::send_msg(client.get(), msg)) break;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
