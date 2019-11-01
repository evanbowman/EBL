#include "persistent.hpp"
#include "environment.hpp"


namespace ebl {

PersistentBase::PersistentBase(Environment& env, ValuePtr val)
    : val_(val), next_(nullptr)
{
    auto& plist = env.getContext()->getPersistentsList();
    if (plist) {
        plist->next_ = this;
        prev_ = plist;
    } else {
        prev_ = nullptr;
    }
    plist = this;
}

PersistentBase::~PersistentBase()
{
    if (next_) {
        next_->prev_ = prev_;
    }
    if (prev_) {
        prev_->next_ = next_;
    }
}


} // namespace ebl
