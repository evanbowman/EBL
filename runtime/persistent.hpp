#pragma once

#include "types.hpp"

// A persistent handle will keep the GC from collecting an value
// bound to a local variable in C++ code. The overhead of a persistent
// is roughly the overhead of a doubly-linked list append/remove
// operation. You could run into caching issues if you use Persistents
// indiscriminately though, so it's better to leave ebl values in
// the input Arguments if possible, and to keep your wrapped
// CFunctions small.

namespace ebl {

class Environment;

class PersistentBase {
public:
    PersistentBase(Environment& env, ValuePtr val);

    ~PersistentBase();

    PersistentBase* next() const
    {
        return next_;
    }

    PersistentBase* prev() const
    {
        return prev_;
    }

    ValuePtr getUntypedVal() const
    {
        return val_;
    }

    operator ValuePtr()
    {
        return val_;
    }

    // NOTE: overwrite is meant for the GC to use when moving values
    // around. If you call this function manually, you could break
    // things.
    void UNSAFE_overwrite(ValuePtr val)
    {
        val_ = val;
    }

protected:
    ValuePtr val_;
    PersistentBase* prev_;
    PersistentBase* next_;
};


template <typename T> class Persistent : PersistentBase {
public:
    using PersistentBase::PersistentBase;

    Persistent(const Persistent&) = delete;
    Persistent& operator=(const Persistent&) = delete;

    Persistent& operator=(Heap::Ptr<T> val)
    {
        val_ = val;
        return *this;
    }

    T& operator*() const
    {
        return *val_.cast<T>().get();
    }

    T* operator->() const
    {
        return val_.cast<T>().get();
    }

    operator Heap::Ptr<T>()
    {
        return val_.cast<T>();
    }
};

} // namespace ebl
