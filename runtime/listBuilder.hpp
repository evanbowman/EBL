#pragma once

// FIXME!!!
#include "../extlib/optional.hpp"
#include "../extlib/smallVector.hpp"

#include "persistent.hpp"
#include "types.hpp"


namespace ebl {

class Environment;

class ListBuilder {
public:
    ListBuilder(Environment& env, ValuePtr first);
    void pushFront(ValuePtr value);
    void pushBack(ValuePtr value);
    ValuePtr result();

private:
    Environment& env_;
    Persistent<Pair> front_;
    Heap::Ptr<Pair> back_;
};


// More overhead, but doesn't require an initial element like
// ListBuilder. Note: this does not build LazyLists, the builder is
// Lazy :)
class LazyListBuilder {
public:
    LazyListBuilder(Environment& env);
    void pushFront(ValuePtr value);
    void pushBack(ValuePtr value);
    ValuePtr result();

private:
    template <typename F> void push(ValuePtr value, F&& callback)
    {
        if (not builder_)
            builder_.emplace(env_, value);
        else
            callback(*builder_);
    }
    Environment& env_;
    nonstd::optional<ListBuilder> builder_;
};

} // namespace ebl
