#include <iostream>
#include "common.h"

void classical_single(const Config& config);
void classical_fork(const Config& config);

using namespace std;

Config parse(int argc, char *argv[]) {
    Config config;
    if (argc < 5) report("wrong usage");
    config.mode = argv[1];
    config.host = argv[2];
    config.port = atoi(argv[3]);
    config.file = argv[4];

    return config;
}

int main(int argc, char *argv[]) {
    Config config = parse(argc, argv);

    if (config.mode == "single") {
        classical_single(config);
     } else if (config.mode == "fork") {
        classical_fork(config);
    } else {
        std::cerr << "unknown mode " << config.mode << std::endl;
    }

    return 0;
}
