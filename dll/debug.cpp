#include "runtime/ebl.hpp"
#include "runtime/listBuilder.hpp"

using namespace ebl;

static struct {
    const char* name_;
    const char* docstring_;
    size_t argc_;
    ebl::CFunction impl_;
} exports[] = {
     {"addr", "(addr obj) -> address of obj", 1,
      [](Environment& env, const Arguments& args) -> ObjectPtr {
          return env.create<RawPointer>(args[0].handle());
      }},
     {"get-interns", "(get-interns) -> list of all values interned by the vm", 0,
      [](Environment& env, const Arguments&) {
          LazyListBuilder builder(env);
          for (auto& var : env.getContext()->immediates()) {
              builder.pushBack(var);
          }
          return builder.result();
      }},
     {"collect-garbage", "(collect-garbage) -> run the gc", 0,
      [](Environment& env, const Arguments&) -> ObjectPtr {
          env.getContext()->runGC(env);
          return env.getNull();
      }},
     {"memory-stats", "(memory-stats) -> (used-memory . remaining-memory)", 0,
      [](Environment& env, const Arguments&) -> ObjectPtr {
          const auto stat = env.getContext()->memoryStat();
          return env.create<Pair>(
              env.create<Integer>((Integer::Rep)stat.used_),
              env.create<Integer>((Integer::Rep)stat.remaining_));
      }},
     {"sizeof", "(sizeof obj) -> number of bytes that obj occupies in memory", 1,
      [](Environment& env, const Arguments& args) -> ObjectPtr {
          return env.create<Integer>(Integer::Rep(typeInfo(args[0]).size_));
      }}
};

extern "C" {
void __dllMain(ebl::Environment& env)
{
    for (const auto& exp : exports) {
        auto doc = env.create<ebl::String>(exp.docstring_, strlen(exp.docstring_));
        env.setGlobal(exp.name_, "debug",
                      env.create<ebl::Function>(doc, exp.argc_, exp.impl_));
    }
}
}
