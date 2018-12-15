#pragma once

#include <cstring>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <type_traits>

namespace lisp {

class Environment;

template <size_t Alignment> class Memory {
public:
    static constexpr const size_t Align = Alignment;

    Memory(size_t capacity)
    {
        init(capacity);
    }

    Memory(size_t capacity, uint8_t initialValue)
    {
        init(capacity);
        std::memset(begin_, initialValue, capacity_);
    }

    Memory() : begin_(nullptr), end_(nullptr), capacity_(0)
    {
    }

    Memory(const Memory&) = delete;

    Memory(Memory&& other)
        : begin_(other.begin_), end_(other.end_), capacity_(other.capacity_)
    {
        other.begin_ = nullptr;
        other.end_ = nullptr;
        other.capacity_ = 0;
    }

    ~Memory()
    {
        free(begin_);
    }

    void init(size_t capacity)
    {
        if (capacity % Alignment not_eq 0) {
            throw std::runtime_error("Allocation request does not satify"
                                     " alignment requirement of " +
                                     std::to_string(Alignment));
        }
        capacity_ = capacity;
        begin_ = (uint8_t*)malloc(capacity);
        if (not begin_) {
            throw std::runtime_error("failed to allocate a heap");
        }
        end_ = begin_;
    }

    template <typename T> class Ptr;
    class GenericPtr;
    template <typename T> class Local;
    template <typename T> class Persistent;

    struct OOM : public std::runtime_error {
        OOM() : std::runtime_error("heap exhausted")
        {
        }
    };

    template <typename T> GenericPtr alloc();

    template <typename T> Memory::Ptr<T> arrayElemAt(size_t index) const;

    void compacted(size_t bytes)
    {
        end_ -= bytes;
    }

    size_t size() const
    {
        return end_ - begin_;
    }

    size_t capacity() const
    {
        return capacity_;
    }

    uint8_t* begin() const
    {
        return begin_;
    }

    uint8_t* end() const
    {
        return end_;
    }

private:
    uint8_t* begin_;
    uint8_t* end_;
    size_t capacity_;
};


template <size_t Alignment> class Memory<Alignment>::GenericPtr {
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

    // NOTE: overwrite is meant for the GC to use when moving objects
    // around. If you call this function manually, you could break
    // things.
    void UNSAFE_overwrite(void* val)
    {
        handle_ = (HandleType)val;
    }

protected:
    friend class Memory;

    GenericPtr(HandleType handle) : handle_(handle)
    {
    }

private:
    HandleType handle_;
};

template <size_t Alignment>
template <typename T>
class Memory<Alignment>::Ptr : public Memory<Alignment>::GenericPtr {
public:
    template <typename U> Ptr(Ptr<U> other) : GenericPtr{other.handle()}
    {
        static_assert(std::is_base_of<T, U>::value, "bad upcast");
    }

    T& operator*() const
    {
        return *reinterpret_cast<T*>(GenericPtr::handle());
    }
    T* operator->() const
    {
        return reinterpret_cast<T*>(GenericPtr::handle());
    }

    T* get()
    {
        return reinterpret_cast<T*>(GenericPtr::handle());
    }

protected:
    friend class GenericPtr;

    Ptr(typename GenericPtr::HandleType handle) : GenericPtr(handle)
    {
    }
};


template <size_t Alignment>
template <typename T>
typename Memory<Alignment>::GenericPtr Memory<Alignment>::alloc()
{
    static_assert(alignof(T) == Alignment, "Invalid alignment");
    if (this->size() + sizeof(T) <= capacity_) {
        auto result = end_;
        end_ += sizeof(T);
        return {result};
    }
    throw OOM{};
}

template <size_t Alignment>
template <typename T>
typename Memory<Alignment>::template Ptr<T>
Memory<Alignment>::arrayElemAt(size_t index) const
{
    return GenericPtr((uint8_t*)(((T*)begin()) + index)).template cast<T>();
}

using Heap = Memory<8>;

} // namespace lisp
