#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace lisp {
using ImmediateId = uint16_t;

using StackLoc = uint16_t;
using FrameDist = uint16_t;
struct VarLoc {
    FrameDist frameDist_;
    StackLoc offset_;
};

using Bytecode = std::vector<uint8_t>;
    
} // namespace lisp
