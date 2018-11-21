#include "ast.hpp"
#include "lisp.hpp"
#include "listBuilder.hpp"
#include <iostream>
#include <sstream>


namespace lisp {
namespace ast {

using ExecutionFailure = std::runtime_error;

ObjectPtr Import::execute(Environment& env)
{
    return env.getNull();
}


ObjectPtr Integer::execute(Environment& env)
{
    return env.loadI(cachedVal_);
}


ObjectPtr Double::execute(Environment& env)
{
    return env.loadI(cachedVal_);
}


ObjectPtr String::execute(Environment& env)
{
    return env.loadI(cachedVal_);
}


ObjectPtr Null::execute(Environment& env)
{
    return env.getNull();
}


ObjectPtr True::execute(Environment& env)
{
    return env.getBool(true);
}


ObjectPtr False::execute(Environment& env)
{
    return env.getBool(false);
}


ObjectPtr LValue::execute(Environment& env)
{
    return env.load(cachedVarLoc_);
}


class InterpretedFunctionImpl : public Function::Impl {
public:
    InterpretedFunctionImpl(ast::Lambda* impl) : impl_(impl)
    {
    }

    ObjectPtr call(Environment& env, Arguments& args)
    {
        auto derived = env.derive();
        for (size_t i = 0; i < impl_->argNames_.size(); ++i) {
            derived->push(args[i]);
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
    const auto argc = argNames_.size();
    auto impl = make_unique<InterpretedFunctionImpl>(this);
    return env.create<lisp::Function>(
        [&]() -> ObjectPtr {
            if (not docstring_.empty()) {
                return env.create<lisp::String>(docstring_.c_str(),
                                                docstring_.length());
            } else {
                return env.getNull();
            }
        }(),
        argc, std::move(impl));
}


class InterpretedVariadicFunctionImpl : public Function::Impl {
public:
    InterpretedVariadicFunctionImpl(ast::VariadicLambda* impl) : impl_(impl)
    {
    }

    ObjectPtr call(Environment& env, Arguments& args)
    {
        auto derived = env.derive();
        for (size_t i = 0; i < impl_->argNames_.size() - 1; ++i) {
            derived->push(args[i]);
        }
        LazyListBuilder builder(env);
        for (size_t i = impl_->argNames_.size() - 1; i < args.size(); ++i) {
            builder.pushBack(args[i]);
        }
        derived->push(builder.result());
        auto up = env.getNull();
        for (auto& statement : impl_->statements_) {
            up = statement->execute(*derived);
        }
        return up;
    }

private:
    const ast::VariadicLambda* impl_;
};


ObjectPtr VariadicLambda::execute(Environment& env)
{
    const auto argc = argNames_.size() - 1;
    auto impl = make_unique<InterpretedVariadicFunctionImpl>(this);
    return env.create<lisp::Function>(
        [&]() -> ObjectPtr {
            if (not docstring_.empty()) {
                return env.create<lisp::String>(docstring_.c_str(),
                                                docstring_.length());
            } else {
                return env.getNull();
            }
        }(),
        argc, std::move(impl));
}


ObjectPtr Application::execute(Environment& env)
{
    try {
        auto loaded = checkedCast<lisp::Function>(toApply_->execute(env));
        Arguments args;
        for (const auto& arg : args_) {
            args.push_back(arg->execute(env));
        }
        return loaded->call(args);
    } catch (const std::exception& err) {
        std::stringstream fmt;
        fmt << "failed to apply \'";
        toApply_->store(fmt);
        fmt << "\', reason: " << err.what();
        throw ExecutionFailure(fmt.str());
    }
}


ObjectPtr Let::execute(Environment& env)
{
    auto derived = env.derive();
    for (const auto& binding : bindings_) {
        derived->push(binding.value_->execute(*derived));
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


ObjectPtr Cond::execute(Environment& env)
{
    for (auto& cCase : cases_) {
        if (cCase.condition_->execute(env) == env.getBool(true)) {
            auto up = env.getNull();
            for (auto& st : cCase.body_) {
                up = st->execute(env);
            }
            return up;
        }
    }
    return env.getNull();
}


ObjectPtr Or::execute(Environment& env)
{
    for (const auto& statement : statements_) {
        auto result = statement->execute(env);
        if (not(result == env.getBool(false))) {
            return result;
        }
    }
    return env.getBool(false);
}


ObjectPtr And::execute(Environment& env)
{
    ObjectPtr result = env.getBool(true);
    for (const auto& statement : statements_) {
        result = statement->execute(env);
        if (result == env.getBool(false)) {
            return env.getBool(false);
        }
    }
    return result;
}


ObjectPtr Def::execute(Environment& env)
{
    env.push(value_->execute(env));
    return env.getNull();
}


ObjectPtr Set::execute(Environment& env)
{
    env.store(cachedVarLoc_, value_->execute(env));
    return env.getNull();
}


ObjectPtr UserObject::execute(Environment& env)
{
    return value_;
}


void Import::init(Environment& env, Scope& scope)
{
    std::cout << "importing " << name_ << std::endl;
}


void String::init(Environment& env, Scope& scope)
{
    cachedVal_ =
        env.storeI(env.create<lisp::String>(value_.c_str(), value_.length()));
}


void Integer::init(Environment& env, Scope& scope)
{
    cachedVal_ = env.storeI(env.create<lisp::Integer>(value_));
}


void Double::init(Environment& env, Scope& scope)
{
    cachedVal_ = env.storeI(env.create<lisp::Double>(value_));
}


void LValue::init(Environment& env, Scope& scope)
{
    cachedVarLoc_ = scope.find(name_);
}


void Lambda::init(Environment& env, Scope& scope)
{
    Scope::setParent(&scope);
    for (const auto& argName : argNames_) {
        Scope::insert(argName);
    }
    for (const auto& statement : statements_) {
        statement->init(env, *this);
    }
}


void Application::init(Environment& env, Scope& scope)
{
    toApply_->init(env, scope);
    for (const auto& arg : args_) {
        arg->init(env, scope);
    }
}


void Let::init(Environment& env, Scope& scope)
{
    Scope::setParent(&scope);
    for (const auto& binding : bindings_) {
        Scope::insert(binding.name_);
        binding.value_->init(env, static_cast<Scope&>(*this));
    }
    for (const auto& statement : statements_) {
        statement->init(env, *this);
    }
}


void Begin::init(Environment& env, Scope& scope)
{
    for (const auto& statement : statements_) {
        statement->init(env, scope);
    }
}


void If::init(Environment& env, Scope& scope)
{
    condition_->init(env, scope);
    trueBranch_->init(env, scope);
    falseBranch_->init(env, scope);
}


void Cond::init(Environment& env, Scope& scope)
{
    for (auto& cCase : cases_) {
        cCase.condition_->init(env, scope);
        for (auto& st : cCase.body_) {
            st->init(env, scope);
        }
    }
}


void Or::init(Environment& env, Scope& scope)
{
    for (const auto& statement : statements_) {
        statement->init(env, scope);
    }
}


void And::init(Environment& env, Scope& scope)
{
    for (const auto& statement : statements_) {
        statement->init(env, scope);
    }
}


void Def::init(Environment& env, Scope& scope)
{
    scope.insert(name_);
    value_->init(env, scope);
}


void Set::init(Environment& env, Scope& scope)
{
    cachedVarLoc_ = scope.find(name_);
    value_->init(env, scope);
}


void TopLevel::init(Environment& env, Scope& scope)
{
    // The builtin functions wouldn't be visible to the compiler otherwise.
    const auto names = getBuiltinList();
    for (auto& name : names) {
        scope.insert(name);
    }
    Begin::init(env, scope);
}


void Import::store(OutputStream& out) const
{
    out << "(import " << name_ << ')';
}


void Integer::store(OutputStream& out) const
{
    out << value_;
}


void Double::store(OutputStream& out) const
{
    out << value_;
}


void String::store(OutputStream& out) const
{
    out << '\"' << value_ << '\"';
}


void Null::store(OutputStream& out) const
{
    out << "null";
}


void True::store(OutputStream& out) const
{
    out << "true";
}


void False::store(OutputStream& out) const
{
    out << "false";
}


void LValue::store(OutputStream& out) const
{
    out << name_;
}


void Lambda::store(OutputStream& out) const
{
    out << "(lambda (";
    for (auto& arg : argNames_) {
        out << arg << ' ';
    }
    out << ')';
    for (auto& st : statements_) {
        out << ' ';
        st->store(out);
    }
    out << ')';
}


void Application::store(OutputStream& out) const
{
    out << '(';
    toApply_->store(out);
    for (auto& arg : args_) {
        out << ' ';
        arg->store(out);
    }
    out << ')';
}


void Let::store(OutputStream& out) const
{
    out << "(let (";
    for (auto& binding : bindings_) {
        out << '(' << binding.name_ << ' ';
        binding.value_->store(out);
        out << ')';
    }
    out << ')';
    for (auto& st : statements_) {
        out << ' ';
        st->store(out);
    }
    out << ')';
}


void Begin::store(OutputStream& out) const
{
    out << "(begin";
    for (auto& st : statements_) {
        out << ' ';
        st->store(out);
    }
    out << ')';
}


void TopLevel::store(OutputStream& out) const
{
    for (auto& st : statements_) {
        out << ' ';
        st->store(out);
    }
}


void If::store(OutputStream& out) const
{
    out << "(if ";
    condition_->store(out);
    out << ' ';
    trueBranch_->store(out);
    out << ' ';
    falseBranch_->store(out);
    out << ')';
}


void Cond::store(OutputStream& out) const
{
    out << "(cond";
    for (auto& c : cases_) {
        out << '(';
        c.condition_->store(out);
        for (auto& st : c.body_) {
            out << ' ';
            st->store(out);
        }
        out << ')';
    }
    out << ')';
}


void Or::store(OutputStream& out) const
{
    out << "(or";
    for (auto& st : statements_) {
        out << ' ';
        st->store(out);
    }
    out << ')';
}


void And::store(OutputStream& out) const
{
    out << "(and";
    for (auto& st : statements_) {
        out << ' ';
        st->store(out);
    }
    out << ')';
}


void Def::store(OutputStream& out) const
{
    out << "(def " << name_ << ' ';
    value_->store(out);
    out << ')';
}


void Set::store(OutputStream& out) const
{
    out << "(set " << name_ << ' ';
    value_->store(out);
    out << ')';
}


void UserObject::store(OutputStream& out) const
{
    out << "\n;; Warning: could not serialize user object\n";
    out << 0;
}


} // namespace ast
} // namespace lisp
