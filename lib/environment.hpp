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
#include "extlib/smallVector.hpp"
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

    ObjectPtr load(const std::string& key);

    void store(ObjectPtr value);
    ObjectPtr load(VarLoc loc);

    ObjectPtr loadI(ImmediateId immediate);
    ImmediateId storeI(ObjectPtr value);

    ObjectPtr exec(const std::string& code);

    ObjectPtr getNull();
    ObjectPtr getBool(bool trueOrFalse);

    EnvPtr derive();
    EnvPtr parent();
    EnvPtr reference();

    const Heap& getHeap() const;

    Context* getContext();

private:
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
    };

    Context(const Configuration& config);

    EnvPtr topLevel();

    friend class Environment;

private:
    template <typename T, typename... Args>
    Heap::Ptr<T> create(Environment& env, Args&&... args)
    {
        auto allocObj = [&] {
            return heap_.alloc<typeInfo.get<T>().size_>().template cast<T>();
        };
        auto mem = alloc<T>(allocObj);
        ConstructImpl<T>::construct(mem.get(), env,
                                    std::forward<Args>(args)...);
        return mem;
    }

    template <typename T, typename F> Heap::Ptr<T> alloc(F&& allocImpl)
    {
        try {
            return allocImpl();
        } catch (const Heap::OOM& oom) {
            GC gc;
            gc.run(*topLevel_);
            return allocImpl();
        }
    }

    Heap heap_;
    EnvPtr topLevel_;
    Persistent<Boolean> booleans_[2];
    Persistent<Null> nullValue_;
    std::vector<ObjectPtr> immediates_;

    // TODO: Eventually the codebase will execute bytecode instead of doing tree
    // walking, but for now, previously exec'd trees need to be stored because
    // some variables in the environment might reference the old ast(s).
    // TODO: Should new code be merged into the top level of the existing ast?
    ast::TopLevel* astRoot_ = nullptr;
};

template <typename T, typename... Args>
Heap::Ptr<T> Environment::create(Args&&... args)
{
    return context_->create<T>(*this, std::forward<Args>(args)...);
}

} // namespace lisp
