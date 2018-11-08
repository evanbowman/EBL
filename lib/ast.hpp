#pragma once

#include "memory.hpp"
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

    void serialize(std::ostream& out, int indent) override
    {
        out << "(integer " << value_ << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
};


struct String : Value {
    StrVal value_;

    void serialize(std::ostream& out, int indent) override
    {
        out << "(string " << value_ << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
};


struct Null : Value {
    void serialize(std::ostream& out, int indent) override
    {
        out << "(null)";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
};


struct LValue : Value {
    StrVal name_;
    void serialize(std::ostream& out, int indent) override
    {
        out << "(lvalue " << name_ << ")";
    }
    Heap::Ptr<Object> execute(Environment& env) override;
};


struct Lambda : Expr {
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
};


struct Application : Expr {
    StrVal target_;
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
};


struct Let : Expr {
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
};


} // namespace ast
} // namespace lisp
