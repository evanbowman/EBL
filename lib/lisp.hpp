#pragma once

#include "environment.hpp"

namespace lisp {

void print(Environment& env, ObjectPtr obj, std::ostream& out, bool showQuotes);

inline ObjectPtr listRef(Heap::Ptr<Pair> p, size_t index)
{
    while (index > 0) {
        p = checkedCast<Pair>(p->getCdr());
        --index;
    }
    return p->getCar();
}

} // namespace lisp
