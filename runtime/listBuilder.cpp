#include "listBuilder.hpp"
#include "environment.hpp"

namespace ebl {

ListBuilder::ListBuilder(Environment& env, ObjectPtr first)
    : env_(env), front_(env, env.create<Pair>(first, env.getNull())),
      back_(front_.get())
{
}


void ListBuilder::pushFront(ObjectPtr value)
{
    front_.set(env_.create<Pair>(value, front_.get()));
}


void ListBuilder::pushBack(ObjectPtr value)
{
    auto next = env_.create<Pair>(value, env_.getNull());
    back_->setCdr(next);
    back_ = next;
}


ObjectPtr ListBuilder::result()
{
    return front_.get();
}


LazyListBuilder::LazyListBuilder(Environment& env) : env_(env)
{
}


void LazyListBuilder::pushFront(ObjectPtr value)
{
    push(value, [&](ListBuilder& builder) { builder.pushFront(value); });
}


void LazyListBuilder::pushBack(ObjectPtr value)
{
    push(value, [&](ListBuilder& builder) { builder.pushBack(value); });
}


ObjectPtr LazyListBuilder::result()
{
    if (builder_)
        return builder_->result();
    else
        return env_.getNull();
}


} // namespace ebl
