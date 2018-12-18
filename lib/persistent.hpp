#pragma once

#include "types.hpp"

// A persistent handle will keep the GC from collecting an object
// bound to a local variable in C++ code. The overhead of a persistent
// is roughly the overhead of a doubly-linked list append/remove
// operation. You could run into caching issues if you use Persistents
// indiscriminately though, so it's better to leave lisp objects in
// the input Arguments if possible, and to keep your wrapped
// CFunctions small.

namespace lisp {

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

    Heap::Ptr<T> get() const
    {
        return obj_.cast<T>();
    }
};

} // namespace lisp
