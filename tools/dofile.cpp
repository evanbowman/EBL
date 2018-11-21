#include "lib/parser.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include "lib/lisp.hpp"


int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cout << "usage: dofile <fname>" << std::endl;
    }
    lisp::Context context;
    auto env = context.topLevel();
    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();
    try {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        env->exec(buffer.str());
        auto stop = high_resolution_clock::now();
        std::cout << "\nexecution finished in "
                  << duration_cast<nanoseconds>(stop - start).count()
                  << "ns" << ", ("
                  << duration_cast<seconds>(stop - start).count() << "s)"
                  << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
