#include "environment.hpp"
#include "parser.hpp"


namespace lisp {

ObjectPtr Environment::load(const std::string& key)
{
    auto loc = context_->astRoot_->find(key);
    return context_->topLevel_->load(loc);
}

void Environment::store(ObjectPtr value)
{
    vars_.push_back(value);
}

ObjectPtr Environment::load(VarLoc loc)
{
    // The compiler will have already validated variable offsets, so there's
    // no need to check out of bounds access.
    auto frame = reference();
    while (loc.frameDist_ > 0) {
        frame = frame->parent_;
        loc.frameDist_--;
    }
    return frame->vars_[loc.offset_];
}

ObjectPtr Environment::loadI(ImmediateId immediate)
{
    return context_->immediates_[immediate];
}

ImmediateId Environment::storeI(ObjectPtr value)
{
    const auto ret = context_->immediates_.size();
    EqualTo eq;
    for (size_t i = 0; i < context_->immediates_.size(); ++i) {
        if (eq(context_->immediates_[i], value)) {
            return i;
        }
    }
    context_->immediates_.push_back(value);
    return ret;
}

ObjectPtr Environment::getNull()
{
    return context_->nullValue_.get();
}

ObjectPtr Environment::getBool(bool trueOrFalse)
{
    return context_->booleans_[trueOrFalse].get();
}

EnvPtr Environment::derive()
{
    return std::make_shared<Environment>(context_, reference());
}

EnvPtr Environment::parent()
{
    return parent_;
}

EnvPtr Environment::reference()
{
    return shared_from_this();
}

void initBuiltins(Environment& env);

Context::Context(const Configuration& config)
    : heap_(100000000), topLevel_(std::make_shared<Environment>(this, nullptr)),
      booleans_{{topLevel_->create<Boolean>(false)},
                {topLevel_->create<Boolean>(true)}},
      nullValue_{topLevel_->create<Null>()}
{
    initBuiltins(*topLevel_);
}

std::shared_ptr<Environment> Context::topLevel()
{
    return topLevel_;
}

const Heap& Environment::getHeap() const
{
    return context_->heap_;
}

Context* Environment::getContext()
{
    return context_;
}

ObjectPtr Environment::exec(const std::string& code)
{
    auto root = lisp::parse(code);
    auto result = getNull();
    if (context_->astRoot_) {
        // Splice and process each statement into the existing environment
        for (auto& st : root->statements_) {
            context_->astRoot_->statements_.push_back(std::move(st));
            context_->astRoot_->statements_.back()->init(*this,
                                                         *context_->astRoot_);
            result = context_->astRoot_->statements_.back()->execute(*this);
        }
    } else {
        root->init(*this, *root);
        result = root->execute(*this);
        context_->astRoot_ = root.release();
    }
    return result;
}


} // namespace lisp
