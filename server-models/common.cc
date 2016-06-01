#include "common.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <sstream>
#include <iostream>

void report(const char* msg) {
    perror(msg);
    exit(1);
}


void read_file(const char* file, char* buffer, int size) {
    int file_fd = open(file, O_RDONLY, 0);
    if (file_fd < 0) report("failed open file");

    int len = read(file_fd, buffer, size);
    if (len < 0) report("failed read file");
    buffer[len] = 0;
    close(file_fd);
}

int assemble_http(char* buffer, const char* html) {
    int len = strlen(html);
    std::stringstream ss;
    ss << "HTTP/1.1 200 OK\r\n";
    ss << "Content-Length: " << len << "\r\n";
    ss << "\r\n" << html;

    strcpy(buffer, ss.str().c_str());
    return ss.str().size();
}

int create_server_sock(const string& url, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) report("socket failed");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, url.c_str(), (sockaddr*)&addr.sin_addr.s_addr) != 1) report("inet_pton failed");

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) report("failed bind");
    if (listen(fd, SOMAXCONN) < 0) report("listen failed");

    return fd;
}

int accept_connection(int fd) {
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(fd, (sockaddr*)&client_addr, &len);
    if (client_fd < 0) report("accept failed");

    return client_fd;
}

void serve_client(int client_fd, string file) {
    char buffer[1024];
    int bytes_read = read(client_fd, buffer, sizeof(buffer));
    if (bytes_read < 0) report("failed read");
    if (bytes_read == 0) {
        std::cerr << "unexpected closed the socket"  << std::endl;
        close(client_fd);
        return;
    } 

    read_file(file.c_str(), buffer, sizeof(buffer));
    char http_buffer[1024];
    int length = assemble_http(http_buffer, buffer);
    int bytes_write = write(client_fd, http_buffer, length);
    if (bytes_write < 0) report("failed write");

    close(client_fd);
}
