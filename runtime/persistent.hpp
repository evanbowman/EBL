#pragma once

#include "types.hpp"

// A persistent handle will keep the GC from collecting an object
// bound to a local variable in C++ code. The overhead of a persistent
// is roughly the overhead of a doubly-linked list append/remove
// operation. You could run into caching issues if you use Persistents
// indiscriminately though, so it's better to leave ebl objects in
// the input Arguments if possible, and to keep your wrapped
// CFunctions small.

namespace ebl {

class Environment;

class PersistentBase {
public:
    PersistentBase(Environment& env, ObjectPtr obj);

    ~PersistentBase();

    PersistentBase* next() const
    {
        return next_;
    }

    PersistentBase* prev() const
    {
        return prev_;
    }

    ObjectPtr getUntypedObj() const
    {
        return obj_;
    }

    operator ObjectPtr()
    {
        return obj_;
    }

    // NOTE: overwrite is meant for the GC to use when moving objects
    // around. If you call this function manually, you could break
    // things.
    void UNSAFE_overwrite(ObjectPtr obj)
    {
        obj_ = obj;
    }

protected:
    ObjectPtr obj_;
    PersistentBase* prev_;
    PersistentBase* next_;
};


template <typename T> class Persistent : PersistentBase {
public:
    using PersistentBase::PersistentBase;

    Persistent(const Persistent&) = delete;
    Persistent& operator=(const Persistent&) = delete;

    Persistent& operator=(Heap::Ptr<T> obj)
    {
        obj_ = obj;
        return *this;
    }

    T& operator*() const
    {
        return *obj_.cast<T>().get();
    }

    T* operator->() const
    {
        return obj_.cast<T>().get();
    }

    operator Heap::Ptr<T>()
    {
        return obj_.cast<T>();
    }
};

} // namespace ebl
