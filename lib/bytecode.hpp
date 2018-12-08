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
    Recur,
    Return,

    // CALL INSTRUCTIONS
    //
    // Invoke a function.
    //
    Call,       // CALL(u8 argc) : invoke, consuming fn and args on stack
    CallCached, // CallCached(u8 cacheId) : After a bytecode call, the
                // vm will cache the call info, and if the same object
                // pointer arrives at the callsite, the vm will jump
                // immediatly without doing any type or argument
                // checking.

    // JUMP INSTRUCTIONS
    //
    // Update the instruction pointer by a relative offset.
    //
    Jump,             // JUMP(u16 offset)
    JumpIfFalse,      // JUMPIFFALSE(u16 offset) : consume stack top, jump if false

    // LOAD INSTRUCTIONS
    //
    // For loading values from the environment onto the operand
    // stack. This is a frequent operation, and the compiler does a
    // number of optimizations to cut down on the cost of loading,
    // because generically loading from the lisp stack, which is a
    // parent pointer tree, is costly.
    //
    Load,      // LOAD(u16 frame_dist, u16 frame_offset)
    Load0,     // LOAD0(u16 frame_offset) : load from the current frame
    Load1,     // LOAD1(u16 frame_offset) : load from the parent frame
    Load2,     // LOAD2(u16 frame_offset) : load from the grandparent frame
    Load0Fast, // LOAD0FAST(u8 frame_offset) : load from current, small offset
    Load1Fast, // LOAD1FAST(u8 frame_offset) : load from parent, small offset

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
