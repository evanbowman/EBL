#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include "runtime/ebl.hpp"


int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cout << "usage: dofile <fname>" << std::endl;
    }
    ebl::Context context;
    auto& env = context.topLevel();
    env.openDLL("libfs");
    env.openDLL("libsys");
    try {
        context.loadFromFile("bc");
    } catch (const std::exception& ex) {
        std::cout << "Error:\n" << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
