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

template <typename Proc>
void dolist(Environment& env, ObjectPtr list, Proc&& proc)
{
    Heap::Ptr<Pair> current = checkedCast<Pair>(env, list);
    proc(current->getCar());
    while (not isType<Null>(env, current->getCdr())) {
        current = checkedCast<Pair>(env, current->getCdr());
        proc(current->getCar());
    }
}

void print(lisp::Environment& env, lisp::ObjectPtr obj, std::ostream& out)
{
    if (lisp::isType<lisp::Pair>(env, obj)) {
        auto pair = obj.cast<lisp::Pair>();
        out << "(";
        print(env, pair->getCar(), out);
        out << " . ";
        print(env, pair->getCdr(), out);
        out << ")";
    } else if (lisp::isType<lisp::Integer>(env, obj)) {
        out << obj.cast<lisp::Integer>()->value();
    } else if (lisp::isType<lisp::Null>(env, obj)) {
        out << "null";
    } else if (obj == env.getBool(true)) {
        out << "#t";
    } else if (obj == env.getBool(false)) {
        out << "#f";
    } else if (lisp::isType<lisp::Function>(env, obj)) {
        out << "lambda<" << obj.cast<lisp::Function>()->argCount() << ">";
    } else if (lisp::isType<lisp::String>(env, obj)) {
        out << '\"' << obj.cast<lisp::String>()->value() << '\"';
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

static const struct BuiltinFunctionInfo {
    const char* name;
    const char* docstring;
    Arguments::Count requiredArgs;
    CFunction impl;
} builtins[] = {
    {"cons", "(cons CAR CDR)", 2,
     [](Environment& env, const Arguments& args) {
         return env.create<Pair>(args[0], args[1]);
     }},
    {"list", nullptr, 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         if (auto argc = args.count()) {
             ListBuilder builder(env, args[0]);
             for (Arguments::Count pos = 1; pos < argc; ++pos) {
                 builder.pushBack(args[pos]);
             }
             return builder.result();
         } else {
             return env.getNull();
         }
     }},
    {"car", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return checkedCast<Pair>(env, args[0])->getCar();
     }},
    {"cdr", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return checkedCast<Pair>(env, args[0])->getCdr();
     }},
    {"length", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         Integer::Rep length = 0;
         if (not isType<Null>(env, args[0])) {
             dolist(env, args[0], [&](ObjectPtr) { ++length; });
         }
         return env.create<Integer>(length);
     }},
    {"list-ref", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         const auto index = checkedCast<Integer>(env, args[1])->value();
         if (not isType<Null>(env, args[0])) {
             Heap::Ptr<Pair> current = checkedCast<Pair>(env, args[0]);
             if (index == 0) {
                 return current->getCar();
             }
             Integer::Rep i = 1;
             while (not isType<Null>(env, current->getCdr())) {
                 current = checkedCast<Pair>(env, current->getCdr());
                 if (i++ == index) {
                     return current->getCar();
                 }
             }
         }
         return env.getNull();
     }},
    {"map", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         if (not isType<Null>(env, args[1])) {
             auto fn = checkedCast<Function>(env, args[0]);
             // TODO: replace vectors with small size optimized containers
             std::vector<ObjectPtr> paramVec;
             std::vector<Heap::Ptr<Pair>> inputLists;
             for (Arguments::Count i = 1; i < args.count(); ++i) {
                 inputLists.push_back(checkedCast<Pair>(env, args[i]));
                 paramVec.push_back(inputLists.back()->getCar());
             }
             auto keepGoing = [&] {
                 for (auto lst : inputLists) {
                     if (isType<Null>(env, lst->getCdr())) {
                         return false;
                     }
                 }
                 return true;
             };
             ListBuilder builder(env, fn->call(paramVec));
             while (keepGoing()) {
                 paramVec.clear();
                 for (auto& lst : inputLists) {
                     lst = checkedCast<Pair>(env, lst->getCdr());
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
         auto pred = checkedCast<Function>(env, args[0]);
         if (not isType<Null>(env, args[1])) {
             std::vector<ObjectPtr> params;
             dolist(env, args[1], [&](ObjectPtr element) {
                 params = {element};
                 if (pred->call(params) == env.getBool(true)) {
                     builder.pushBack(element);
                 }
             });
         }
         return builder.result();
     }},
    {"null?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Null>(env, args[0]));
     }},
    {"pair?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Pair>(env, args[0]));
     }},
    {"bool?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Boolean>(env, args[0]));
     }},
    {"integer?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Integer>(env, args[0]));
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
         std::vector<ObjectPtr> params;
         if (not isType<Null>(env, args[1])) {
             dolist(env, args[1],
                    [&](ObjectPtr elem) { params.push_back(elem); });
         }
         auto fn = checkedCast<Function>(env, args[0]);
         return fn->call(params);
     }},
    {"memoize!", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         checkedCast<Function>(env, args[0])->memoize();
         return env.getNull();
     }},
    {"unmemoize!", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         checkedCast<Function>(env, args[0])->unmemoize();
         return env.getNull();
     }},
    {"memoized?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(checkedCast<Function>(env, args[0])->isMemoized());
     }},
    {"incr", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         const auto prev = checkedCast<Integer>(env, args[0])->value();
         return env.create<Integer>(prev + 1);
     }},
    {"decr", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         const auto prev = checkedCast<Integer>(env, args[0])->value();
         return env.create<Integer>(prev - 1);
     }},
    {"abs", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         auto val = checkedCast<Integer>(env, args[0]);
         if (val->value() > 0) {
             return val;
         } else {
             return env.create<Integer>(val->value() * -1);
         }
     }},
    {"+", nullptr, 0,
     [](Environment& env, const Arguments& args) {
         Integer::Rep result = 0;
         for (Arguments::Count i = 0; i < args.count(); ++i) {
             result += checkedCast<Integer>(env, args[i])->value();
         }
         return env.create<Integer>(result);
     }},
    {"-", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         auto lhs = checkedCast<Integer>(env, args[0]);
         auto rhs = checkedCast<Integer>(env, args[1]);
         return env.create<Integer>(lhs->value() - rhs->value());
     }},
    {"*", nullptr, 0,
     [](Environment& env, const Arguments& args) {
         Integer::Rep result = 1;
         for (Arguments::Count i = 0; i < args.count(); ++i) {
             result *= checkedCast<Integer>(env, args[i])->value();
         }
         return env.create<Integer>(result);
     }},
    {"max", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         auto result = checkedCast<Integer>(env, args[0]);
         for (Arguments::Count i = 0; i < args.count(); ++i) {
             auto current = checkedCast<Integer>(env, args[i]);
             if (current->value() > result->value()) {
                 result = current;
             }
         }
         return result;
     }},
    {"min", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         auto result = checkedCast<Integer>(env, args[0]);
         for (Arguments::Count i = 0; i < args.count(); ++i) {
             auto current = checkedCast<Integer>(env, args[i]);
             if (current->value() < result->value()) {
                 result = current;
             }
         }
         return result;
     }},
    {"time", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         std::vector<ObjectPtr> params;
         if (not isType<Null>(env, args[1])) {
             dolist(env, args[1],
                    [&](ObjectPtr elem) { params.push_back(elem); });
         }
         auto fn = checkedCast<Function>(env, args[0]);
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
         const auto arg = checkedCast<Integer>(env, args[0])->value();
         std::this_thread::sleep_for(std::chrono::microseconds(arg));
         return env.getNull();
     }},
    {"help", nullptr, 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         auto fn = checkedCast<Function>(env, args[0]);
         if (auto doc = fn->getDocstring()) {
             std::cout << doc << std::endl;
         }
         return env.getNull();
     }},
    {">", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         auto lhs = checkedCast<Integer>(env, args[0]);
         auto rhs = checkedCast<Integer>(env, args[1]);
         return env.getBool(lhs->value() > rhs->value());
     }},
    {"<", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         auto lhs = checkedCast<Integer>(env, args[0]);
         auto rhs = checkedCast<Integer>(env, args[1]);
         return env.getBool(lhs->value() < rhs->value());
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

};

void Environment::store(const std::string& key, ObjectPtr value)
{
    for (auto& binding : vars_) {
        if (binding.first == key) {
            binding.second = value;
            return;
        }
    }
    vars_.push_back({key, value});
}

ObjectPtr Environment::load(const std::string& key)
{
    for (auto& binding : vars_) {
        if (binding.first == key) {
            return binding.second;
        }
    }
    if (parent_) {
        // TODO: Need to create visibility barrier for function calls.
        // e.g.: With this implementation, a function can reference variables
        // in the caller's environment. Upwards visibility is necessary for
        // some special forms (like let bindings), but needs to be prevented
        // for function calls.
        return parent_->load(key);
    }
    throw std::runtime_error("binding for \'" + key + "\' does not exist");
}

ObjectPtr Environment::getNull()
{
    return context_->nullValue_.get();
}

ObjectPtr Environment::getBool(bool trueOrFalse)
{
    return context_->booleans_[trueOrFalse].get();
}

EnvPtr Environment::derive()
{
    return std::make_shared<Environment>(context_, reference());
}

EnvPtr Environment::parent()
{
    return parent_;
}

EnvPtr Environment::reference()
{
    return shared_from_this();
}

Context::Context(const Configuration& config)
    : heap_(1000000), topLevel_(std::make_shared<Environment>(this, nullptr)),
      booleans_{{topLevel_->create<Boolean>(false)},
                {topLevel_->create<Boolean>(true)}},
      nullValue_{topLevel_->create<Null>()}
{
    std::for_each(std::begin(builtins), std::end(builtins),
                  [&](const BuiltinFunctionInfo& info) {
                      auto fn = topLevel_->create<Function>(
                          info.docstring, info.requiredArgs, info.impl);
                      topLevel_->store(info.name, fn);
                  });
}

std::shared_ptr<Environment> Context::topLevel()
{
    return topLevel_;
}

const Heap& Environment::getHeap() const
{
    return context_->heap_;
}

} // namespace lisp
