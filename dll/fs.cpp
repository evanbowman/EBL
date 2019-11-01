#include "runtime/ebl.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
// the c libraries deal with pointers, which are easier to wrap with the lisp
// runtime.
#include <stdio.h>

static struct {
    const char* name_;
    size_t argc_;
    const char* docstring_;
    ebl::CFunction impl_;
} exports[] = {
     {"open", 3, "(open filename mode callback) -> result of invoking callback on opened file",
     [](ebl::Environment& env, const ebl::Arguments& args) {
         ebl::Arguments callbackArgs(env);
         const auto fname = ebl::checkedCast<ebl::String>(args[0])->toAscii();
         const auto mode = ebl::checkedCast<ebl::String>(args[1])->toAscii();
         auto file = fopen(fname.c_str(), mode.c_str());
         callbackArgs.push(env.create<ebl::RawPointer>((void*)file));
         auto result = ebl::checkedCast<ebl::Function>(args[2])->call(callbackArgs);
         fclose(file);
         return result;
     }},
     {"slurp", 1, "(slurp file-name) -> string containing entire file",
      [](ebl::Environment& env, const ebl::Arguments& args) -> ebl::ValuePtr {
          std::ifstream t(ebl::checkedCast<ebl::String>(args[0])->toAscii());
          std::stringstream buffer;
          buffer << t.rdbuf();
          auto result = buffer.str();
          return env.create<ebl::String>(result.c_str(), result.length());
      }},
    {"getline", 1, "(getline file) -> string containing next line in the file",
     [](ebl::Environment& env, const ebl::Arguments& args) -> ebl::ValuePtr {
         char* line = nullptr;
         size_t cap = 0;
         ssize_t len;
         auto file = ebl::checkedCast<ebl::RawPointer>(args[0])->value();
         if ((len = getline(&line, &cap, (FILE*)file)) > 0) {
             if (line[len - 1] == '\n') {
                 len -= 1;
             }
             return env.create<ebl::String>(line, (size_t)len);
         } else {
             return env.getBool(false);
         }
         free(line);
         return env.getNull();
     }},
     {"write", 1, "(write file obj ...) -> write representations of objects to file",
      [](ebl::Environment& env, const ebl::Arguments& args) -> ebl::ValuePtr {
          auto file = ebl::checkedCast<ebl::RawPointer>(args[0])->value();
          std::stringstream format;
          for (size_t i = 1; i < args.count(); ++i) {
              ebl::print(env, args[i], format, false);
          }
          const auto result = format.str();
          fwrite(result.c_str(), result.size(), 1, (FILE*)file);
          return env.getNull();
      }},
};

extern "C" {
void __dllMain(ebl::Environment& env)
{
    for (const auto& exp : exports) {
        auto doc = env.create<ebl::String>(exp.docstring_, strlen(exp.docstring_));
        env.setGlobal(exp.name_, "fs",
                      env.create<ebl::Function>(doc, exp.argc_, exp.impl_));
    }
}
}
