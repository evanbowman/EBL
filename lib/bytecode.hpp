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
    Exit,        // exit
    Jump,        // jump(relative-addr)
    JumpIfFalse, // jumpIfFalse(relative-addr) ; If top of stack is false,
                 // branch to addr
    Call,        // call(argcount)
    Recur,       // recur
    Return,      // return
    Load,        // load(stackloc)
    Store,       // store                     ; operandStack -> frame's stack
    PushI,       // pushI(immediateId)
    PushNull,    // pushNull
    PushTrue,    // pushTrue
    PushFalse,   // pushFalse
    PushLambda,  // pushLambda(argc)
    Pop,         // pop                       ; operandStack -1
    EnterLet,    // enterLet
    ExitLet,     // exitLet
};

} // namespace lisp
