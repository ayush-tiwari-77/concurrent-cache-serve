#include "sharded_cache.h"
#include "thread_pool.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <atomic>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

static std::atomic<bool> g_running{true};
static int g_server_fd = -1;

void handle_sigint(int) {
    g_running = false;
    if (g_server_fd >= 0) shutdown(g_server_fd, SHUT_RDWR);
}

static std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> parts;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) parts.push_back(tok);
    return parts;
}

// Tiny text protocol:
//   SET <key> <value>  -> OK
//   GET <key>          -> <value>  or NULL
//   DEL <key>          -> OK / NOTFOUND
//   STATS              -> shard/hit/miss/eviction counters
static std::string process_command(ShardedCache& cache, const std::string& line) {
    auto parts = split(line);
    if (parts.empty()) return "ERR empty command\n";
    std::string cmd = parts[0];
    for (auto& c : cmd) c = toupper(c);

    if (cmd == "SET" && parts.size() >= 3) {
        cache.put(parts[1], parts[2]);
        return "OK\n";
    } else if (cmd == "GET" && parts.size() == 2) {
        auto v = cache.get(parts[1]);
        return v ? (*v + "\n") : "NULL\n";
    } else if (cmd == "DEL" && parts.size() == 2) {
        return cache.remove(parts[1]) ? "OK\n" : "NOTFOUND\n";
    } else if (cmd == "STATS") {
        return cache.stats_string() + "\n";
    }
    return "ERR unknown command\n";
}

static void handle_client(int client_fd, ShardedCache& cache) {
    char buf[4096];
    std::string leftover;
    while (true) {
        ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        leftover += buf;
        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string line = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::string response = process_command(cache, line);
            send(client_fd, response.c_str(), response.size(), 0);
        }
    }
    close(client_fd);
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::atoi(argv[1]) : 9090;
    size_t num_shards = 8;
    size_t capacity_per_shard = 2000;
    size_t pool_size = 8;

    ShardedCache cache(num_shards, capacity_per_shard);
    ThreadPool pool(pool_size);

    signal(SIGINT, handle_sigint);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }
    g_server_fd = server_fd;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(server_fd, 256) < 0) { perror("listen"); return 1; }

    std::cout << "Cache server listening on port " << port
              << " | shards=" << num_shards
              << " | worker_threads=" << pool_size << std::endl;

    while (g_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!g_running) break;
            continue;
        }
        pool.enqueue([client_fd, &cache] { handle_client(client_fd, cache); });
    }

    close(server_fd);
    std::cout << "Server shut down." << std::endl;
    return 0;
}
