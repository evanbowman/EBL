#pragma once

#include <stddef.h>
#include <stdint.h>

namespace lisp {
using ImmediateId = uint32_t;

using StackLoc = size_t;
using FrameDist = size_t;
struct VarLoc {
    FrameDist frameDist_;
    StackLoc offset_;
};
} // namespace lisp
