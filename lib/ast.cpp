#include "ast.hpp"
#include "lisp.hpp"
#include "listBuilder.hpp"
#include "utility.hpp"
#include <sstream>


namespace lisp {
namespace ast {

// FIXME: shouldn't be global... maybe put in Context or Environment...
thread_local Vector<StrVal*> namespacePath;
thread_local Vector<Lambda*> currentFunction;

Scope::FindResult Scope::find(const Vector<StrVal>& varNamePatterns,
                              FrameDist traversed) const
{
    for (StackLoc i = 0; i < variables_.size(); ++i) {
        for (auto& pattern : varNamePatterns) {
            if (variables_[i].name_ == pattern) {
                return {{traversed, i}, this, variables_[i].isMutable_};
            }
        }
    }
    if (parent_) {
        return parent_->find(varNamePatterns, traversed + 1);
    } else {
        throw Error("variable " + varNamePatterns.back() +
                    " is not visible in the current environment");
    }
}

Scope::FindResult Scope::find(const StrVal& varNamePath, FrameDist traversed) const
{
    for (StackLoc i = 0; i < variables_.size(); ++i) {
        if (variables_[i].name_ == varNamePath) {
            return {{traversed, i}, this, variables_[i].isMutable_};
        }
    }
    if (parent_) {
        return parent_->find(varNamePath, traversed + 1);
    } else {
        throw Error("variable " + varNamePath +
                    " is not visible in the current environment");
    }
}


using ExecutionFailure = std::runtime_error;

void Integer::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Double::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Character::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void String::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Null::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void True::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void False::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Namespace::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void LValue::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Lambda::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void VariadicLambda::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Application::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Let::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Begin::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void TopLevel::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void If::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Recur::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Cond::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Or::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void And::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Def::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Set::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void UserObject::visit(Visitor& visitor)
{
    visitor.visit(*this);
}

void Namespace::init(Environment& env, Scope& scope)
{
    if (not currentFunction.empty()) {
        throw std::runtime_error("namespace only allowed in top level");
    }
    namespacePath.push_back(&name_);
    for (auto& statement : statements_) {
        statement->init(env, scope);
    }
    namespacePath.pop_back();
}


void Character::init(Environment& env, Scope& scope)
{
    cachedVal_ = env.getContext()->storeI<lisp::Character>(value_);
}


void String::init(Environment& env, Scope& scope)
{
    cachedVal_ = env.getContext()->storeI<lisp::String>(value_);
}


void Integer::init(Environment& env, Scope& scope)
{
    cachedVal_ = env.getContext()->storeI<lisp::Integer>(value_);
}


void Double::init(Environment& env, Scope& scope)
{
    cachedVal_ = env.getContext()->storeI<lisp::Double>(value_);
}

// This is a potential area for improvement. In order to look up a variable, we
// need to generate paths based on all the ascending namespaces where the
// variable might exist, otherwise, code that refrences variables within a
// namespace would need to use the full qualified path.
static Vector<StrVal> makeNsPatterns(const StrVal& varName)
{
    Vector<StrVal> patterns;
    StrVal builder;
    for (int i = namespacePath.size() - 1; i > -1; --i) {
        builder.clear();
        for (int j = 0; j < i + 1; ++j) {
            builder += *namespacePath[j];
            builder += "::";
        }
        builder += varName;
        patterns.push_back(builder);
    }
    patterns.push_back(varName);
    return patterns;
}

static void validateIdentifier(const StrVal& name)
{
    if (name.find("::") not_eq StrVal::npos) {
        throw std::runtime_error(
            "identifiers may not contain a scope operator \'::\'");
    }
}

void LValue::init(Environment& env, Scope& scope)
{
    const auto patterns = makeNsPatterns(name_);
    cachedVarInfo_ = scope.find(patterns);
}


void Lambda::init(Environment& env, Scope& scope)
{
    currentFunction.push_back(this);
    dynamicWind(
        [&] {
            if (not docstring_.empty()) {
                cachedDocstringLoc_ = env.getContext()->storeI<lisp::String>(docstring_);
            }
            Scope::setParent(&scope);
            for (auto it = argNames_.rbegin(); it != argNames_.rend(); ++it) {
                validateIdentifier(*it);
                Scope::insert(*it);
            }
            for (const auto& statement : statements_) {
                statement->init(env, *this);
            }
        },
        [&] {
            currentFunction.pop_back();
        });
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
        binding.value_->init(env, *this);
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


void Recur::init(Environment& env, Scope& scope)
{
    if (currentFunction.empty()) {
        throw std::runtime_error("recur isn\'t allowed outside of a function");
    }
    if (args_.size() not_eq currentFunction.back()->argNames_.size()) {
        throw std::runtime_error("wrong number of args supplied to recur");
    }
    for (auto& arg : args_) {
        arg->init(env, scope);
    }
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
    validateIdentifier(name_);
    StrVal fullName;
    for (auto name : namespacePath) {
        fullName += *name;
        fullName += "::";
    }
    fullName += name_;
    scope.insert(fullName);
    value_->init(env, scope);
}


void DefMut::init(Environment& env, Scope& scope)
{
    validateIdentifier(name_);
    StrVal fullName;
    for (auto name : namespacePath) {
        fullName += *name;
        fullName += "::";
    }
    fullName += name_;
    scope.insert(fullName, true);
    value_->init(env, scope);
}


void Set::init(Environment& env, Scope& scope)
{
    const auto patterns = makeNsPatterns(name_);
    auto found = scope.find(patterns);
    if (not found.isMutable_) {
        throw std::runtime_error("failed to rebind immutable variable " + name_);
    }
    cachedVarLoc_ = found.varLoc_;
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


} // namespace ast
} // namespace lisp
