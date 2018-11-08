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
    Heap(size_t capacity) : capacity_(capacity)
    {
        begin_ = (uint8_t*)malloc(capacity);
        end_ = begin_;
    }

    Heap(const Heap&) = delete;

    ~Heap()
    {
        free(begin_);
    }

    template <typename T> class Ptr;

    class GenericPtr {
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

    template <typename T> class Local;

    template <typename T> class Persistent;

    template <typename T> class Ptr : public GenericPtr {
    public:
        template <typename U> Ptr(Ptr<U> other) : GenericPtr{other.handle()}
        {
            static_assert(std::is_base_of<T, U>::value, "bad upcast");
        }

        Ptr(Local<T> local);

        Ptr(Persistent<T> persistent);

        T* operator*() const
        {
            return reinterpret_cast<T*>(handle());
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

    size_t size() const
    {
        return end_ - begin_;
    }

    size_t capacity() const
    {
        return capacity_;
    }

    struct OOM : public std::runtime_error {
        OOM() : std::runtime_error("heap exhausted")
        {
        }
    };

    template <size_t size> GenericPtr alloc()
    {
        if (this->size() + size < capacity_) {
            auto result = end_;
            end_ += size;
            return {result};
        }
        throw OOM{};
    }

private:
    friend class GC;

    uint8_t* begin_;
    uint8_t* end_;
    size_t capacity_;
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

class GC {
public:
    void run(Environment& env)
    {
        mark(env);
        compact(env);
    }

private:
    void mark(Environment& env);
    void compact(Environment& env);
};

} // namespace lisp
