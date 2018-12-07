#pragma once

#include "common.hpp"


namespace lisp {

class Environment;
    
class VM {
public:
    void execute(Environment& env, const Bytecode& bc, size_t start);
};
    
}
