#pragma once

#include "common.hpp"
#include "memory.hpp"
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
        ;
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
    inline void format(std::ostream& out, int indent)
    {
        out << StrVal(indent, ' ');
    }
    virtual void serialize(std::ostream& out, int indent) = 0;
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


struct Integer : Value {
    using Rep = int32_t;
    Rep value_;
    ImmediateId cachedVal_;

    void serialize(std::ostream& out, int indent) override
    {
        out << "(integer " << value_ << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Double : Value {
    using Rep = double;
    Rep value_;
    ImmediateId cachedVal_;

    void serialize(std::ostream& out, int indent) override
    {
        out << "(double " << value_ << ')';
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct String : Value {
    StrVal value_;
    ImmediateId cachedVal_;

    void serialize(std::ostream& out, int indent) override
    {
        out << "(string " << value_ << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Null : Value {
    void serialize(std::ostream& out, int indent) override
    {
        out << "(null)";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override
    {
    }
};


struct True : Value {
    void serialize(std::ostream& out, int indent) override
    {
        out << "(true)";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override
    {
    }
};


struct False : Value {
    void serialize(std::ostream& out, int indent) override
    {
        out << "(false)";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override
    {
    }
};


struct LValue : Value {
    StrVal name_;
    VarLoc cachedVarLoc_;
    void serialize(std::ostream& out, int indent) override
    {
        out << "(lvalue " << name_ << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Lambda : Expr, Scope {
    Vector<StrVal> argNames_;
    Vector<Ptr<Statement>> statements_;
    void serialize(std::ostream& out, int indent) override
    {
        out << "(lambda\n";
        format(out, indent + 2);
        out << "(args";
        for (const auto& name : argNames_) {
            out << " " << name;
        }
        out << ")\n";
        format(out, indent + 2);
        out << "(statements\n";
        for (const auto& st : statements_) {
            format(out, indent + 4);
            st->serialize(out, indent + 4);
        }
        out << "))";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Application : Expr {
    StrVal target_;
    VarLoc cachedTargetLoc_;
    Vector<Ptr<Statement>> args_;
    void serialize(std::ostream& out, int indent) override
    {
        out << "(application " << target_;
        for (const auto& arg : args_) {
            out << "\n";
            format(out, indent + 2);
            arg->serialize(out, indent);
        }
        out << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Let : Expr, Scope {
    struct Binding : Node {
        StrVal name_;
        Ptr<Statement> value_;

        void serialize(std::ostream& out, int indent) override
        {
            out << "(binding " << name_ << " ";
            value_->serialize(out, indent);
            out << ")";
        }
        Heap::Ptr<Object> execute(Environment& env) override;
        void init(Environment&, Scope&) override;
    };

    Vector<Ptr<Binding>> bindings_;
    Vector<Ptr<Statement>> statements_;

    void serialize(std::ostream& out, int indent) override
    {
        out << "(let\n";
        format(out, indent + 2);
        out << "(bindings";
        for (const auto& binding : bindings_) {
            out << '\n';
            format(out, indent + 4);
            binding->serialize(out, indent + 4);
        }
        out << ")\n";
        format(out, indent + 2);
        out << "(statements";
        for (const auto& st : statements_) {
            out << '\n';
            format(out, indent + 4);
            st->serialize(out, indent + 4);
        }
        out << "))";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Begin : Expr {
    Vector<Ptr<Statement>> statements_;

    void serialize(std::ostream& out, int indent) override
    {
        out << "(begin";
        for (const auto& st : statements_) {
            out << '\n';
            format(out, indent + 2);
            st->serialize(out, indent + 2);
        }
        out << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct TopLevel : Begin, Scope {
    void serialize(std::ostream& out, int indent) override
    {
        out << "(toplevel";
        for (const auto& st : statements_) {
            out << '\n';
            format(out, indent + 2);
            st->serialize(out, indent + 2);
        }
        out << ")";
    }

    void init(Environment& env, Scope&) override;
};


struct If : Expr {
    Ptr<Statement> condition_;
    Ptr<Statement> trueBranch_;
    Ptr<Statement> falseBranch_;
    void serialize(std::ostream& out, int indent) override
    {
        out << "(if\n";
        format(out, indent + 2);
        condition_->serialize(out, indent + 2);
        out << '\n';
        format(out, indent + 2);
        trueBranch_->serialize(out, indent + 2);
        out << '\n';
        format(out, indent + 2);
        falseBranch_->serialize(out, indent + 2);
        out << '\n';
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


struct Def : Expr {
    StrVal name_;
    Ptr<Statement> value_;

    void serialize(std::ostream& out, int indent) override
    {
        out << "(def " << name_ << "\n";
        format(out, indent + 2);
        value_->serialize(out, indent + 2);
        out << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
    void init(Environment&, Scope&) override;
};


} // namespace ast
} // namespace lisp
