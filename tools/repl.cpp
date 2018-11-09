#include "lib/lisp.hpp"
#include <iostream>


int main()
{
    lisp::Context context({});
    std::string input;
    auto env = context.topLevel();
    std::vector<lisp::ObjectPtr> printArgv;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (not input.empty()) {
            try {
                printArgv = { env->exec(input) };
                lisp::checkedCast<lisp::Function>(*env,
                                                  env->load("print"))
                    ->call(printArgv);
                std::cout << std::endl;
            } catch (const std::exception& ex) {
                std::cout << "Error: " << ex.what() << std::endl;
            }
        }
    }
}
