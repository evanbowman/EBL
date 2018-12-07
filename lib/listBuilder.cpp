#include "listBuilder.hpp"
#include "environment.hpp"

namespace lisp {

ListBuilder::ListBuilder(Environment& env, ObjectPtr first)
    : env_(env), front_(env.create<Pair>(first, env.getNull())), back_(front_)
{
}


void ListBuilder::pushFront(ObjectPtr value)
{
    front_ = env_.create<Pair>(value, front_);
}


void ListBuilder::pushBack(ObjectPtr value)
{
    auto next = env_.create<Pair>(value, env_.getNull());
    back_->setCdr(next);
    back_ = next;
}


ObjectPtr ListBuilder::result()
{
    return front_;
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


} // namespace lisp
