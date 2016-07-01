#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>

using std::string;

void print_layout() {
    std::fstream f("/proc/self/maps", std::ios::in);
    std::string line;

    while (getline(f, line)) {
        std::cout << line << std::endl;
    }
}

string get_input() {
    string cmd;
    std::cout << std::endl << "Please issue command: ";
    getline(std::cin, cmd);
    return cmd;
}

bool start_with(const string& str, const string& prefix) {
    if (str.size() < prefix.size()) return false;

    return memcmp(str.c_str(), prefix.c_str(), prefix.size()) == 0;
}

size_t parse_size(const string& str) {
    std::istringstream s(str);
    string dummy;
    size_t size = 0;
    string unit;
    s >> dummy >> size >> unit;
    if (unit.empty()) {
        return size;
    } else if (unit == "k" || unit == "K" || unit == "kb" || unit == "KB") {
        return size * 1024;
    } else if (unit == "m" || unit == "M" || unit == "mb" || unit == "MB" ) {
        return size * 1024 * 1024;
    } else if (unit == "g" || unit == "G" || unit == "gb" || unit == "GB") {
        return size * 1024 * 1024 * 1024;
    } else {
        std::cerr << "unknown unit " << unit<< std::endl;
        std::cerr << "default to " << size << " bytes" << std::endl;
        return size;
    }
}

void handle_command(const string& cmd) {
    if (start_with(cmd, "alloc")) {
        size_t size = parse_size(cmd);
        void* ptr = malloc(size);
        if (ptr == nullptr) {
            std::cerr << "failed to malloc" << std::endl;
        } else {
            std::cout << size << " bytes allocated at address " << ptr << std::endl;
        }
    } else if (cmd == "map") {
        print_layout();
    } else {
        std::cerr << "unknown command" << std::endl;
    }
}

int main(int argc, char *argv[]) {
    
    while (true) {
        string cmd = get_input();

        if (cmd == "exit") break;
        handle_command(cmd);
    }
    return 0;
}
