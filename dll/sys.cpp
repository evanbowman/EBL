#include "lib/lisp.hpp"
#include <iostream>
#include <stdio.h>

extern "C" {
void __dllMain(lisp::Environment& env)
{
    env.setGlobal("stdin", "sys", env.create<lisp::RawPointer>(stdin));
    env.setGlobal("stdout", "sys", env.create<lisp::RawPointer>(stdout));
    env.setGlobal("stderr", "sys", env.create<lisp::RawPointer>(stderr));
}
}
