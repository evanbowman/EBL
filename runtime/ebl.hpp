#pragma once

#include "environment.hpp"

namespace ebl {

void print(Environment& env, ValuePtr val, std::ostream& out, bool showQuotes);

inline ValuePtr listRef(Heap::Ptr<Pair> p, size_t index)
{
    while (index > 0) {
        p = checkedCast<Pair>(p->getCdr());
        --index;
    }
    return p->getCar();
}

} // namespace ebl
