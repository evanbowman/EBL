#include "lib/parser.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include "lib/lisp.hpp"


int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cout << "usage: dofile <fname>" << std::endl;
    }
    lisp::Context context({});
    auto env = context.topLevel();
    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();
    try {
        auto top = lisp::parse(buffer.str());
        top->serialize(std::cout, 0);
        top->execute(*env);
    } catch (const std::exception& ex) {
        std::cout << "Error: " << ex.what() << std::endl;
    }
}
