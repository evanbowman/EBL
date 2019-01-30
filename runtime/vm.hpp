#pragma once

#include "common.hpp"
#include <memory>

namespace ebl {

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
    static void execute(Environment& env, const Bytecode& bc, size_t start);
};

} // namespace ebl
