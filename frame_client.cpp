// frame_client.cpp - usage: frame_client HOST PORT word1 word2 ... - Tutorial 6
#include <iostream>
#include "net.hpp"

int main(int argc, char** argv) {
  if (argc < 4) { std::cerr << "Usage: frame_client HOST PORT MSG...\n"; return 2; }
  try {
    net::Runtime runtime;
    net::Socket s = net::tcp_connect(argv[1], argv[2]);
    for (int i = 3; i < argc; ++i) {
      if (!net::send_msg(s.get(), argv[i])) { std::cerr << "send failed\n"; return 1; }
    }
    for (int i = 3; i < argc; ++i) {
      std::string reply;
      if (!net::recv_msg(s.get(), reply)) { std::cerr << "recv failed\n"; return 1; }
      std::cout << "Reply: " << reply << "\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
