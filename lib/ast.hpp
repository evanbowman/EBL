#pragma once

#include "common.hpp"
#include "memory.hpp"
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace lisp {

class Environment;
class Object;


namespace ast {

template <typename T> using Ptr = std::unique_ptr<T>;
template <typename T> using Vector = std::vector<T>;
using StrVal = std::string;
using Error = std::runtime_error;

struct TopLevel;

class Scope {
public:
    void setParent(Scope* parent)
    {
        parent_ = parent;
    }


    StackLoc insert(const std::string& varName)
    {
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


    VarLoc find(const std::string& varName, FrameDist traversed = 0) const
    {
        for (StackLoc i = 0; i < varNames_.size(); ++i) {
            if (varNames_[i] == varName) {
                return {traversed, i};
            }
        }
        if (parent_) {
            return parent_->find(varName, traversed + 1);
        } else {
            throw Error("variable " + varName +
                        " is not visible in the current environment");
        }
    }

private:
    Scope* parent_ = nullptr;
    std::vector<std::string> varNames_;
};


struct Node {
    virtual ~Node()
    {
    }
    inline void format(std::ostream& out, int indent) const
    {
        out << StrVal(indent, ' ');
    }
    virtual Heap::Ptr<Object> execute(Environment& env) = 0;

    // Resolve the stack addresses for variables accesses, and initialize
    // constants that we'll need while executing the program. It's possible
    // to save a lot of memory by caching literals.
    virtual void init(Environment&, Scope&) = 0;
};


struct Statement : Node {
};


struct Expr : Statement {
};


struct Value : Statement {
};


struct Import : Expr {
    StrVal name_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Integer : Value {
    using Rep = int32_t;
    Rep value_;
    ImmediateId cachedVal_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Double : Value {
    using Rep = double;
    Rep value_;
    ImmediateId cachedVal_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct String : Value {
    StrVal value_;
    ImmediateId cachedVal_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Null : Value {
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override
    {
    }
};


struct True : Value {
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override
    {
    }
};


struct False : Value {
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override
    {
    }
};


struct LValue : Value {
    StrVal name_;
    VarLoc cachedVarLoc_;
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Lambda : Expr, Scope {
    Vector<StrVal> argNames_;
    Vector<Ptr<Statement>> statements_;
    StrVal docstring_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Application : Expr {
    Ptr<Statement> toApply_;
    Vector<Ptr<Statement>> args_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Let : Expr, Scope {
    struct Binding : Node {
        StrVal name_;
        Ptr<Statement> value_;

        Heap::Ptr<Object> execute(Environment& env) override;
        void init(Environment&, Scope&) override;
    };

    Vector<Ptr<Binding>> bindings_;
    Vector<Ptr<Statement>> statements_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Begin : Expr {
    Vector<Ptr<Statement>> statements_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct TopLevel : Begin, Scope {
    void init(Environment& env, Scope&) override;
};


struct If : Expr {
    Ptr<Statement> condition_;
    Ptr<Statement> trueBranch_;
    Ptr<Statement> falseBranch_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Or : Expr {
    std::vector<Ptr<Statement>> statements_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct And : Expr {
    std::vector<Ptr<Statement>> statements_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Def : Expr {
    StrVal name_;
    Ptr<Statement> value_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Set : Expr {
    StrVal name_;
    Ptr<Statement> value_;
    VarLoc cachedVarLoc_;

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


// Users of the library might want to push their own data into the environment,
// but
struct UserObject : Statement {
    Heap::Ptr<Object> value_;

    UserObject(Heap::Ptr<Object> value) : value_(value)
    {
    }

    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override
    {
    }
};


} // namespace ast
} // namespace lisp
