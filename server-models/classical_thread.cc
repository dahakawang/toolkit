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

#include "common.h"

using std::string;
using std::thread;


void classical_thread(const Config& config) {
    int fd = create_server_sock(config.host, config.port);


    while (true) {
        int client_fd = accept_connection(fd);

        thread t([&config, client_fd]{
            serve_client(client_fd, config.file);
        });
        t.detach();
    }

    close(fd);
}
