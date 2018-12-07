#pragma once

#include "ast.hpp"
#include "common.hpp"
#include <vector>


namespace lisp {

class Context;

class BytecodeBuilder : public ast::Visitor {
public:
    void visit(ast::Namespace& node) override;
    void visit(ast::Integer& node) override;
    void visit(ast::Double& node) override;
    void visit(ast::Character& node) override;
    void visit(ast::String& node) override;
    void visit(ast::Null& node) override;
    void visit(ast::True& node) override;
    void visit(ast::False& node) override;
    void visit(ast::LValue& node) override;
    void visit(ast::Lambda& node) override;
    void visit(ast::VariadicLambda& node) override;
    void visit(ast::Application& node) override;
    void visit(ast::Let& node) override;
    void visit(ast::TopLevel& node) override;
    void visit(ast::Begin& node) override;
    void visit(ast::If& node) override;
    void visit(ast::Cond& node) override;
    void visit(ast::Or& node) override;
    void visit(ast::And& node) override;
    void visit(ast::Def& node) override;
    void visit(ast::Set& node) override;
    void visit(ast::UserObject& node) override;
    void visit(ast::Recur& node) override;

    Bytecode result();

private:
    Bytecode data_;
};

enum class Opcode : uint8_t {
    Exit,
    Jump,
    JumpIfFalse,
    Call,
    Recur,
    Return,
    Load0,
    Load1,
    Load2,
    Load,
    Store,
    PushI,
    PushNull,
    PushTrue,
    PushFalse,
    PushLambda,
    Pop,
    EnterLet,
    ExitLet,
};

} // namespace lisp
