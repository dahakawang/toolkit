#pragma once 

#include <string>

using std::string;

class Config {
public:
    int port;
    string host;
    string file;
    string mode;
};

void report(const char* msg);
void read_file(const char* file, char* buffer, int size);
int assemble_http(char* buffer, const char* html);
int create_server_sock(const string& url, int port);
int accept_connection(int fd);
void serve_client(int client_fd, string file);
