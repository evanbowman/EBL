#pragma once

#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
#include <type_traits>

namespace lisp {

class Environment;

class Heap {
public:
    Heap(size_t capacity);
    Heap();
    Heap(const Heap&) = delete;
    ~Heap();

    void init(size_t capacity);

    template <typename T> class Ptr;
    class GenericPtr;
    template <typename T> class Local;
    template <typename T> class Persistent;

    struct OOM : public std::runtime_error {
        OOM() : std::runtime_error("heap exhausted")
        {
        }
    };

    template <size_t Size> GenericPtr alloc();

    template <typename T> Heap::Ptr<T> arrayElemAt(size_t index) const;

    size_t size() const;
    size_t capacity() const;
    uint8_t* begin() const;
    uint8_t* end() const;

private:
    uint8_t* begin_;
    uint8_t* end_;
    size_t capacity_;
};

class Heap::GenericPtr {
public:
    using HandleType = uint8_t*;

    template <typename T> Ptr<T> cast() const
    {
        return {handle_};
    }

    HandleType handle() const
    {
        return handle_;
    }

    bool operator==(const GenericPtr& other) const
    {
        return handle_ == other.handle_;
    }

protected:
    friend class Heap;

    GenericPtr(HandleType handle) : handle_(handle)
    {
    }

private:
    uint8_t* handle_;
};

template <typename T> class Heap::Ptr : public GenericPtr {
public:
    template <typename U> Ptr(Ptr<U> other) : GenericPtr{other.handle()}
    {
        static_assert(std::is_base_of<T, U>::value, "bad upcast");
    }

    Ptr(Local<T> local);

    Ptr(Persistent<T> persistent);

    T& operator*() const
    {
        return *reinterpret_cast<T*>(handle());
    }
    T* operator->() const
    {
        return reinterpret_cast<T*>(handle());
    }

    T* get()
    {
        return reinterpret_cast<T*>(handle());
    }

protected:
    friend class GenericPtr;

    Ptr(HandleType handle) : GenericPtr(handle)
    {
    }
};

template <typename T> class Local {
public:
    template <typename U>
    Local(Environment&, Heap::Ptr<U> value) : value_(value)
    {
    }

    template <typename U> Local& operator=(Heap::Ptr<U> value)
    {
        value_ = value;
        return *this;
    }

    Heap::Ptr<T> get() const
    {
        return value_;
    }

    // Copying or moving a local doesn't make any sense!
    Local(Local&) = delete;

private:
    // TODO: right now Local doesn't do anything. Later, Local should
    // instead store the value on the VM stack, and store a pointer to
    // that stack location.
    Heap::Ptr<T> value_;
};

template <typename T> Heap::Ptr<T>::Ptr(Local<T> other) : Ptr(other.get())
{
}

template <typename T> Heap::Ptr<T>::Ptr(Persistent<T> other) : Ptr(other.value_)
{
}

template <typename T> class Persistent {
public:
    Heap::Ptr<T> get()
    {
        return value_;
    }

    // TODO
    Heap::Ptr<T> value_;
};

template <size_t Size> Heap::GenericPtr Heap::alloc()
{
    if (this->size() + Size <= capacity_) {
        auto result = end_;
        end_ += Size;
        return {result};
    }
    throw OOM{};
}

template <typename T> Heap::Ptr<T> Heap::arrayElemAt(size_t index) const
{
    return GenericPtr((uint8_t*)(((T*)begin()) + index)).cast<T>();
}

} // namespace lisp
