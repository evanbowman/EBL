#pragma once

#include "common.hpp"
#include "memory.hpp"
#include "utility.hpp"
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace lisp {

class Object;

namespace ast {

template <typename T> using Ptr = std::unique_ptr<T>;
template <typename T> using Vector = std::vector<T>;
using StrVal = std::string;
using Error = std::runtime_error;


class Scope {
private:
    struct Variable {
        StrVal name_;
        bool isMutable_;
    };

public:
    void setParent(Scope* parent)
    {
        parent_ = parent;
    }


    inline StackLoc insert(const std::string& varName, bool isMutable = false)
    {
        if (variables_.size() > std::numeric_limits<StackLoc>::max()) {
            throw std::runtime_error("Too many variables in environment");
        }
        const StackLoc ret = variables_.size();
        for (const auto& var : variables_) {
            if (var.name_ == varName) {
                throw Error("redefinition of variable " + varName +
                            " not allowed");
            }
        }
        variables_.push_back({varName, isMutable});
        return ret;
    }

    struct FindResult {
        VarLoc varLoc_;
        const Scope* owner_;
        bool isMutable_;
    };

    FindResult find(const StrVal& varPath, FrameDist traversed = 0) const;


    FindResult find(const Vector<StrVal>& varNamePatterns,
                    FrameDist traversed = 0) const;

    inline Scope* getParent() const
    {
        return parent_;
    }

private:
    Scope* parent_ = nullptr;
    Vector<Variable> variables_;
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


struct Literal : Value {
    ImmediateId cachedVal_;

    void visit(Visitor& visitor) override;
};


struct Integer : Literal {
    using Rep = int32_t;
    Rep value_;

    void init(Environment& env, Scope& scope) override;
};


struct Float : Literal {
    using Rep = double;
    Rep value_;

    void init(Environment& env, Scope& scope) override;
};


struct Character : Literal {
    using Rep = WideChar;
    Rep value_;

    void init(Environment& env, Scope& scope) override;
};


struct String : Literal {
    StrVal value_;

    void init(Environment& env, Scope& scope) override;
};


struct Symbol : Literal {
    StrVal value_;

    void init(Environment& env, Scope& scope) override;
};


struct List : Literal {
    std::vector<ast::Ptr<Literal>> contents_;

    void init(Environment& env, Scope& scope) override;
};


struct Pair : Literal {
    ast::Ptr<Literal> first_;
    ast::Ptr<Literal> second_;

    void init(Environment& env, Scope& scope) override;
};


struct Null : Value {
    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override{};
};


struct True : Value {
    void visit(Visitor& visitor) override;
    void init(Environment&, Scope&) override{};
};


struct False : Value {
    void visit(Visitor& visitor) override;
    void init(Environment&, Scope&) override{};
};


struct Namespace : Expr {
    StrVal name_;
    Vector<Ptr<Statement>> statements_;

    void visit(Visitor& visitor) override;
    void init(Environment&, Scope&) override;
};


struct LValue : Value {
    StrVal name_;
    Scope::FindResult cachedVarInfo_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct Lambda : Expr, Scope {
    Vector<StrVal> argNames_;
    Vector<Ptr<Statement>> statements_;
    StrVal docstring_;
    ImmediateId cachedDocstringLoc_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct VariadicLambda : Lambda {
    void visit(Visitor& visitor) override;
};


struct Application : Expr {
    Ptr<Statement> toApply_;
    Vector<Ptr<Statement>> args_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct Let : Expr, Scope {
    struct Binding {
        StrVal name_;
        Ptr<Statement> value_;
    };

    Vector<Binding> bindings_;
    Vector<Ptr<Statement>> statements_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct LetMut : Let {
    void init(Environment& env, Scope& scope) override;
};


struct Begin : Expr {
    Vector<Ptr<Statement>> statements_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct TopLevel : Begin, Scope {
    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct If : Expr {
    Ptr<Statement> condition_;
    Ptr<Statement> trueBranch_;
    Ptr<Statement> falseBranch_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct Recur : Expr {
    Vector<Ptr<Statement>> args_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct Or : Expr {
    std::vector<Ptr<Statement>> statements_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct And : Expr {
    std::vector<Ptr<Statement>> statements_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct Def : Expr {
    StrVal name_;
    Ptr<Statement> value_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


struct DefMut : Def {
    void init(Environment& env, Scope& scope) override;
};

struct Set : Expr {
    StrVal name_;
    Ptr<Statement> value_;
    VarLoc cachedVarLoc_;

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override;
};


// Users of the library might want to push their own data into the environment,
// but
struct UserObject : Statement {
    ImmediateId varLoc_;

    UserObject(ImmediateId loc) : varLoc_(loc)
    {
    }

    void visit(Visitor& visitor) override;
    void init(Environment& env, Scope& scope) override{};
};


class Visitor {
public:
    virtual ~Visitor()
    {
    }

    virtual void visit(Namespace& node) = 0;
    virtual void visit(Literal& node) = 0;
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
    virtual void visit(Or& node) = 0;
    virtual void visit(And& node) = 0;
    virtual void visit(Def& node) = 0;
    virtual void visit(Set& node) = 0;
    virtual void visit(UserObject& node) = 0;
};

} // namespace ast
} // namespace lisp
