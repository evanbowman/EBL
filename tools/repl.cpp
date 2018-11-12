#include "lib/lisp.hpp"
#include <iostream>


int main()
{
    using namespace lisp;
    Context context({});
    std::string input;
    auto env = context.topLevel();
    Arguments printArgv;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (not input.empty()) {
            try {
                printArgv.push_back(env->exec(input));
                checkedCast<Function>(env->load("print"))->call(printArgv);
                std::cout << std::endl;
                printArgv.clear();
            } catch (const std::exception& ex) {
                std::cout << "Error: " << ex.what() << std::endl;
            }
        }
    }
}
