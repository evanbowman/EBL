#pragma once

#include "common.hpp"
#include "memory.hpp"
#include "utility.hpp"
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include <limits>

namespace lisp {

class Object;

namespace ast {

template <typename T> using Ptr = std::unique_ptr<T>;
template <typename T> using Vector = std::vector<T>;
using StrVal = std::string;
using Error = std::runtime_error;
using OutputStream = std::ostream;


class Scope {
public:
    void setParent(Scope* parent)
    {
        parent_ = parent;
    }


    StackLoc insert(const std::string& varName)
    {
        if (varNames_.size() > std::numeric_limits<StackLoc>::max()) {
            throw std::runtime_error("Too many variables in environment");
        }
        const StackLoc ret = varNames_.size();
        for (const auto& name : varNames_) {
            if (name == varName) {
                throw Error("redefinition of variable " + varName +
                            "not allowed");
            }
        }
        varNames_.push_back(varName);
        return ret;
    }


    VarLoc find(const Vector<StrVal>& varNamePatterns,
                FrameDist traversed = 0) const;

private:
    Scope* parent_ = nullptr;
    std::vector<std::string> varNames_;
};


class Visitor;


struct Node {
    virtual ~Node()
    {
    }

    virtual void visit(Visitor& visitor) = 0;
    virtual void init(Environment& env, Scope& scope) = 0;
};


struct Statement : Node {
};


struct Expr : Statement {
};


struct Value : Statement {
};


struct Integer : Value {
    using Rep = int32_t;
    Rep value_;
    ImmediateId cachedVal_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Double : Value {
    using Rep = double;
    Rep value_;
    ImmediateId cachedVal_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Character : Value {
    using Rep = WideChar;
    Rep value_;
    ImmediateId cachedVal_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct String : Value {
    StrVal value_;
    ImmediateId cachedVal_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Null : Value {
    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override{};
};


struct True : Value {
    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment&, Scope&) override{};
};


struct False : Value {
    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment&, Scope&) override{};
};


struct Namespace : Expr {
    StrVal name_;
    Vector<Ptr<Statement>> statements_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment&, Scope&) override;
};


struct LValue : Value {
    StrVal name_;
    VarLoc cachedVarLoc_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Lambda : Expr, Scope {
    Vector<StrVal> argNames_;
    Vector<Ptr<Statement>> statements_;
    StrVal docstring_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct VariadicLambda : Lambda {
    virtual void visit(Visitor& visitor) override;
};


struct Application : Expr {
    Ptr<Statement> toApply_;
    Vector<Ptr<Statement>> args_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Let : Expr, Scope {
    struct Binding {
        StrVal name_;
        Ptr<Statement> value_;
    };

    Vector<Binding> bindings_;
    Vector<Ptr<Statement>> statements_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Begin : Expr {
    Vector<Ptr<Statement>> statements_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct TopLevel : Begin, Scope {
    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct If : Expr {
    Ptr<Statement> condition_;
    Ptr<Statement> trueBranch_;
    Ptr<Statement> falseBranch_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Recur : Expr {
    Vector<Ptr<Statement>> args_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Cond : Expr {
    struct Case {
        Ptr<Statement> condition_;
        std::vector<Ptr<Statement>> body_;
    };

    std::vector<Case> cases_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Or : Expr {
    std::vector<Ptr<Statement>> statements_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct And : Expr {
    std::vector<Ptr<Statement>> statements_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Def : Expr {
    StrVal name_;
    Ptr<Statement> value_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


struct Set : Expr {
    StrVal name_;
    Ptr<Statement> value_;
    VarLoc cachedVarLoc_;

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override;
};


// Users of the library might want to push their own data into the environment,
// but
struct UserObject : Statement {
    Heap::Ptr<Object> value_;

    UserObject(Heap::Ptr<Object> value) : value_(value)
    {
    }

    virtual void visit(Visitor& visitor) override;
    virtual void init(Environment& env, Scope& scope) override{};
};


class Visitor {
public:
    virtual ~Visitor()
    {
    }

    virtual void visit(Namespace& node) = 0;
    virtual void visit(Integer& node) = 0;
    virtual void visit(Double& node) = 0;
    virtual void visit(Character& node) = 0;
    virtual void visit(String& node) = 0;
    virtual void visit(Null& node) = 0;
    virtual void visit(True& node) = 0;
    virtual void visit(False& node) = 0;
    virtual void visit(LValue& node) = 0;
    virtual void visit(Lambda& node) = 0;
    virtual void visit(VariadicLambda& node) = 0;
    virtual void visit(Application& node) = 0;
    virtual void visit(Let& node) = 0;
    virtual void visit(TopLevel& node) = 0;
    virtual void visit(Begin& node) = 0;
    virtual void visit(If& node) = 0;
    virtual void visit(Recur& node) = 0;
    virtual void visit(Cond& node) = 0;
    virtual void visit(Or& node) = 0;
    virtual void visit(And& node) = 0;
    virtual void visit(Def& node) = 0;
    virtual void visit(Set& node) = 0;
    virtual void visit(UserObject& node) = 0;
};

} // namespace ast
} // namespace lisp
