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

    void unusedExpr();

    Bytecode result();

private:
    Bytecode data_;
};

enum class Opcode : uint8_t {
    Exit, // EXIT : stop execution and exit the vm.

    // CALL INSTRUCTIONS
    //
    // Invoke a function.
    //
    Call, // CALL(u8 argc) : invoke, consuming fn and args on stack

    Return, // RETURN : transfer control back to the caller

    Recur, // RECUR : re-invoke the current function, by recycling the
           // working environment frame. NOTE: A RECUR bytecode needs
           // to be preceeded by a number of EXITLET opcodes equal to
           // the number of let expressions within which the RECUR
           // form occurs. This is because ENTERLET advances the
           // environment frame without pushing onto the callstack
           // (because let opens an environment, but technically isn't
           // a function call).

    // JUMP INSTRUCTIONS
    //
    // Update the instruction pointer by a relative offset.
    //
    Jump,        // JUMP(u16 offset)
    JumpIfFalse, // JUMPIFFALSE(u16 offset) : consume stack top, jump if false

    // LOAD INSTRUCTIONS
    //
    // For loading values from the environment onto the operand
    // stack. This is a frequent operation, and the compiler does a
    // number of optimizations to cut down on the cost of loading,
    // because generically loading from the lisp stack, which is a
    // parent pointer tree, is costly.
    //
    // The *Fast opcodes use a single byte index, which works pretty
    // well actually, because most function call environments don't
    // have too many local variables, and the vm doesn't need to do
    // any bitwise operations to read a single byte, the way it does
    // for u16 parameters.
    //
    Load,      // LOAD(u16 frame_dist, u16 frame_offset)
    Load0,     // LOAD0(u16 frame_offset) : load from the current frame
    Load1,     // LOAD1(u16 frame_offset) : load from the parent frame
    Load2,     // LOAD2(u16 frame_offset) : load from the grandparent frame
    Load0Fast, // LOAD0FAST(u8 frame_offset) : load from current, small offset
    Load1Fast, // LOAD1FAST(u8 frame_offset) : load from parent, small offset

    Store, // STORE : move the top of the stack to the end of env frame.

    // PUSH INSTRUCTIONS
    //
    // Push constants onto the operand stack.
    // Immediates are compiled constants that live at locations in the
    // context given by u16 ids.
    //
    PushI,                // PUSHI(u16 id) : push immediate value onto stack
    PushNull,             // PUSHNULL : push the null constant onto the stack
    PushTrue,             // PUSHTRUE : push the true constant onto the stack
    PushFalse,            // PUSHFALSE : push the false constant onto the stack
    PushLambda,           // PUSHLAMBDA(u8 argc)
    PushDocumentedLambda, // PUSHDOCUMENTEDLAMBDA(u8 argc, u16 id)

    Pop, // POP : pop the top of the operand stack

    EnterLet, // ENTERLET : open a new env frame for the let expression
    ExitLet,  // EXITLET : pop the env frame associated with the let expr
};

} // namespace lisp
