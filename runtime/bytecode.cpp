#include "bytecode.hpp"
#include "environment.hpp"
#include <cassert>

namespace ebl {

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

template <Opcode op> void writeOp(Bytecode& bc)
{
    bc.push_back(static_cast<uint8_t>(op));
}

void BytecodeBuilder::visit(ast::Literal& node)
{
    writeOp<Opcode::PushI>(data_);
    writeParam(data_, node.cachedVal_);
}

void BytecodeBuilder::visit(ast::Null& node)
{
    writeOp<Opcode::PushNull>(data_);
}

void BytecodeBuilder::visit(ast::True& node)
{
    writeOp<Opcode::PushTrue>(data_);
}

void BytecodeBuilder::visit(ast::False& node)
{
    writeOp<Opcode::PushFalse>(data_);
}

void BytecodeBuilder::visit(ast::LValue& node)
{
    const auto varloc = node.cachedVarInfo_.varLoc_;
    if (varloc.frameDist_ == 0) {
        if (varloc.offset_ < 256) {
            writeOp<Opcode::Load0Fast>(data_);
            writeParam(data_, (uint8_t)varloc.offset_);
        } else {
            writeOp<Opcode::Load0>(data_);
            writeParam(data_, varloc.offset_);
        }
    } else if (varloc.frameDist_ == 1) {
        if (varloc.offset_ < 256) {
            writeOp<Opcode::Load1Fast>(data_);
            writeParam(data_, (uint8_t)varloc.offset_);
        } else {
            writeOp<Opcode::Load1>(data_);
            writeParam(data_, varloc.offset_);
        }
    } else if (varloc.frameDist_ == 2) {
        writeOp<Opcode::Load2>(data_);
        writeParam(data_, varloc.offset_);
    } else {
        writeOp<Opcode::Load>(data_);
        writeParam(data_, varloc.frameDist_);
        writeParam(data_, varloc.offset_);
    }
}

void BytecodeBuilder::visit(ast::Set& node)
{
    node.value_->visit(*this);
    writeOp<Opcode::Rebind>(data_);
    writeParam(data_, node.cachedVarLoc_.frameDist_);
    writeParam(data_, node.cachedVarLoc_.offset_);
    writeOp<Opcode::PushNull>(data_);
}

void BytecodeBuilder::visit(ast::Lambda& node)
{
    fnContexts.push_back({0});
    assert(node.argNames_.size() < 256);
    if (node.docstring_.empty()) {
        writeOp<Opcode::PushLambda>(data_);
        data_.push_back((uint8_t)node.argNames_.size());
    } else {
        writeOp<Opcode::PushDocumentedLambda>(data_);
        data_.push_back((uint8_t)node.argNames_.size());
        writeParam(data_, node.cachedDocstringLoc_);
    }
    writeOp<Opcode::Jump>(data_);
    size_t jumpLoc = data_.size();
    writeParam(data_, (uint16_t)0);
    for (size_t i = 0; i < node.argNames_.size(); ++i) {
        writeOp<Opcode::Store>(data_);
    }
    for (auto& statement : node.statements_) {
        statement->visit(*this);
        writeOp<Opcode::Discard>(data_);
    }
    data_.pop_back();
    writeOp<Opcode::Return>(data_);
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
    // FIXME: This is mostly a shameless copy-paste, refactor!
    fnContexts.push_back({0});
    assert(node.argNames_.size() < 256);
    if (node.docstring_.empty()) {
        writeOp<Opcode::PushVariadicLambda>(data_);
        data_.push_back((uint8_t)node.argNames_.size());
    } else {
        throw std::runtime_error("TODO: documentedVariadicLambda");
        writeOp<Opcode::PushDocumentedLambda>(data_);
        data_.push_back((uint8_t)node.argNames_.size());
        writeParam(data_, node.cachedDocstringLoc_);
    }
    writeOp<Opcode::Jump>(data_);
    size_t jumpLoc = data_.size();
    writeParam(data_, (uint16_t)0);
    for (size_t i = 0; i < node.argNames_.size(); ++i) {
        writeOp<Opcode::Store>(data_);
    }
    for (auto& statement : node.statements_) {
        statement->visit(*this);
        writeOp<Opcode::Discard>(data_);
    }
    data_.pop_back();
    writeOp<Opcode::Return>(data_);
    uint16_t* jumpOffset = (uint16_t*)(&data_[jumpLoc]);
    const size_t offset = (data_.size() - 2) - jumpLoc;
    if (offset > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("jump offset exceeds allowed size");
    }
    *jumpOffset = offset;
    fnContexts.pop_back();
}

void BytecodeBuilder::visit(ast::Application& node)
{
    if (auto lval = dynamic_cast<ast::LValue*>(node.toApply_.get())) {
        const bool isTopLevel =
            lval->cachedVarInfo_.owner_->getParent() == nullptr;
        if (isTopLevel) {
            if (lval->name_ == "cons") {
                if (node.args_.size() not_eq 2) {
                    throw std::runtime_error("wrong number of args to cons");
                }
                node.args_[0]->visit(*this);
                node.args_[1]->visit(*this);
                writeOp<Opcode::Cons>(data_);
                return;
            } else if (lval->name_ == "car") {
                if (node.args_.size() not_eq 1) {
                    throw std::runtime_error("wrong number of args to car");
                }
                node.args_[0]->visit(*this);
                writeOp<Opcode::Car>(data_);
                return;
            } else if (lval->name_ == "cdr") {
                if (node.args_.size() not_eq 1) {
                    throw std::runtime_error("wrong number of args to cdr");
                }
                node.args_[0]->visit(*this);
                writeOp<Opcode::Cdr>(data_);
                return;
            } else if (lval->name_ == "null?") {
                if (node.args_.size() not_eq 1) {
                    throw std::runtime_error("wrong number of args to null?");
                }
                node.args_[0]->visit(*this);
                writeOp<Opcode::IsNull>(data_);
                return;
            }
        }
    }
    for (auto& arg : node.args_) {
        arg->visit(*this);
    }
    node.toApply_->visit(*this);
    writeOp<Opcode::Call>(data_);
    assert(node.args_.size() < 256);
    data_.push_back((uint8_t)node.args_.size());
}

void BytecodeBuilder::visit(ast::Let& node)
{
    if (not fnContexts.empty()) {
        ++fnContexts.back().letCount_;
    }
    writeOp<Opcode::EnterLet>(data_);
    for (auto& binding : node.bindings_) {
        binding.value_->visit(*this);
        writeOp<Opcode::Store>(data_);
    }
    for (auto& st : node.statements_) {
        st->visit(*this);
        writeOp<Opcode::Discard>(data_);
    }
    data_.pop_back();
    writeOp<Opcode::ExitLet>(data_);
    if (not fnContexts.empty()) {
        --fnContexts.back().letCount_;
    }
}

void BytecodeBuilder::visit(ast::TopLevel& node)
{
    for (auto& st : node.statements_) {
        st->visit(*this);
        writeOp<Opcode::Discard>(data_);
    }
}

void BytecodeBuilder::visit(ast::Namespace& node)
{
    for (auto& st : node.statements_) {
        st->visit(*this);
        writeOp<Opcode::Discard>(data_);
    }
    data_.pop_back();
}

void BytecodeBuilder::visit(ast::Begin& node)
{
    for (auto& st : node.statements_) {
        st->visit(*this);
        writeOp<Opcode::Discard>(data_);
    }
    // A begin expression needs to return a value, so remove the last
    // Opcode::Pop, in order to keep the result on the operand stack.
    data_.pop_back();
}

void BytecodeBuilder::visit(ast::If& node)
{
    // Condition, then conditionally branch over the true block
    node.condition_->visit(*this);
    writeOp<Opcode::JumpIfFalse>(data_);
    const size_t jumpOffset1Loc = data_.size();
    writeParam(data_, (uint16_t)0);
    // True block, then unconditionally branch over the false block
    node.trueBranch_->visit(*this);
    writeOp<Opcode::Jump>(data_);
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
    writeOp<Opcode::Store>(data_);
    writeOp<Opcode::PushNull>(data_);
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
        writeOp<Opcode::ExitLet>(data_);
    }
    writeOp<Opcode::Recur>(data_);
}

void BytecodeBuilder::visit(ast::UserObject& node)
{
    writeOp<Opcode::PushI>(data_);
    writeParam(data_, node.varLoc_);
}

void BytecodeBuilder::unusedExpr()
{
    writeOp<Opcode::Discard>(data_);
}

} // namespace ebl
