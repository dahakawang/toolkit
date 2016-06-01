#include <iostream>
#include "common.h"

void classical_single(const Config& config);
void classical_fork(const Config& config);
void classical_thread(const Config& config);
void classical_thread_pool(const Config& config);

using namespace std;

Config parse(int argc, char *argv[]) {
    Config config;
    if (argc < 5) report("wrong usage");
    config.mode = argv[1];
    config.host = argv[2];
    config.port = atoi(argv[3]);
    config.file = argv[4];

    for (int i = 5; i < argc; i++) {
        string item = argv[i];

        if (item.find("-ps") == 0) {
            string value = item.substr(3);
            config.pool_size = atoi(value.c_str());
        } else {
            std::cerr << "Unknown option " << item << std::endl;
            exit(1);
        }
    }

    return config;
}

int main(int argc, char *argv[]) {
    Config config = parse(argc, argv);

    if (config.mode == "single") {
        classical_single(config);
     } else if (config.mode == "fork") {
        classical_fork(config);
     } else if (config.mode == "thread") {
        classical_thread(config);
     } else if (config.mode == "pool") {
        classical_thread_pool(config);    } else {
        std::cerr << "unknown mode " << config.mode << std::endl;
    }

    return 0;
}
