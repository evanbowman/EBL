#pragma once

#include "utility.hpp"
#include <functional>
#include <limits>
#include <map>
#include <new>
#include <string>
#include <tuple>
#include <vector>

#include "common.hpp"
#include "dll.hpp"

// FIXME!!!
#include "../extlib/smallVector.hpp"

#include "gc.hpp"
#include "memory.hpp"
#include "types.hpp"
#include "vm.hpp"


namespace ebl {

class Context;

class Environment;
using EnvPtr = std::shared_ptr<Environment>;

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment(Context* context, EnvPtr parent)
        : context_(context), parent_(parent)
    {
    }
    Environment(const Environment&) = delete;

    template <typename T, typename... Args> Heap::Ptr<T> create(Args&&... args);

    // Load/store a variable in the root environment.
    ValuePtr getGlobal(const std::string& key);
    void setGlobal(const std::string& key, ValuePtr value);
    void setGlobal(const std::string& key, const std::string& nameSpace,
                   ValuePtr value);

    // Compile and execute ebl code
    ValuePtr exec(const std::string& code);

    ValuePtr getNull();
    ValuePtr getBool(bool trueOrFalse);

    // For storing and loading from an environment's stack. Only meant to be
    // called by the runtime or the vm.
    void push(ValuePtr value);
    void store(VarLoc loc, ValuePtr value);
    ValuePtr load(VarLoc loc);
    void clear();

    void openDLL(const std::string& name);

    EnvPtr derive();
    EnvPtr parent();
    EnvPtr reference();

    Context* getContext();

    using Variables = Ogre::SmallVector<ValuePtr, 6>;
    Variables& getVars()
    {
        return vars_;
    }

private:
    Environment& getFrame(VarLoc loc);

    Context* context_;
    EnvPtr parent_;
    Variables vars_;
};

template <typename T> struct ConstructImpl {
    template <typename... Args>
    static void construct(T* mem, Environment&, Args&&... args)
    {
        new (mem) T{std::forward<Args>(args)...};
    }
};

template <> struct ConstructImpl<Function> {
    template <typename... Args>
    static void construct(Function* mem, Environment& env, Args&&... args)
    {
        new (mem) Function{env, std::forward<Args>(args)...};
    }
};


namespace ast {
struct TopLevel;
}

class PersistentBase;


class Context {
public:
    struct Configuration {
        size_t heapSize_;
    };

    Context(const Configuration& config = defaultConfig());
    Context(const Context&) = delete;
    ~Context();

    std::vector<ValuePtr>& immediates()
    {
        return immediates_;
    }

    std::vector<ValuePtr>& operandStack()
    {
        return operandStack_;
    }

    Environment& topLevel()
    {
        return *topLevel_;
    }

    friend class Environment;

    using CallStack = std::vector<StackFrame>;
    CallStack& callStack()
    {
        return callStack_;
    }

    void runGC(Environment& env)
    {
        collector_->run(env, heap_);
    }

    void writeToFile(const std::string& fname);

    void loadFromFile(const std::string& fname);

    const Bytecode& getProgram() const
    {
        return program_;
    }

    PersistentBase*& getPersistentsList()
    {
        return persistentsList_;
    }

    struct MemoryStat {
        size_t used_;
        size_t remaining_;
    };

    MemoryStat memoryStat() const
    {
        return {heap_.size(), heap_.capacity() - heap_.size()};
    }

private:
    template <typename T, typename... Args>
    Heap::Ptr<T> create(Environment& env, Args&&... args)
    {
        auto allocVal = [&] { return heap_.alloc<T>().template cast<T>(); };
        auto mem = alloc<T>(env, allocVal);
        ConstructImpl<T>::construct(mem.get(), env,
                                    std::forward<Args>(args)...);
        return mem;
    }

    template <typename T, typename F>
    Heap::Ptr<T> alloc(Environment& env, F&& allocImpl)
    {
        try {
            return allocImpl();
        } catch (const Heap::OOM& oom) {
            runGC(env);
            return allocImpl();
        }
    }

    static const Configuration& defaultConfig();

    Heap heap_;
    EnvPtr topLevel_;
    Heap::Ptr<Boolean> booleans_[2];
    Heap::Ptr<Null> nullValue_;
    std::vector<ValuePtr> immediates_;
    std::vector<ValuePtr> operandStack_;
    std::vector<DLL> dlls_;
    ast::TopLevel* astRoot_ = nullptr;
    Bytecode program_;
    std::unique_ptr<GC> collector_;
    CallStack callStack_;
    PersistentBase* persistentsList_;
};

template <typename T, typename... Args>
Heap::Ptr<T> Environment::create(Args&&... args)
{
    return context_->create<T>(*this, std::forward<Args>(args)...);
}

template <typename T>
ImmediateId storeI(Context& context, const typename T::Input& val)
{
    auto& immediates = context.immediates();
    const auto ret = immediates.size();
    for (size_t i = 0; i < immediates.size(); ++i) {
        if (isType<T>(immediates[i])) {
            if (immediates[i].cast<T>()->value() == val) {
                return i;
            }
        }
    }
    immediates.push_back(context.topLevel().create<T>(val));
    return ret;
}

template <>
inline ImmediateId storeI<Symbol>(Context& context, const Heap::Ptr<String>& val)
{
    auto& immediates = context.immediates();
    const auto ret = immediates.size();
    for (size_t i = 0; i < immediates.size(); ++i) {
        if (isType<Symbol>(immediates[i])) {
            if (*immediates[i].cast<Symbol>()->value() == *val) {
                return i;
            }
        }
    }
    immediates.push_back(context.topLevel().create<Symbol>(val));
    return ret;
}

} // namespace ebl
