#pragma once

#include <tuple>
#include <string>
#include <new>
#include <vector>
#include <map>
#include "utility.hpp"
#include <functional>
#include <limits>

#include "types.hpp"
#include "memory.hpp"

namespace lisp {

class Context;

class Environment;
using EnvPtr = std::shared_ptr<Environment>;


class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment(Context* context, EnvPtr parent) :
        context_(context),
        parent_(parent) {}

    template <typename T, typename ...Args>
    Heap::Ptr<T> create(Args&& ...args);

    void store(const std::string& key, ObjectPtr value);
    ObjectPtr load(const std::string& key);

    ObjectPtr getNull();
    ObjectPtr getBool(bool trueOrFalse);

    EnvPtr derive();
    EnvPtr parent();
    EnvPtr reference();

private:
    Context* context_;
    EnvPtr parent_;
    std::map<std::string, ObjectPtr> vars_;
};


template <typename T>
struct ConstructImpl {
    template <typename ...Args>
    static void construct(T* mem, Environment& env, Args&& ...args) {
        new (mem) T{typeInfo.typeId<T>(), std::forward<Args>(args)...};
    }
};

template <>
struct ConstructImpl<Subr> {
    template <typename ...Args>
    static void construct(Subr* mem, Environment& env, Args&& ...args) {
        new (mem) Subr{typeInfo.typeId<Subr>(), env,
                       std::forward<Args>(args)...};
    }
};


class Context {
public:
    struct Configuration {};

    Context(const Configuration& config);

    ObjectPtr getNull();
    ObjectPtr getBool(bool trueOrFalse);

    std::shared_ptr<Environment> topLevel();

    friend class Environment;

private:
    template <typename T, typename ...Args>
    Heap::Ptr<T> create(Environment& env, Args&& ...args) {
        auto allocObj = [&] {
            return heap_.alloc<typeInfo.get<T>().size_>().template cast<T>();
        };
        auto mem = alloc<T>(allocObj);
        ConstructImpl<T>::construct(mem.get(), env,
                                    std::forward<Args>(args)...);
        return mem;
    }

    template <typename T, typename F>
    Heap::Ptr<T> alloc(F&& allocImpl) {
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
};


template <typename T, typename ...Args>
Heap::Ptr<T> Environment::create(Args&& ...args) {
    return context_->create<T>(*this, std::forward<Args>(args)...);
}

} // lisp
