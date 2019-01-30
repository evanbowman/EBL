#include "runtime/ebl.hpp"
#include <iostream>
#include <stdio.h>

extern "C" {
void __dllMain(ebl::Environment& env)
{
    env.setGlobal("stdin", "sys", env.create<ebl::RawPointer>(stdin));
    env.setGlobal("stdout", "sys", env.create<ebl::RawPointer>(stdout));
    env.setGlobal("stderr", "sys", env.create<ebl::RawPointer>(stderr));
}
}
