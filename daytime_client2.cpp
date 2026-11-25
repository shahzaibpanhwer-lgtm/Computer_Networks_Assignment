// daytime_client2.cpp - Tutorial 2 again, now with net.hpp
#include <iostream>
#include "net.hpp"

int main(int argc, char** argv) {
  const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
  const char* port = (argc > 2) ? argv[2] : "1313";
  try {
    net::Runtime runtime;
    net::Socket s = net::tcp_connect(host, port); // getaddrinfo + socket + connect
    char buf[256];
    for (;;) {
      long long n = net::recv_some(s.get(), buf, sizeof buf);
      if (n <= 0) break;
      std::cout.write(buf, static_cast<std::streamsize>(n));
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
