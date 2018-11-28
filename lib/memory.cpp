#include "memory.hpp"
#include "environment.hpp"

namespace lisp {

Heap::Heap(size_t capacity) : capacity_(capacity)
{
    begin_ = (uint8_t*)malloc(capacity);
    end_ = begin_;
}

Heap::Heap() : begin_(nullptr), end_(nullptr), capacity_(0)
{
}

void Heap::init(size_t capacity)
{
    capacity_ = capacity;
    begin_ = (uint8_t*)malloc(capacity);
    end_ = begin_;
}

Heap::~Heap()
{
    free(begin_);
}

size_t Heap::size() const
{
    return end_ - begin_;
}

size_t Heap::capacity() const
{
    return capacity_;
}

uint8_t* Heap::begin() const
{
    return begin_;
}

uint8_t* Heap::end() const
{
    return end_;
}

} // namespace lisp
