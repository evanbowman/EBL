#include "ast.hpp"
#include "lisp.hpp"


namespace lisp {
namespace ast {

ObjectPtr Integer::execute(Environment& env)
{
    return env.create<lisp::Integer>(value_);
}


ObjectPtr String::execute(Environment& env)
{
    return env.create<lisp::String>(value_.c_str(), value_.length());
}


ObjectPtr Null::execute(Environment& env)
{
    return env.getNull();
}


ObjectPtr LValue::execute(Environment& env)
{
    return env.load(name_);
}


class InterpretedFunctionImpl : public Function::Impl {
public:
    // This solution stores a raw pointer to an ast subtree. While
    // the code may appear unsafe, the ast will definitely outlive
    // the raw pointer; you are traversing the ast while executing
    // the program, after all :).
    InterpretedFunctionImpl(ast::Lambda* impl) : impl_(impl)
    {
    }


    ObjectPtr call(Environment& env, std::vector<ObjectPtr>& args)
    {
        auto derived = env.derive();
        for (size_t i = 0; i < args.size(); ++i) {
            derived->store(impl_->argNames_[i], args[i]);
        }
        auto up = env.getNull();
        for (auto& statement : impl_->statements_) {
            up = statement->execute(*derived);
        }
        return up;
    }


private:
    const ast::Lambda* impl_;
};


ObjectPtr Lambda::execute(Environment& env)
{
    auto impl = make_unique<InterpretedFunctionImpl>(this);
    const Arguments::Count argc = argNames_.size();
    return env.create<lisp::Function>("", argc, std::move(impl));
}


ObjectPtr Application::execute(Environment& env)
{
    auto loaded = checkedCast<lisp::Function>(env, env.load(target_));
    std::vector<ObjectPtr> args;
    for (const auto& arg : args_) {
        args.push_back(arg->execute(env));
    }
    return loaded->call(args);
}


ObjectPtr Let::Binding::execute(Environment& env)
{
    env.store(name_, value_->execute(env));
    return env.getNull();
}


ObjectPtr Let::execute(Environment& env)
{
    auto derived = env.derive();
    for (const auto& binding : bindings_) {
        binding->execute(*derived);
    }
    ObjectPtr up = env.getNull();
    for (const auto& st : statements_) {
        up = st->execute(*derived);
    }
    return up;
}


ObjectPtr Begin::execute(Environment& env)
{
    auto up = env.getNull();
    for (const auto& st : statements_) {
        up = st->execute(env);
    }
    return up;
}


 ObjectPtr If::execute(Environment& env)
 {
     auto cond = condition_->execute(env);
     if (cond == env.getBool(true)) {
         return trueBranch_->execute(env);
     } else if (cond == env.getBool(false)) {
         return falseBranch_->execute(env);
     } else {
         throw std::runtime_error("bad if expression condition");
     }
 }


ObjectPtr Def::execute(Environment& env)
{
    env.store(name_, value_->execute(env));
    return env.getNull();
}


} // namespace ast
} // namespace lisp
