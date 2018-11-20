#pragma once

#include "extlib/optional.hpp"
#include "types.hpp"


namespace lisp {

class Environment;

class ListBuilder {
public:
    ListBuilder(Environment& env, ObjectPtr first);
    void pushFront(ObjectPtr value);
    void pushBack(ObjectPtr value);
    ObjectPtr result();

private:
    Environment& env_;
    Local<Pair> front_;
    Heap::Ptr<Pair> back_;
};


// More overhead, but doesn't require an initial element like ListBuilder
class LazyListBuilder {
public:
    LazyListBuilder(Environment& env);
    void pushFront(ObjectPtr value);
    void pushBack(ObjectPtr value);
    ObjectPtr result();

private:
    template <typename F> void push(ObjectPtr value, F&& callback)
    {
        if (not builder_)
            builder_.emplace(env_, value);
        else
            callback(*builder_);
    }
    Environment& env_;
    nonstd::optional<ListBuilder> builder_;
};

} // namespace lisp
