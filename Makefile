CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -pthread -Iinclude

all: server client load_test

server: src/server.cpp
	$(CXX) $(CXXFLAGS) src/server.cpp -o server

client: src/client.cpp
	$(CXX) $(CXXFLAGS) src/client.cpp -o client

load_test: src/load_test.cpp
	$(CXX) $(CXXFLAGS) src/load_test.cpp -o load_test

clean:
	rm -f server client load_test
