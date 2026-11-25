// chat_client.cpp - Assignment: multi-user chat client
// Two threads so incoming text appears while you type (R10). Portable (net.hpp only).
//
// Build (Linux):   g++ -std=c++17 -Wall -Wextra -pthread -o chat_client chat_client.cpp
// Build (MinGW):   g++ -std=c++17 -o chat_client.exe chat_client.cpp -lws2_32
//                  (use the POSIX thread model of MinGW-w64 for std::thread)
// Run:             ./chat_client 127.0.0.1 6000

#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include "net.hpp"

static std::atomic<bool> g_running{true};

// Background thread: keep receiving from the server and print it.
static void receive_loop(socket_t s) {
  char buf[1024];
  while (g_running) {
    long long n = net::recv_some(s, buf, sizeof buf);
    if (n <= 0) {                         // server closed or error
      std::cout << "\n[disconnected from server]\n";
      g_running = false;
      break;
    }
    std::cout.write(buf, static_cast<std::streamsize>(n));
    std::cout.flush();
  }
}

int main(int argc, char** argv) {
  const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
  const char* port = (argc > 2) ? argv[2] : "6000";
  try {
    net::Runtime runtime;
    net::Socket s = net::tcp_connect(host, port);
    std::cout << "Connected to " << host << ":" << port
              << ". Type a message, or /nick NAME, /who, /quit.\n";

    // Start the receive thread.
    std::thread reader(receive_loop, s.get());

    // Main thread: read keyboard lines and send them.
    std::string line;
    while (g_running && std::getline(std::cin, line)) {
      line += "\n";
      if (!net::send_all(s.get(), line.data(), line.size())) break;
      if (line == "/quit\n") break;
    }

    g_running = false;
    net::shutdown_send(s.get());   // wake the receive thread
    if (reader.joinable()) reader.join();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
