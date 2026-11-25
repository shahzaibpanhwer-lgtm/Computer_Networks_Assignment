# Makefile - build every program on Linux (g++)
# Usage: make        (build all)
#        make clean  (remove binaries)
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
PTHREAD  = -pthread

PLAIN = daytime_server daytime_client daytime_server2 daytime_client2 \
        echo_server echo_client web_server frame_server frame_client select_server
THREADED = thread_server chat_server chat_client

all: $(PLAIN) $(THREADED)

$(PLAIN): %: %.cpp net.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(THREADED): %: %.cpp net.hpp
	$(CXX) $(CXXFLAGS) $(PTHREAD) -o $@ $<

clean:
	rm -f $(PLAIN) $(THREADED)
