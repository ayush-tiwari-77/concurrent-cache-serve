#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::atoi(argv[1]) : 9090;
    const char* host = (argc > 2) ? argv[2] : "127.0.0.1";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    std::cout << "Connected to cache server at " << host << ":" << port << "\n";
    std::cout << "Commands: SET k v | GET k | DEL k | STATS | exit\n";
    std::string line;
    char buf[4096];
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
        line += "\n";
        send(sock, line.c_str(), line.size(), 0);
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        std::cout << buf;
    }
    close(sock);
    return 0;
}
