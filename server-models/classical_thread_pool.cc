#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cassert>

#include "common.h"

using std::string;
using std::thread;


template<typename T>
class block_queue {
public:
    void pop(T& holder) {
        std::unique_lock<std::mutex> l(_m);
        _cv.wait(l, [this]{ return !_q.empty(); });
        holder = _q.front();
        _q.pop();
        --_size;
        l.unlock();
    }

    void push(const T& elem) {
        std::unique_lock<std::mutex> l(_m);
        _q.push(elem);
        ++_size;
        l.unlock();
        _cv.notify_all();
    }

    void size() {
        return _size;
    }

private:
    long long _size{0};
    std::queue<T> _q;
    std::mutex _m;
    std::condition_variable _cv;
};

class Worker {
public:
    Worker() {
        _t = std::thread([this](){
            this->run();
        });
    }

    void post(int fd) {
        _work.push(fd);
    }

    void set_file(const std::string& file_path) {
        file = file_path;
    }

private:
    block_queue<int> _work;
    std::thread _t;
    std::string file;

    void run() {

        while (true) {
            int fd = -1;
            _work.pop(fd);

            serve_client(fd, file);
        }
    }
};

void classical_thread_pool(const Config& config) {
    assert(config.pool_size != 0);

    int fd = create_server_sock(config.host, config.port);

    std::vector<Worker> workers(config.pool_size);
    for (auto& w : workers) w.set_file(config.file);

    int pos = 0;
    while (true) {
        int client_fd = accept_connection(fd);

        workers[pos].post(client_fd);
        pos = (pos + 1) % workers.size();
    }

    close(fd);
}
