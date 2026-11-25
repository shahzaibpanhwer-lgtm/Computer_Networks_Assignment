# Computer Networks – Assignment 1

Network programming in C++ (portable: Windows + Linux). All programs share one
header, `net.hpp`, so the application files contain no `#ifdef`.

## Contents

| File | Tutorial | What it is |
|------|----------|------------|
| `net.hpp` | 3, 6 | Portable socket layer (RAII sockets, send_all/recv_all, framing) |
| `daytime_server.cpp` / `daytime_client.cpp` | 1, 2 | Daytime service (raw, with `#ifdef`) |
| `daytime_server2.cpp` / `daytime_client2.cpp` | 3 | Same, rewritten with `net.hpp` |
| `echo_server.cpp` / `echo_client.cpp` | 4 | TCP echo (one client at a time) |
| `web_server.cpp` | 5 | Static HTTP/1.0 web server (`www/` folder) |
| `frame_server.cpp` / `frame_client.cpp` | 6 | Length-prefixed message framing |
| `thread_server.cpp` | 8 | Threaded echo server (one thread per client) |
| `select_server.cpp` | 9 | `select()` echo server (one thread, many clients) |
| `chat_server.cpp` / `chat_client.cpp` | Assignment | Multi-user chat (R1–R11) |

## Build

### Windows (MinGW g++)
Open a terminal in this folder and run:
```
build.bat
```
Or build one program:
```
g++ -std=c++17 -Wall -o chat_server.exe chat_server.cpp -lws2_32
```
> MinGW needs `-lws2_32` explicitly. For `std::thread` (chat_client, thread_server)
> use a **POSIX-thread** MinGW-w64 build.

### Linux (g++)
```
make
```

## Run and test

### Daytime (Tutorials 1–3)
```
daytime_server 1313
daytime_client 127.0.0.1 1313
```

### Echo (Tutorial 4)
```
echo_server 5000
echo_client 127.0.0.1 5000
```

### Web server (Tutorial 5) – run from this folder so it finds www/
```
web_server 8080 www
```
Then open http://localhost:8080/ , or:
```
curl -i http://localhost:8080/
curl -i http://localhost:8080/missing.html          # 404
curl -i -X POST http://localhost:8080/               # 405
curl -i --path-as-is "http://localhost:8080/../net.hpp"   # 400 (safe)
```

### Framing (Tutorial 6)
```
frame_server 5001
frame_client 127.0.0.1 5001 one two three
```

### Threaded / select echo (Tutorials 8–9)
```
thread_server 5000      (or)   select_server 5000
echo_client 127.0.0.1 5000     # start several clients
```

### Chat (Assignment)
Terminal 1:
```
chat_server 6000
```
Terminals 2, 3, ...:
```
chat_client 127.0.0.1 6000
```
Commands: type text to chat, `/nick NAME`, `/who`, `/quit`.

## Chat requirements covered

- **R1** up to 60 clients  — **R2** one thread + `select()`  — **R3** default names `userN`
- **R4** broadcast with sender name  — **R5** `/nick` (1–16 chars, unique)  — **R6** `/who`
- **R7** `/quit`  — **R8** join/leave/rename messages  — **R9** partial/merged lines (per-client buffer)
- **R10** client uses two threads (incoming shows while typing)  — **R11** `net.hpp` only, no `#ifdef` in app files

### R9 split-line test (Linux / WSL / Git Bash)
```
nc 127.0.0.1 6000                                   # terminal B (watch)
(printf 'Hel'; sleep 2; printf 'lo\n') | nc 127.0.0.1 6000   # terminal A
```
Terminal B shows `user2: Hello` only after the second part arrives.

## Note

No encryption and no login. Do not expose these servers on the public Internet.

## Bonus: crash / DoS test (`crash_test.py`)

The provided `web_server` and `daytime_server` serve **one client at a time**
(single thread). `crash_test.py` abuses that to take the web server down.

```
python crash_test.py slowloris 127.0.0.1 8080     # hold the server with one slow connection
python crash_test.py flood     127.0.0.1 1313 --count 500   # overflow the accept backlog
```

**Slowloris result:** while the script holds one connection and dribbles a byte
every few seconds, `read_head()` keeps waiting and the server can serve nobody
else. A normal request then times out:

```
baseline:      status=200  time=0.002s
during attack: status=000  time=5.0s   (web server unreachable = DoS)
```

Educational use only — run it against your own server on localhost.
