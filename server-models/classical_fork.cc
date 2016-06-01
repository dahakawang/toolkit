
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

#include "common.h"

using std::string;

void handle(int) {
    if (waitpid(-1, NULL, WNOHANG) < 0) report("error waitpid");
}


void classical_fork(const Config& config) {
    int fd = create_server_sock(config.host, config.port);
    if (signal(SIGCHLD, handle) == SIG_ERR) report("failed to signal");


    while (true) {
        int client_fd = accept_connection(fd);

        pid_t pid = fork();
        if (pid > 0) {
            continue;
        } else if (pid == 0) {
            close(fd);
            serve_client(client_fd, config.file);
            exit(0);
        } else {
            report("error fork");
        }
    }

    close(fd);
}
