#pragma once

#include <atomic>

namespace ebl {

class Spinlock {
public:
    inline void lock()
    {
        while (lock_.test_and_set(std::memory_order_acquire))
            ;
    }

    inline void unlock()
    {
        lock_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};


} // namespace ebl
