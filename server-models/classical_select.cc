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

#include "common.h"


void classical_select(const Config& config) {
    int fd = create_server_sock(config.host, config.port);

    while (true) {
        int client_fd = accept_connection(fd);
        serve_client(client_fd, config.file);
    }

    close(fd);
}
