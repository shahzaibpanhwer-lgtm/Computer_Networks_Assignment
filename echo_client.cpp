// echo_client.cpp - read lines from the keyboard, send them, show the echo - Tutorial 4
#include <iostream>
#include <string>
#include "net.hpp"

int main(int argc, char** argv) {
  const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
  const char* port = (argc > 2) ? argv[2] : "5000";
  try {
    net::Runtime runtime;
    net::Socket s = net::tcp_connect(host, port);
    std::cout << "Connected. Type text. Press Ctrl+D (Linux) or Ctrl+Z then Enter (Windows) to stop.\n";
    std::string line;
    while (std::getline(std::cin, line)) {
      line += "\n";
      if (!net::send_all(s.get(), line.data(), line.size())) break;
      std::string reply(line.size(), '\0');
      if (!net::recv_all(s.get(), &reply[0], reply.size())) break;
      std::cout << "Echo: " << reply;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
