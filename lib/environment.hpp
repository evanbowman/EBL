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
#include "extlib/smallVector.hpp"
#include "gc.hpp"
#include "memory.hpp"
#include "types.hpp"


namespace lisp {

class Context;

class Environment;
using EnvPtr = std::shared_ptr<Environment>;

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment(Context* context, EnvPtr parent)
        : context_(context), parent_(parent)
    {
    }

    template <typename T, typename... Args> Heap::Ptr<T> create(Args&&... args);

    // Load/store a variable in the root environment.
    ObjectPtr getGlobal(const std::string& key);
    void setGlobal(const std::string& key, ObjectPtr value);

    // For storing and loading intern'd immediates
    ObjectPtr loadI(ImmediateId immediate);
    ImmediateId storeI(ObjectPtr value);

    // Compile and execute lisp code
    ObjectPtr exec(const std::string& code);

    ObjectPtr getNull();
    ObjectPtr getBool(bool trueOrFalse);

    // For storing and loading from an environment's stack. Only meant to be
    // called by the runtime or the vm.
    void push(ObjectPtr value);
    void store(VarLoc loc, ObjectPtr value);
    ObjectPtr load(VarLoc loc);

    void openDLL(const std::string& name);

    EnvPtr derive();
    EnvPtr parent();
    EnvPtr reference();

    Context* getContext();

    using Variables = Ogre::SmallVector<ObjectPtr, 6>;
    Variables& getVars()
    {
        return vars_;
    }

private:
    Environment& getFrame(VarLoc loc);

    Context* context_;
    EnvPtr parent_;
    Ogre::SmallVector<ObjectPtr, 6> vars_;
};

template <typename T> struct ConstructImpl {
    template <typename... Args>
    static void construct(T* mem, Environment& env, Args&&... args)
    {
        new (mem) T{typeInfo.typeId<T>(), std::forward<Args>(args)...};
    }
};

template <> struct ConstructImpl<Function> {
    template <typename... Args>
    static void construct(Function* mem, Environment& env, Args&&... args)
    {
        new (mem) Function{typeInfo.typeId<Function>(), env,
                           std::forward<Args>(args)...};
    }
};

std::vector<std::string> getBuiltinList();

namespace ast {
struct TopLevel;
}

class Context {
public:
    struct Configuration {
        size_t heapSize_;
    };

    Context(const Configuration& config = defaultConfig());
    Context(const Context&) = delete;
    ~Context();

    std::vector<ObjectPtr>& immediates()
    {
        return immediates_;
    }

    EnvPtr topLevel();

    friend class Environment;

    using CallStack = std::vector<EnvPtr>;
    CallStack& callStack()
    {
        return callStack_;
    }

private:
    template <typename T, typename... Args>
    Heap::Ptr<T> create(Environment& env, Args&&... args)
    {
        auto allocObj = [&] {
            return heap_.alloc<typeInfo.get<T>().size_>().template cast<T>();
        };
        auto mem = alloc<T>(env, allocObj);
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
            collector_->run(env, heap_);
            return allocImpl();
        }
    }

    static const Configuration& defaultConfig();

    Heap heap_;
    EnvPtr topLevel_;
    Persistent<Boolean> booleans_[2];
    Persistent<Null> nullValue_;
    std::vector<ObjectPtr> immediates_;
    std::vector<DLL> dlls_;
    ast::TopLevel* astRoot_ = nullptr;
    std::unique_ptr<GC> collector_;

    // A lambda knows where it was defined, but has no concept of
    // where it was called, so we do actually need a call stack.
    std::vector<EnvPtr> callStack_;
};

template <typename T, typename... Args>
Heap::Ptr<T> Environment::create(Args&&... args)
{
    return context_->create<T>(*this, std::forward<Args>(args)...);
}

} // namespace lisp
