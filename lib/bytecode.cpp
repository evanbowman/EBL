#include "bytecode.hpp"
#include "environment.hpp"
#include <cassert>
#include <iostream>

namespace lisp {

struct FunctionContext {
    size_t letCount_;
};

thread_local std::vector<FunctionContext> fnContexts;

Bytecode BytecodeBuilder::result()
{
    // Appending an Exit to the end of a sequence of expressions
    // allows new bytecode to be simply appended to old bytecode.
    data_.push_back((uint8_t)Opcode::Exit);
    return std::move(data_);
}

template <typename T> void writeParam(Bytecode& bc, const T& param)
{
    const auto bytes = (const uint8_t*)&param;
    for (size_t i = 0; i < sizeof(param); ++i) {
        bc.push_back(bytes[i]);
    }
}

void BytecodeBuilder::visit(ast::Integer& node)
{
    data_.push_back((uint8_t)Opcode::PushI);
    writeParam(data_, node.cachedVal_);
}

void BytecodeBuilder::visit(ast::Double& node)
{
    data_.push_back((uint8_t)Opcode::PushI);
    writeParam(data_, node.cachedVal_);
}

void BytecodeBuilder::visit(ast::Character& node)
{
    data_.push_back((uint8_t)Opcode::PushI);
    writeParam(data_, node.cachedVal_);
}

void BytecodeBuilder::visit(ast::String& node)
{
    data_.push_back((uint8_t)Opcode::PushI);
    writeParam(data_, node.cachedVal_);
}

void BytecodeBuilder::visit(ast::Null& node)
{
    data_.push_back((uint8_t)Opcode::PushNull);
}

void BytecodeBuilder::visit(ast::True& node)
{
    data_.push_back((uint8_t)Opcode::PushTrue);
}

void BytecodeBuilder::visit(ast::False& node)
{
    data_.push_back((uint8_t)Opcode::PushFalse);
}

void BytecodeBuilder::visit(ast::LValue& node)
{
    if (node.cachedVarLoc_.frameDist_ == 0) {
        if (node.cachedVarLoc_.offset_ < 256) {
            data_.push_back((uint8_t)Opcode::Load0Fast);
            writeParam(data_, (uint8_t)node.cachedVarLoc_.offset_);
        } else {
            data_.push_back((uint8_t)Opcode::Load0);
            writeParam(data_, node.cachedVarLoc_.offset_);
        }
    } else if (node.cachedVarLoc_.frameDist_ == 1) {
        if (node.cachedVarLoc_.offset_ < 256) {
            data_.push_back((uint8_t)Opcode::Load1Fast);
            writeParam(data_, (uint8_t)node.cachedVarLoc_.offset_);
        } else {
            data_.push_back((uint8_t)Opcode::Load1);
            writeParam(data_, node.cachedVarLoc_.offset_);
        }
    } else if (node.cachedVarLoc_.frameDist_ == 2) {
        data_.push_back((uint8_t)Opcode::Load2);
        writeParam(data_, node.cachedVarLoc_.offset_);
    } else {
        data_.push_back((uint8_t)Opcode::Load);
        writeParam(data_, node.cachedVarLoc_.frameDist_);
        writeParam(data_, node.cachedVarLoc_.offset_);
    }
}

void BytecodeBuilder::visit(ast::Lambda& node)
{
    fnContexts.push_back({0});
    assert(node.argNames_.size() < 256);
    if (node.docstring_.empty()) {
        data_.push_back((uint8_t)Opcode::PushLambda);
        data_.push_back((uint8_t)node.argNames_.size());
    } else {
        data_.push_back((uint8_t)Opcode::PushDocumentedLambda);
        data_.push_back((uint8_t)node.argNames_.size());
        writeParam(data_, node.cachedDocstringLoc_);
    }
    data_.push_back((uint8_t)Opcode::Jump);
    size_t jumpLoc = data_.size();
    writeParam(data_, (uint16_t)0);
    for (size_t i = 0; i < node.argNames_.size(); ++i) {
        data_.push_back((uint8_t)Opcode::Store);
    }
    for (auto& statement : node.statements_) {
        statement->visit(*this);
        data_.push_back((uint8_t)Opcode::Pop);
    }
    data_.pop_back();
    data_.push_back((uint8_t)Opcode::Return);
    uint16_t* jumpOffset = (uint16_t*)(&data_[jumpLoc]);
    const size_t offset = (data_.size() - 2) - jumpLoc;
    if (offset > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("jump offset exceeds allowed size");
    }
    *jumpOffset = offset;
    fnContexts.pop_back();
}

void BytecodeBuilder::visit(ast::VariadicLambda& node)
{
    throw std::runtime_error("unimplemented");
}

void BytecodeBuilder::visit(ast::Application& node)
{
    for (auto& arg : node.args_) {
        arg->visit(*this);
    }
    node.toApply_->visit(*this);
    data_.push_back((uint8_t)Opcode::Call);
    assert(node.args_.size() < 256);
    data_.push_back((uint8_t)node.args_.size());
}

void BytecodeBuilder::visit(ast::Let& node)
{
    if (not fnContexts.empty()) {
        ++fnContexts.back().letCount_;
    }
    data_.push_back((uint8_t)Opcode::EnterLet);
    for (auto& binding : node.bindings_) {
        binding.value_->visit(*this);
        data_.push_back((uint8_t)Opcode::Store);
    }
    for (auto& st : node.statements_) {
        st->visit(*this);
        data_.push_back((uint8_t)Opcode::Pop);
    }
    data_.pop_back();
    data_.push_back((uint8_t)Opcode::ExitLet);
    if (not fnContexts.empty()) {
        --fnContexts.back().letCount_;
    }
}

void BytecodeBuilder::visit(ast::TopLevel& node)
{
    for (auto& st : node.statements_) {
        st->visit(*this);
        data_.push_back((uint8_t)Opcode::Pop);
    }
}

void BytecodeBuilder::visit(ast::Namespace& node)
{
    for (auto& st : node.statements_) {
        st->visit(*this);
        data_.push_back((uint8_t)Opcode::Pop);
    }
    data_.pop_back();
}

void BytecodeBuilder::visit(ast::Begin& node)
{
    for (auto& st : node.statements_) {
        st->visit(*this);
        data_.push_back((uint8_t)Opcode::Pop);
    }
    // A begin expression needs to return a value, so remove the last
    // Opcode::Pop, in order to keep the result on the operand stack.
    data_.pop_back();
}

void BytecodeBuilder::visit(ast::If& node)
{
    // Condition, then conditionally branch over the true block
    node.condition_->visit(*this);
    data_.push_back((uint8_t)Opcode::JumpIfFalse);
    const size_t jumpOffset1Loc = data_.size();
    writeParam(data_, (uint16_t)0);
    // True block, then unconditionally branch over the false block
    node.trueBranch_->visit(*this);
    data_.push_back((uint8_t)Opcode::Jump);
    const size_t jumpOffset2Loc = data_.size();
    writeParam(data_, (uint16_t)0);
    // False block
    node.falseBranch_->visit(*this);
    uint16_t* jumpOffset1 = (uint16_t*)(&data_[jumpOffset1Loc]);
    uint16_t* jumpOffset2 = (uint16_t*)(&data_[jumpOffset2Loc]);
    // size of the true block
    const size_t j1result = jumpOffset2Loc - jumpOffset1Loc;
    // size of the false block
    const size_t j2result = (data_.size() - 2) - jumpOffset2Loc;
    if (j1result > std::numeric_limits<uint16_t>::max() or
        j2result > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("jump offset exceeds allowed size");
    }
    *jumpOffset1 = j1result;
    *jumpOffset2 = j2result;
}

void BytecodeBuilder::visit(ast::Cond& node)
{
    throw std::runtime_error("cond unimplemented");
}

void BytecodeBuilder::visit(ast::Or& node)
{
    throw std::runtime_error("or unimplemented");
}

void BytecodeBuilder::visit(ast::And& node)
{
    throw std::runtime_error("and unimplemented");
}

void BytecodeBuilder::visit(ast::Def& node)
{
    node.value_->visit(*this);
    data_.push_back((uint8_t)Opcode::Store);
    data_.push_back((uint8_t)Opcode::PushNull);
}

void BytecodeBuilder::visit(ast::Recur& node)
{
    for (auto& arg : node.args_) {
        arg->visit(*this);
    }
    // If recur is used within a let environment nested within a
    // function, we need to exit the nested environments before
    // re-playing the function.
    for (size_t i = 0; i < fnContexts.back().letCount_; ++i) {
        data_.push_back((uint8_t)Opcode::ExitLet);
    }
    data_.push_back((uint8_t)Opcode::Recur);
}

void BytecodeBuilder::visit(ast::Set& node)
{
    throw std::runtime_error("set unimplemented");
}

void BytecodeBuilder::visit(ast::UserObject& node)
{
    puts("TODO: userobject");
}

} // namespace lisp
