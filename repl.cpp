#include "evaluator.hpp"
#include <iostream>

int main()
{
    lisp::Context context({});
    std::string input;
    auto env = context.topLevel();
    std::vector<lisp::ObjectPtr> printArg;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (not input.empty()) {
            try {
                auto result = eval(*env, input);
                auto print =
                    lisp::checkedCast<lisp::Function>(*env, env->load("print"));
                printArg = {result};
                print->call(printArg);
                std::cout << std::endl;
            } catch (const std::exception& ex) {
                std::cout << "Error: " << ex.what() << std::endl;
            }
        }
    }
}
