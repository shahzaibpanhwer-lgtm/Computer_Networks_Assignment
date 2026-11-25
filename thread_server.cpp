// thread_server.cpp - Tutorial 8: one thread for each client (threaded echo server)
// Build (Linux): g++ -std=c++17 -Wall -Wextra -pthread -o thread_server thread_server.cpp
// Build (MinGW): g++ -std=c++17 -o thread_server.exe thread_server.cpp -lws2_32
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include "net.hpp"

static std::mutex g_log_mutex;          // one thread writes to the console at a time
static std::atomic<int> g_active{0};     // number of connected clients

static void log_line(const std::string& s) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::cout << s << std::endl;
}

// Each client runs this function in its own thread.
static void serve_client(net::Socket client, std::string who) {
  int n = ++g_active;
  log_line(who + " connected. Active clients: " + std::to_string(n));
  char buf[1024];
  for (;;) {
    long long r = net::recv_some(client.get(), buf, sizeof buf);
    if (r <= 0) break;                                   // closed or error
    if (!net::send_all(client.get(), buf, static_cast<size_t>(r))) break;
  }
  n = --g_active;
  log_line(who + " disconnected. Active clients: " + std::to_string(n));
  // client socket closes here (Socket destructor)
}

int main(int argc, char** argv) {
  const char* port = (argc > 1) ? argv[1] : "5000";
  try {
    net::Runtime runtime;
    net::Socket listener = net::tcp_listen(port);
    std::cout << "Threaded echo server on port " << port << "\n";
    for (;;) {
      sockaddr_storage peer{};
      sock_len peer_len = sizeof peer;
      net::Socket client(accept(listener.get(),
                         reinterpret_cast<sockaddr*>(&peer), &peer_len));
      if (!client.valid()) continue;
      std::string who = net::peer_name(reinterpret_cast<sockaddr*>(&peer), peer_len);
      // Give the socket to a new thread with std::move, then let the thread run alone.
      std::thread(serve_client, std::move(client), who).detach();
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
