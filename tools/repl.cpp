#include "lib/lisp.hpp"
#include <iostream>


int main(int argc, char** argv)
{
    using namespace lisp;
    Context context;
    std::string input;
    auto env = context.topLevel();
    env->setGlobal("quit", env->create<Function>(env->getNull(), size_t(0),
                                                 [](Environment& env, const Arguments&) {
                                                     exit(0);
                                                     return env.getNull();
                                                 }));
    if (argc == 2) {
        env->exec(argv[1]);
    }
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (not input.empty()) {
            try {
                Arguments printArgv(*env);
                printArgv.push(env->exec(input));
                checkedCast<Function>(env->getGlobal("print"))->call(printArgv);
                std::cout << std::endl;
            } catch (const std::exception& ex) {
                std::cout << "Error: " << ex.what() << std::endl;
            }
        }
    }
}
