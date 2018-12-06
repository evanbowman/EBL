#include "lib/lisp.hpp"
#include <iostream>
// the c libraries deal with pointers, which are easier to wrap with the lisp
// runtime.
#include <stdio.h>

static struct {
    const char* name_;
    size_t argc_;
    const char* docstring_;
    lisp::CFunction impl_;
} exports[] = {
     {"open", 3, "[filename mode callback] -> result of invoking callback on opened file",
     [](lisp::Environment& env, const lisp::Arguments& args) {
         lisp::Arguments callbackArgs(env);
         const auto fname = lisp::checkedCast<lisp::String>(args[0])->toAscii();
         const auto mode = lisp::checkedCast<lisp::String>(args[1])->toAscii();
         auto file = fopen(fname.c_str(), mode.c_str());
         callbackArgs.push(env.create<lisp::RawPointer>((void*)file));
         auto result = lisp::checkedCast<lisp::Function>(args[2])->call(callbackArgs);
         fclose(file);
         return result;
     }},
    {"get-line", 1, "[file] -> string containing next line in the file",
     [](lisp::Environment& env, const lisp::Arguments& args) -> lisp::ObjectPtr {
         char* line = nullptr;
         size_t cap = 0;
         ssize_t len;
         auto file = lisp::checkedCast<lisp::RawPointer>(args[0])->value();
         if ((len = getline(&line, &cap, (FILE*)file)) > 0) {
             if (line[len - 1] == '\n') {
                 len -= 1;
             }
             return env.create<lisp::String>(line, (size_t)len);
         } else {
             return env.getBool(false);
         }
         free(line);
         return env.getNull();
     }}
};

extern "C" {
void __dllMain(lisp::Environment& env)
{
    for (const auto& exp : exports) {
        auto doc = env.create<lisp::String>(exp.docstring_, strlen(exp.docstring_));
        env.setGlobal(exp.name_, "fs",
                      env.create<lisp::Function>(doc, exp.argc_, exp.impl_));
    }
}
}
