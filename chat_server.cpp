// chat_server.cpp - Assignment: multi-user chat server
// One thread + select(). Up to 60 clients. Portable (net.hpp only, no #ifdef here).
//
// Protocol (text lines, each ends with \n; \r\n also accepted):
//   <text>        -> broadcast as "name: <text>" to everyone else
//   /nick NAME    -> change name (1-16 chars: letters, digits, _; must be unique)
//   /who          -> list of connected user names
//   /quit         -> disconnect
//
// Build (Linux):   g++ -std=c++17 -Wall -Wextra -o chat_server chat_server.cpp
// Build (MinGW):   g++ -std=c++17 -o chat_server.exe chat_server.cpp -lws2_32
// Run:             ./chat_server 6000

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "net.hpp"

constexpr int    MAX_CLIENTS = 60;          // R1
constexpr size_t MAX_LINE    = 1024;        // design: max line length

struct Client {
  net::Socket sock;      // owns the handle (closed automatically)
  std::string name;      // default userN, or set with /nick
  std::string inbuf;     // partial data kept between recv() calls (R9)
};

// All connected clients, keyed by their raw socket value.
static std::map<socket_t, Client> g_clients;
static int g_name_counter = 0;   // gives user1, user2, ...

// Send one line (we add the \n) to a single client.
static void send_line(socket_t s, const std::string& line) {
  std::string out = line + "\n";
  net::send_all(s, out.data(), out.size());
}

// Send a line to everyone, optionally skipping one socket (the sender).
static void broadcast(const std::string& line, socket_t skip = INVALID_SOCK) {
  for (auto& kv : g_clients)
    if (kv.first != skip)
      send_line(kv.first, line);
}

// Check that a /nick name is valid and not already taken.
static bool name_ok(const std::string& name) {
  if (name.empty() || name.size() > 16) return false;
  for (char c : name)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
  for (auto& kv : g_clients)
    if (kv.second.name == name) return false;   // R5: no duplicate names
  return true;
}

// Handle one complete line from one client.
// Returns false if the client asked to quit (/quit) -> caller removes it.
static bool handle_line(Client& c, const std::string& line) {
  if (line.empty()) return true;               // ignore blank lines

  if (line[0] == '/') {                         // it is a command
    // split into first word (command) and the rest (argument)
    size_t sp = line.find(' ');
    std::string cmd = line.substr(0, sp);
    std::string arg = (sp == std::string::npos) ? "" : line.substr(sp + 1);

    if (cmd == "/quit") {                        // R7
      return false;
    }
    else if (cmd == "/who") {                    // R6
      std::string list = "Users:";
      for (auto& kv : g_clients) list += " " + kv.second.name;
      send_line(c.sock.get(), list);
    }
    else if (cmd == "/nick") {                    // R5
      if (name_ok(arg)) {
        std::string old = c.name;
        c.name = arg;
        broadcast(old + " is now " + c.name);     // R8-style system message
      } else {
        send_line(c.sock.get(), "Error: that name is in use or not valid");
      }
    }
    else {
      send_line(c.sock.get(), "Error: unknown command");
    }
  } else {                                        // normal chat text -> R4
    broadcast(c.name + ": " + line, c.sock.get());
  }
  return true;
}

int main(int argc, char** argv) {
  const char* port = (argc > 1) ? argv[1] : "6000";
  try {
    net::Runtime runtime;
    net::Socket listener = net::tcp_listen(port, MAX_CLIENTS);
    std::cout << "Chat server on port " << port << ". Press Ctrl+C to stop.\n";

    for (;;) {
      // ---- Step 1: build the read set (listener + every client) --------
      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(listener.get(), &readset);
      int maxfd = static_cast<int>(listener.get());
      for (auto& kv : g_clients) {
        FD_SET(kv.first, &readset);
        if (static_cast<int>(kv.first) > maxfd) maxfd = static_cast<int>(kv.first);
      }

      // ---- Step 2: wait for activity on any socket (R2) ----------------
      if (select(maxfd + 1, &readset, nullptr, nullptr, nullptr) < 0) {
        std::cerr << "select failed: " << net::error_text(net::last_error()) << "\n";
        break;
      }

      // ---- Step 3: a new client is waiting on the listener -------------
      if (FD_ISSET(listener.get(), &readset)) {
        sockaddr_storage peer{};
        sock_len peer_len = sizeof peer;
        net::Socket client(accept(listener.get(),
                           reinterpret_cast<sockaddr*>(&peer), &peer_len));
        if (client.valid()) {
          if (g_clients.size() >= MAX_CLIENTS) {          // R1 limit
            send_line(client.get(), "Error: server is full");
            // client socket closes here when it goes out of scope
          } else {
            socket_t fd = client.get();
            std::string name = "user" + std::to_string(++g_name_counter);  // R3
            send_line(fd, "Welcome " + name + ". Commands: /nick /who /quit");
            broadcast(name + " joined", fd);               // R8
            Client c;
            c.sock = std::move(client);
            c.name = name;
            g_clients.emplace(fd, std::move(c));
          }
        }
      }

      // ---- Step 4: read from the clients that have data ---------------
      std::vector<socket_t> to_remove;
      for (auto& kv : g_clients) {
        socket_t fd = kv.first;
        if (!FD_ISSET(fd, &readset)) continue;

        char buf[1024];
        long long n = net::recv_some(fd, buf, sizeof buf);
        if (n <= 0) {                        // 0 = closed, <0 = error
          to_remove.push_back(fd);
          continue;
        }
        kv.second.inbuf.append(buf, static_cast<size_t>(n));

        // Pull out every complete line. Keep the rest for next time (R9).
        bool quit = false;
        size_t pos;
        while ((pos = kv.second.inbuf.find('\n')) != std::string::npos) {
          std::string line = kv.second.inbuf.substr(0, pos);
          kv.second.inbuf.erase(0, pos + 1);
          if (!line.empty() && line.back() == '\r') line.pop_back();  // accept \r\n
          if (!handle_line(kv.second, line)) { quit = true; break; }
        }
        // Protect against a very long line with no newline.
        if (kv.second.inbuf.size() > MAX_LINE) {
          send_line(fd, "Error: line too long");
          quit = true;
        }
        if (quit) to_remove.push_back(fd);
      }

      // ---- Step 5: drop the clients that left, tell the others (R8) ---
      for (socket_t fd : to_remove) {
        auto it = g_clients.find(fd);
        if (it == g_clients.end()) continue;
        std::string name = it->second.name;
        g_clients.erase(it);                 // Socket destructor closes the handle
        broadcast(name + " left");
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
