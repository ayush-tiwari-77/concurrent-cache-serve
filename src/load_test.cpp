#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static std::atomic<long> success_count{0};
static std::atomic<long> fail_count{0};

static void worker(int id, int port, int ops_per_thread) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fail_count += ops_per_thread;
        return;
    }
    char buf[256];
    for (int i = 0; i < ops_per_thread; ++i) {
        std::string key = "key" + std::to_string(id) + "_" + std::to_string(i);
        std::string val = "val" + std::to_string(i);

        std::string set_cmd = "SET " + key + " " + val + "\n";
        send(sock, set_cmd.c_str(), set_cmd.size(), 0);
        recv(sock, buf, sizeof(buf), 0);

        std::string get_cmd = "GET " + key + "\n";
        send(sock, get_cmd.c_str(), get_cmd.size(), 0);
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        buf[n > 0 ? n : 0] = '\0';
        std::string resp(buf);

        if (resp.find(val) != std::string::npos) success_count++;
        else fail_count++;
    }
    close(sock);
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::atoi(argv[1]) : 9090;
    int num_threads = (argc > 2) ? std::atoi(argv[2]) : 20;
    int ops = (argc > 3) ? std::atoi(argv[3]) : 50;

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker, i, port, ops);
    for (auto& t : threads) t.join();

    auto end = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(end - start).count();
    long total = (long)num_threads * ops;

    std::cout << "Concurrent clients : " << num_threads << "\n";
    std::cout << "Ops per client     : " << ops << " (SET+GET pairs)\n";
    std::cout << "Total ops          : " << total * 2 << "\n";
    std::cout << "Succeeded          : " << success_count << "\n";
    std::cout << "Failed             : " << fail_count << "\n";
    std::cout << "Time taken         : " << secs << " s\n";
    std::cout << "Throughput         : " << (total * 2 / secs) << " ops/sec\n";
    return 0;
}
