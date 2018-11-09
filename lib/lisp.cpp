#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "builtin.hpp"
#include "environment.hpp"
#include "extlib/optional.hpp"
#include "lexer.hpp"
#include "parser.hpp"
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
        out << "true";
    } else if (obj == env.getBool(false)) {
        out << "false";
    } else if (lisp::isType<lisp::Function>(env, obj)) {
        out << "lambda<" << obj.cast<lisp::Function>()->argCount() << ">";
    } else if (lisp::isType<lisp::String>(env, obj)) {
        out << obj.cast<lisp::String>()->value();
    } else if (lisp::isType<lisp::Double>(env, obj)) {
        out << obj.cast<lisp::Double>()->value();
    } else if (lisp::isType<lisp::Complex>(env, obj)) {
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
    Arguments::Count requiredArgs;
    CFunction impl;
};

template <size_t size>
void initBuiltins(Environment& env, const BuiltinFunctionInfo (&builtins)[size])
{
    std::for_each(std::begin(builtins), std::end(builtins),
                  [&](const BuiltinFunctionInfo& info) {
                      auto fn = env.create<Function>(
                          info.docstring, info.requiredArgs, info.impl);
                      env.store(fn);
                  });
}

static const BuiltinFunctionInfo builtins[] = {
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
    {"dolist", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         auto pred = checkedCast<Function>(env, args[0]);
         if (not isType<Null>(env, args[1])) {
             std::vector<ObjectPtr> params;
             dolist(env, args[1], [&](ObjectPtr element) {
                 params = {element};
                 pred->call(params);
             });
         }
         return env.getNull();
     }},
    {"dotimes", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         auto pred = checkedCast<Function>(env, args[0]);
         const auto times = checkedCast<Integer>(env, args[1])->value();
         std::vector<ObjectPtr> params;
         for (Integer::Rep i = 0; i < times; ++i) {
             auto iObj = env.create<Integer>(i);
             params = {iObj};
             pred->call(params);
         }
         return env.getNull();
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
         for (Arguments::Count i = 0; i < args.count(); ++i) {
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
             return env.create<Integer>(
                 args[0].cast<Integer>()->value() -
                 checkedCast<Integer>(env, args[1])->value());

         case typeInfo.typeId<Double>():
             return env.create<Double>(
                 args[0].cast<Double>()->value() -
                 checkedCast<Double>(env, args[1])->value());

         case typeInfo.typeId<Complex>():
             return env.create<Complex>(
                 args[0].cast<Complex>()->value() -
                 checkedCast<Complex>(env, args[1])->value());
         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"*", nullptr, 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         Integer::Rep iProd = 1;
         Double::Rep dProd = 1.0;
         Complex::Rep cProd(1.0);
         for (Arguments::Count i = 0; i < args.count(); ++i) {
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
             return env.create<Integer>(
                 args[0].cast<Integer>()->value() /
                 checkedCast<Integer>(env, args[1])->value());

         case typeInfo.typeId<Double>():
             return env.create<Double>(
                 args[0].cast<Double>()->value() /
                 checkedCast<Double>(env, args[1])->value());

         case typeInfo.typeId<Complex>():
             return env.create<Complex>(
                 args[0].cast<Complex>()->value() /
                 checkedCast<Complex>(env, args[1])->value());
         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {">", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.getBool(args[0].cast<Integer>()->value() >
                                checkedCast<Integer>(env, args[1])->value());

         case typeInfo.typeId<Double>():
             return env.getBool(args[0].cast<Double>()->value() >
                                checkedCast<Double>(env, args[1])->value());

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
                                checkedCast<Integer>(env, args[1])->value());

         case typeInfo.typeId<Double>():
             return env.getBool(args[0].cast<Double>()->value() <
                                checkedCast<Double>(env, args[1])->value());

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
         const auto real = checkedCast<Double>(env, args[0])->value();
         const auto imag = checkedCast<Double>(env, args[1])->value();
         return env.create<Complex>(Complex::Rep(real, imag));
     }},
};

std::vector<std::string> getBuiltinList()
{
    std::vector<std::string> ret;
    std::for_each(
        std::begin(builtins), std::end(builtins),
        [&](const BuiltinFunctionInfo& info) { ret.push_back(info.name); });
    return ret;
}

ObjectPtr Environment::load(const std::string& key)
{
    auto loc = context_->astRoot_->find(key);
    return context_->topLevel_->load(loc);
}

void Environment::store(ObjectPtr value)
{
    vars_.push_back(value);
}

ObjectPtr Environment::load(VarLoc loc)
{
    // std::cout << "loading: " << loc.frameDist_ << ", "
    //           << loc.offset_ << std::endl;
    // The compiler will have already validated variable offsets, so there's
    // no need to check out of bounds access.
    auto frame = reference();
    while (loc.frameDist_ > 0) {
        frame = frame->parent_;
        loc.frameDist_--;
    }
    return frame->vars_[loc.offset_];
}

ObjectPtr Environment::loadI(ImmediateId immediate)
{
    return context_->immediates_[immediate];
}

ImmediateId Environment::storeI(ObjectPtr value)
{
    const auto ret = context_->immediates_.size();
    context_->immediates_.push_back(value);
    return ret;
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
    : heap_(100000000), topLevel_(std::make_shared<Environment>(this, nullptr)),
      booleans_{{topLevel_->create<Boolean>(false)},
                {topLevel_->create<Boolean>(true)}},
      nullValue_{topLevel_->create<Null>()}
{
    initBuiltins(*topLevel_, builtins);
}

std::shared_ptr<Environment> Context::topLevel()
{
    return topLevel_;
}

const Heap& Environment::getHeap() const
{
    return context_->heap_;
}

Context* Environment::getContext()
{
    return context_;
}

ObjectPtr Environment::exec(const std::string& code)
{
    auto root = lisp::parse(code);
    auto result = getNull();
    if (context_->astRoot_) {
        // Splice and process each statement into the existing environment
        for (auto& st : root->statements_) {
            context_->astRoot_->statements_.push_back(std::move(st));
            context_->astRoot_->statements_.back()->init(*this,
                                                         *context_->astRoot_);
            result = context_->astRoot_->statements_.back()->execute(*this);
        }
    } else {
        root->init(*this, *root);
        result = root->execute(*this);
        context_->astRoot_ = root.release();
    }
    return result;
}


} // namespace lisp
