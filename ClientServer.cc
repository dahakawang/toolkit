#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstdio>

using namespace std;


void report(const char* msg) {
    perror(msg);
    exit(1);
}

void server(const char* interface, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) report("socket failed");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, interface, &addr.sin_addr.s_addr) != 1) report("inet_pton failed");

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        report("bind failed");
    }

    if (listen(sock, SOMAXCONN) < 0) {
        report("listen failed");
    }

   struct sockaddr_in client_addr;
   memset(&client_addr, 0, sizeof(client_addr));
   socklen_t len = 0;
   int client_sock;
   client_sock = accept(sock, (sockaddr*) &client_addr, &len);
   if (client_sock < 0) {
       report("accept failed");
   }

   char buffer[1024];
   int bytes_read = 0, bytes_written = 0;
   for (int i = 0; i < 3; ++i) {
       bytes_read = read(client_sock, buffer, sizeof(buffer));
       if (bytes_read < 0) report("failed to read");
        if (bytes_read == 0) break;

       bytes_written = write(client_sock, buffer, bytes_read);
       if (bytes_written < 0) report("failed to write");
   }

   close(client_sock);
   close(sock);
}

void client(const char* url, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, url, &addr.sin_addr.s_addr) != 1) report("inet_pton failed");

    if (connect(sock, (sockaddr*) &addr, sizeof(addr)) < 0) {
        report("connect faile");
    }

    char buffer[1024];
    int bytes_write = 0, bytes_read = 0;
    while (fgets(buffer, sizeof(buffer), stdin)) {
        if (strcmp(buffer, "EOF") == 0) break;
        bytes_write = write(sock, buffer, strlen(buffer) + 1);
        if (bytes_write < 0) report("failed write");

        bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read < 0) report("failed read");

        std::cout << "echo: " << buffer << std::endl;
    }
    bytes_read = read(sock, buffer, sizeof(buffer));
    std::cout << bytes_read << std::endl;

    close(sock);
    
}

int main(int argc, char *argv[]) {
    if (argc != 4)  {
        std::cerr << "wrong usage" << std::endl;
        return 1;
    }

    string mode = argv[1];
    string url = argv[2];
    int port = atoi(argv[3]);

    if (mode == "-client") {
        client(url.c_str(), port);
    } else if (mode == "-server") {
        server(url.c_str(), port);
    } else {
        std::cerr << "invalid mode " << mode << std::endl;
    }
    return 0;
}
