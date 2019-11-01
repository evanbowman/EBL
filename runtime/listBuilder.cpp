#include "listBuilder.hpp"
#include "environment.hpp"

namespace ebl {

ListBuilder::ListBuilder(Environment& env, ValuePtr first)
    : env_(env), front_(env, env.create<Pair>(first, env.getNull())),
      back_((Heap::Ptr<Pair>)front_)
{
}


void ListBuilder::pushFront(ValuePtr value)
{
    front_ = env_.create<Pair>(value, (Heap::Ptr<Pair>)front_);
}


void ListBuilder::pushBack(ValuePtr value)
{
    auto next = env_.create<Pair>(value, env_.getNull());
    back_->setCdr(next);
    back_ = next;
}


ValuePtr ListBuilder::result()
{
    return (Heap::Ptr<Pair>)front_;
}


LazyListBuilder::LazyListBuilder(Environment& env) : env_(env)
{
}


void LazyListBuilder::pushFront(ValuePtr value)
{
    push(value, [&](ListBuilder& builder) { builder.pushFront(value); });
}


void LazyListBuilder::pushBack(ValuePtr value)
{
    push(value, [&](ListBuilder& builder) { builder.pushBack(value); });
}


ValuePtr LazyListBuilder::result()
{
    if (builder_)
        return builder_->result();
    else
        return env_.getNull();
}


} // namespace ebl
