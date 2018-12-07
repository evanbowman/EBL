#pragma once

#include "common.hpp"


namespace lisp {

class Environment;
using EnvPtr = std::shared_ptr<Environment>;
using InstructionAddress = size_t;

struct StackFrame {
    InstructionAddress returnAddress_;
    InstructionAddress functionTop_;
    EnvPtr env_;
};

class VM {
public:
    void execute(Environment& env, const Bytecode& bc, size_t start);
};

} // namespace lisp
