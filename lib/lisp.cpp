#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "environment.hpp"
#include "extlib/optional.hpp"
#include "lexer.hpp"
#include "types.hpp"

#ifdef __GNUC__
#define unlikely(COND) __builtin_expect(COND, false)
#else
#define unlikely(COND) COND
#endif

#include <thread>

namespace lisp {

template <typename Proc> void dolist(ObjectPtr list, Proc&& proc)
{
    Heap::Ptr<Pair> current = checkedCast<Pair>(list);
    proc(current->getCar());
    while (not isType<Null>(current->getCdr())) {
        current = checkedCast<Pair>(current->getCdr());
        proc(current->getCar());
    }
}

void print(lisp::Environment& env, lisp::ObjectPtr obj, std::ostream& out)
{
    if (lisp::isType<lisp::Pair>(obj)) {
        auto pair = obj.cast<lisp::Pair>();
        out << "(";
        print(env, pair->getCar(), out);
        while (true) {
            if (isType<Pair>(pair->getCdr())) {
                out << " ";
                print(env, pair->getCdr().cast<Pair>()->getCar(), out);
                pair = pair->getCdr().cast<Pair>();
            } else if (not isType<Null>(pair->getCdr())) {
                out << " ";
                print(env, pair->getCdr(), out);
                break;
            } else {
                break;
            }
        }
        out << ")";
    } else if (lisp::isType<lisp::Integer>(obj)) {
        out << obj.cast<lisp::Integer>()->value();
    } else if (lisp::isType<lisp::Null>(obj)) {
        out << "null";
    } else if (obj == env.getBool(true)) {
        out << "true";
    } else if (obj == env.getBool(false)) {
        out << "false";
    } else if (lisp::isType<lisp::Function>(obj)) {
        out << "lambda<" << obj.cast<lisp::Function>()->argCount() << ">";
    } else if (lisp::isType<lisp::String>(obj)) {
        out << obj.cast<lisp::String>()->value();
    } else if (lisp::isType<lisp::Double>(obj)) {
        out << obj.cast<lisp::Double>()->value();
    } else if (lisp::isType<lisp::Complex>(obj)) {
        out << obj.cast<lisp::Complex>()->value();
    } else {
        out << "unknown object" << std::endl;
    }
}

class ListBuilder {
public:
    ListBuilder(Environment& env, ObjectPtr first)
        : env_(env), front_(env, env.create<Pair>(first, env.getNull())),
          back_(front_.get())
    {
    }

    void pushFront(ObjectPtr value)
    {
        front_ = env_.create<Pair>(value, front_.get());
    }

    void pushBack(ObjectPtr value)
    {
        auto next = env_.create<Pair>(value, env_.getNull());
        back_->setCdr(next);
        back_ = next;
    }

    ObjectPtr result()
    {
        return front_.get();
    }

private:
    Environment& env_;
    Local<Pair> front_;
    Heap::Ptr<Pair> back_;
};

// More overhead, but doesn't require an initial element like ListBuilder
class LazyListBuilder {
public:
    LazyListBuilder(Environment& env) : env_(env)
    {
    }

    void pushFront(ObjectPtr value)
    {
        push(value, [&](ListBuilder& builder) { builder.pushFront(value); });
    }
    void pushBack(ObjectPtr value)
    {
        push(value, [&](ListBuilder& builder) { builder.pushBack(value); });
    }

    ObjectPtr result()
    {
        if (builder_)
            return builder_->result();
        else
            return env_.getNull();
    }

private:
    template <typename F> void push(ObjectPtr value, F&& callback)
    {
        if (not builder_)
            builder_.emplace(env_, value);
        else
            callback(*builder_);
    }
    Environment& env_;
    nonstd::optional<ListBuilder> builder_;
};

struct BuiltinFunctionInfo {
    const char* name;
    const char* docstring;
    size_t requiredArgs;
    CFunction impl;
};

static const BuiltinFunctionInfo builtins[] = {
    {"cons", "[car cdr] -> create a pair from car and cdr", 2,
     [](Environment& env, const Arguments& args) {
         return env.create<Pair>(args[0], args[1]);
     }},
    {"list",
     "[args...] -> create a chain of pairs, where each arg in args..."
     " supplies the first element of each pair",
     0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         if (auto argc = args.size()) {
             ListBuilder builder(env, args[0]);
             for (size_t pos = 1; pos < argc; ++pos) {
                 builder.pushBack(args[pos]);
             }
             return builder.result();
         } else {
             return env.getNull();
         }
     }},
    {"car", "[pair] -> get the first element of pair", 1,
     [](Environment& env, const Arguments& args) {
         return checkedCast<Pair>(args[0])->getCar();
     }},
    {"cdr", "[pair] -> get the second element of pair", 1,
     [](Environment& env, const Arguments& args) {
         return checkedCast<Pair>(args[0])->getCdr();
     }},
    {"length", "[list] -> get the length of list", 1,
     [](Environment& env, const Arguments& args) {
         Integer::Rep length = 0;
         if (not isType<Null>(args[0])) {
             dolist(args[0], [&](ObjectPtr) { ++length; });
         }
         return env.create<Integer>(length);
     }},
    {"list-ref", "[list] -> get the nth element from list", 2,
     [](Environment& env, const Arguments& args) {
         const auto index = checkedCast<Integer>(args[1])->value();
         if (not isType<Null>(args[0])) {
             Heap::Ptr<Pair> current = checkedCast<Pair>(args[0]);
             if (index == 0) {
                 return current->getCar();
             }
             Integer::Rep i = 1;
             while (not isType<Null>(current->getCdr())) {
                 current = checkedCast<Pair>(current->getCdr());
                 if (i++ == index) {
                     return current->getCar();
                 }
             }
         }
         return env.getNull();
     }},
    {"map", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         if (not isType<Null>(args[1])) {
             auto fn = checkedCast<Function>(args[0]);
             // TODO: replace vectors with small size optimized containers
             Arguments paramVec;
             std::vector<Heap::Ptr<Pair>> inputLists;
             for (size_t i = 1; i < args.size(); ++i) {
                 inputLists.push_back(checkedCast<Pair>(args[i]));
                 paramVec.push_back(inputLists.back()->getCar());
             }
             auto keepGoing = [&] {
                 for (auto lst : inputLists) {
                     if (isType<Null>(lst->getCdr())) {
                         return false;
                     }
                 }
                 return true;
             };
             ListBuilder builder(env, fn->call(paramVec));
             while (keepGoing()) {
                 paramVec.clear();
                 for (auto& lst : inputLists) {
                     lst = checkedCast<Pair>(lst->getCdr());
                     paramVec.push_back(lst->getCar());
                 }
                 builder.pushBack(fn->call(paramVec));
             }
             return builder.result();
         }
         return env.getNull();
     }},
    {"filter", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         LazyListBuilder builder(env);
         auto pred = checkedCast<Function>(args[0]);
         if (not isType<Null>(args[1])) {
             Arguments params;
             dolist(args[1], [&](ObjectPtr element) {
                 params.push_back(element);
                 if (pred->call(params) == env.getBool(true)) {
                     builder.pushBack(element);
                 }
                 params.clear();
             });
         }
         return builder.result();
     }},
    {"dolist", "[fn list] -> call fn on each element of list", 2,
     [](Environment& env, const Arguments& args) {
         auto pred = checkedCast<Function>(args[0]);
         if (not isType<Null>(args[1])) {
             Arguments params;
             dolist(args[1], [&](ObjectPtr element) {
                 params.push_back(element);
                 pred->call(params);
                 params.clear();
             });
         }
         return env.getNull();
     }},
    {"dotimes", "[fn n] -> call fn n times, passing each n as an arg", 2,
     [](Environment& env, const Arguments& args) {
         auto pred = checkedCast<Function>(args[0]);
         const auto times = checkedCast<Integer>(args[1])->value();
         Arguments params;
         for (Integer::Rep i = 0; i < times; ++i) {
             auto iObj = env.create<Integer>(i);
             params.push_back(iObj);
             pred->call(params);
             params.clear();
         }
         return env.getNull();
     }},
    {"null?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Null>(args[0]));
     }},
    {"pair?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Pair>(args[0]));
     }},
    {"bool?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Boolean>(args[0]));
     }},
    {"integer?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Integer>(args[0]));
     }},
    {"identical?", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         return env.getBool(args[0] == args[1]);
     }},
    {"equal?", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         EqualTo eq;
         return env.getBool(eq(args[0], args[1]));
     }},
    {"not", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         // I was lazy, TODO simplify this
         if (args[0] == env.getBool(true)) {
             return env.getBool(false);
         } else if (args[0] == env.getBool(false)) {
             return env.getBool(true);
         }
         // TODO: raise error
         return env.getNull();
     }},
    {"identity", nullptr, 1,
     [](Environment& env, const Arguments& args) { return args[0]; }},
    {"apply", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         Arguments params;
         if (not isType<Null>(args[1])) {
             dolist(args[1], [&](ObjectPtr elem) { params.push_back(elem); });
         }
         auto fn = checkedCast<Function>(args[0]);
         return fn->call(params);
     }},
    {"incr", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         const auto prev = checkedCast<Integer>(args[0])->value();
         return env.create<Integer>(prev + 1);
     }},
    {"decr", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         const auto prev = checkedCast<Integer>(args[0])->value();
         return env.create<Integer>(prev - 1);
     }},
    {"max", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         auto result = checkedCast<Integer>(args[0]);
         for (size_t i = 0; i < args.size(); ++i) {
             auto current = checkedCast<Integer>(args[i]);
             if (current->value() > result->value()) {
                 result = current;
             }
         }
         return result;
     }},
    {"min", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         auto result = checkedCast<Integer>(args[0]);
         for (size_t i = 0; i < args.size(); ++i) {
             auto current = checkedCast<Integer>(args[i]);
             if (current->value() < result->value()) {
                 result = current;
             }
         }
         return result;
     }},
    {"time", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         Arguments params;
         if (not isType<Null>(args[1])) {
             dolist(args[1], [&](ObjectPtr elem) { params.push_back(elem); });
         }
         auto fn = checkedCast<Function>(args[0]);
         using namespace std::chrono;
         const auto start = steady_clock::now();
         auto result = fn->call(params);
         const auto stop = steady_clock::now();
         std::cout << duration_cast<nanoseconds>(stop - start).count()
                   << " nanoseconds" << std::endl;
         return result;
     }},
    {"sleep", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         const auto arg = checkedCast<Integer>(args[0])->value();
         std::this_thread::sleep_for(std::chrono::microseconds(arg));
         return env.getNull();
     }},
    {"help", "[fn] -> get the docstring for fn", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         auto fn = checkedCast<Function>(args[0]);
         auto doc = fn->getDocstring();
         if (not(doc == env.getNull())) {
             return doc;
         }
         return env.getNull();
     }},
    {"memory-statistics", nullptr, 0,
     [](Environment& env, const Arguments& args) {
         std::cout << "heap capacity: " << env.getHeap().capacity()
                   << ", heap used: " << env.getHeap().size() << " ("
                   << env.getHeap().capacity() - env.getHeap().size()
                   << " remaining)" << std::endl;
         return env.getNull();
     }},
    {"print", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         print(env, args[0], std::cout);
         return env.getNull();
     }},
    {"println", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         print(env, args[0], std::cout);
         std::cout << "\n";
         return env.getNull();
     }},
    // Temporary, until we have support for characters
    {"newline", nullptr, 0,
     [](Environment& env, const Arguments& args) {
         std::cout << "\n" << std::endl;
         return env.getNull();
     }},
    {"space", nullptr, 0,
     [](Environment& env, const Arguments& args) {
         std::cout << " " << std::endl;
         return env.getNull();
     }},
    {"+", nullptr, 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         Integer::Rep iSum = 0;
         Complex::Rep cSum;
         Double::Rep dSum = 0.0;
         for (size_t i = 0; i < args.size(); ++i) {
             switch (args[i]->typeId()) {
             case typeInfo.typeId<Integer>():
                 iSum += args[i].cast<Integer>()->value();
                 break;

             case typeInfo.typeId<Double>():
                 dSum += args[i].cast<Double>()->value();
                 break;

             case typeInfo.typeId<Complex>():
                 cSum += args[i].cast<Complex>()->value();
                 break;

             default:
                 throw TypeError(args[i]->typeId(), "not a number");
             }
         }
         if (cSum not_eq Complex::Rep{0.0, 0.0}) {
             return env.create<Complex>(cSum + dSum +
                                        static_cast<Double::Rep>(iSum));
         } else if (dSum) {
             return env.create<Double>(dSum + iSum);
         }
         return env.create<Integer>(iSum);
     }},
    {"-", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         // FIXME: this isn't as flexible as it should be
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.create<Integer>(args[0].cast<Integer>()->value() -
                                        checkedCast<Integer>(args[1])->value());

         case typeInfo.typeId<Double>():
             return env.create<Double>(args[0].cast<Double>()->value() -
                                       checkedCast<Double>(args[1])->value());

         case typeInfo.typeId<Complex>():
             return env.create<Complex>(args[0].cast<Complex>()->value() -
                                        checkedCast<Complex>(args[1])->value());
         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"*", nullptr, 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         Integer::Rep iProd = 1;
         Double::Rep dProd = 1.0;
         Complex::Rep cProd(1.0);
         for (size_t i = 0; i < args.size(); ++i) {
             switch (args[i]->typeId()) {
             case typeInfo.typeId<Integer>():
                 iProd *= args[i].cast<Integer>()->value();
                 break;

             case typeInfo.typeId<Double>():
                 dProd *= args[i].cast<Double>()->value();
                 break;

             case typeInfo.typeId<Complex>():
                 cProd *= args[i].cast<Complex>()->value();
                 break;
             }
         }
         if (cProd not_eq Complex::Rep{1.0}) {
             return env.create<Complex>(cProd * dProd *
                                        static_cast<Double::Rep>(iProd));
         } else if (dProd not_eq 1.0) {
             return env.create<Double>(dProd * iProd);
         }
         return env.create<Integer>(iProd);
     }},
    {"/", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         // FIXME: this isn't as flexible as it should be
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.create<Integer>(args[0].cast<Integer>()->value() /
                                        checkedCast<Integer>(args[1])->value());

         case typeInfo.typeId<Double>():
             return env.create<Double>(args[0].cast<Double>()->value() /
                                       checkedCast<Double>(args[1])->value());

         case typeInfo.typeId<Complex>():
             return env.create<Complex>(args[0].cast<Complex>()->value() /
                                        checkedCast<Complex>(args[1])->value());
         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {">", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.getBool(args[0].cast<Integer>()->value() >
                                checkedCast<Integer>(args[1])->value());

         case typeInfo.typeId<Double>():
             return env.getBool(args[0].cast<Double>()->value() >
                                checkedCast<Double>(args[1])->value());

         case typeInfo.typeId<Complex>():
             throw TypeError(typeInfo.typeId<Complex>(),
                             "Comparison unsupported for complex numbers. "
                             "Why not try comparing the magnitude?");

         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"<", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.getBool(args[0].cast<Integer>()->value() <
                                checkedCast<Integer>(args[1])->value());

         case typeInfo.typeId<Double>():
             return env.getBool(args[0].cast<Double>()->value() <
                                checkedCast<Double>(args[1])->value());

         case typeInfo.typeId<Complex>():
             throw TypeError(typeInfo.typeId<Complex>(),
                             "Comparison unsupported for complex numbers. "
                             "Why not try comparing the magnitude?");

         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"abs", nullptr, 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         auto inp = args[0];
         switch (inp->typeId()) {
         case typeInfo.typeId<Integer>():
             if (inp.cast<Integer>()->value() > 0) {
                 return inp;
             } else {
                 const auto result = inp.cast<Integer>()->value() * -1;
                 return env.create<Integer>(result);
             }
             break;

         case typeInfo.typeId<Double>():
             if (inp.cast<Double>()->value() > 0.0) {
                 return inp;
             } else {
                 const auto result = std::abs(inp.cast<Double>()->value());
                 return env.create<Double>(result);
             }
             break;

         case typeInfo.typeId<Complex>(): {
             const auto result = std::abs(inp.cast<Complex>()->value());
             return env.create<Double>(result);
         }

         default:
             throw TypeError(inp->typeId(), "not a number");
         }
     }},
    {"complex", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         const auto real = checkedCast<Double>(args[0])->value();
         const auto imag = checkedCast<Double>(args[1])->value();
         return env.create<Complex>(Complex::Rep(real, imag));
     }},
};

void initBuiltins(Environment& env)
{
    std::for_each(std::begin(builtins), std::end(builtins),
                  [&](const BuiltinFunctionInfo& info) {
                      auto fn = env.create<Function>(
                          info.docstring, info.requiredArgs, info.impl);
                      env.push(fn);
                  });
}

std::vector<std::string> getBuiltinList()
{
    std::vector<std::string> ret;
    std::for_each(
        std::begin(builtins), std::end(builtins),
        [&](const BuiltinFunctionInfo& info) { ret.push_back(info.name); });
    return ret;
}

} // namespace lisp
