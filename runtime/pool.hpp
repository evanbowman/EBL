#include "macros.hpp"
#include <array>
#include <cassert>
#include <mutex>
#include <stddef.h>

namespace ebl {

// NOTE: Pools are only designed for scalar allocations. The whole
// intent of this class is specifically for use with allocate_shared,
// for allocating environment frames.

template <typename T> struct PoolAllocator {
    typedef T value_type;

    class Pool {
    private:
        struct Node {
            alignas(T) std::array<uint8_t, sizeof(T)> mem_;
            Node* next_;
        };

        void grow()
        {
            constexpr size_t allocCount = 4096 / sizeof(Node);
            constexpr size_t allocSize = allocCount * sizeof(Node);
            auto nodes = (Node*)malloc(allocSize);
            for (size_t i = 0; i < allocCount; ++i) {
                Node* current = &nodes[i];
                current->next_ = freelist_;
                freelist_ = current;
            }
        }

    public:
        Pool() : freelist_(nullptr)
        {
        }

        void* alloc()
        {
            std::lock_guard<std::mutex> guard(lock_);
            if (UNLIKELY(freelist_ == nullptr)) {
                grow();
            }
            void* const ret = freelist_->mem_.data();
            freelist_ = freelist_->next_;
            return ret;
        }

        void dealloc(void* mem)
        {
            std::lock_guard<std::mutex> guard(lock_);
            auto node = (Node*)mem;
            node->next_ = freelist_;
            freelist_ = node;
        }

    private:
        Node* freelist_;
        std::mutex lock_;
    };

    static Pool pool;

    PoolAllocator() noexcept {};

    template <typename U>
    PoolAllocator(const PoolAllocator<U>& other) throw(){};

    T* allocate(size_t n, const void* hint = 0)
    {
        return static_cast<T*>(pool.alloc());
    }

    void deallocate(T* ptr, size_t n)
    {
        pool.dealloc(ptr);
    }
};

template <typename T> typename PoolAllocator<T>::Pool PoolAllocator<T>::pool;

template <typename T, typename U>
inline bool operator==(const PoolAllocator<T>&, const PoolAllocator<U>&)
{
    return true;
}

template <typename T, typename U>
inline bool operator!=(const PoolAllocator<T>& a, const PoolAllocator<U>& b)
{
    return !(a == b);
}

} // namespace ebl
