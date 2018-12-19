#include "vm.hpp"
#include "bytecode.hpp"
#include "environment.hpp"

namespace lisp {

template <typename T> T readParam(const Bytecode& bc, size_t& ip)
{
    throw std::runtime_error("encountered unsupported param size");
}

template <> uint8_t readParam(const Bytecode& bc, size_t& ip)
{
    const auto result = bc[ip];
    ip += 1;
    return result;
}

template <> uint16_t readParam(const Bytecode& bc, size_t& ip)
{
    const auto result = ((uint16_t)(bc[ip])) | (((uint16_t)(bc[ip + 1])) << 8);
    ip += 2;
    return result;
}

#if defined(_WIN32) or defined(_WIN64)
#define NO_DIRECT_THREADING
#endif

void VM::execute(Environment& environment, const Bytecode& bc, size_t start)
{
    auto env = environment.reference();
    Context* const context = env->getContext();
    auto& operandStack = context->operandStack();
    auto& callStack = context->callStack();
    size_t ip = start;
#ifndef NO_DIRECT_THREADING
    static const std::array<void*, (uint8_t)Opcode::Count> labels = {
        &&Exit,
        &&Call,
        &&Return,
        &&Recur,
        &&Jump,
        &&JumpIfFalse,
        &&Load,
        &&Load0,
        &&Load1,
        &&Load2,
        &&Load0Fast,
        &&Load1Fast,
        &&Store,
        &&Rebind,
        &&PushI,
        &&PushNull,
        &&PushTrue,
        &&PushFalse,
        &&PushLambda,
        &&PushDocumentedLambda,
        &&Discard,
        &&EnterLet,
        &&ExitLet,
        &&Cons,
        &&Car,
        &&Cdr,
        &&IsNull
    };
#define VM_DISPATCH_BEGIN() goto *labels[bc[ip]];
#define VM_DISPATCH_END() ;
#define VM_BLOCK_BEGIN(IDENTIFIER) IDENTIFIER:
#define VM_BLOCK_END() VM_DISPATCH_BEGIN();
#else // No direct threading, use switch case instead.
#define VM_DISPATCH_BEGIN() while (true) { switch ((Opcode)bc[ip]) {
#define VM_DISPATCH_END() }}
#define VM_BLOCK_BEGIN(IDENTIFIER) case Opcode::IDENTIFIER: {
#define VM_BLOCK_END() } break;
#endif

    VM_DISPATCH_BEGIN();

    VM_BLOCK_BEGIN(Cons) {
        ++ip;
        static_assert(std::is_same<decltype(operandStack),
                      std::vector<ObjectPtr>&>::value,
                      "operandStack indexing makes an assumption based "
                      "on random access iterator here.");
        auto cell = env->create<Pair>(operandStack.end()[-2],
                                      operandStack.end()[-1]);
        operandStack.pop_back();
        operandStack.pop_back();
        operandStack.push_back(cell);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Car) {
        ++ip;
        auto result = checkedCast<Pair>(operandStack.back())->getCar();
        operandStack.pop_back();
        operandStack.push_back(result);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Cdr) {
        ++ip;
        auto result = checkedCast<Pair>(operandStack.back())->getCdr();
        operandStack.pop_back();
        operandStack.push_back(result);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(IsNull) {
        ++ip;
        auto result = env->getBool(isType<Null>(operandStack.back()));
        operandStack.pop_back();
        operandStack.push_back(result);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Call) {
        ++ip;
        auto argc = readParam<uint8_t>(bc, ip);
        auto target = operandStack.back();
        auto fn = checkedCast<Function>(target);
        if (auto addr = fn->getBytecodeAddress()) {
            if (UNLIKELY(argc not_eq fn->argCount())) {
                if (argc < fn->argCount()) {
                    throw std::runtime_error("too few arguments");
                } else if (argc > fn->argCount()) {
                    throw std::runtime_error("too many arguments");
                }
            }
            operandStack.pop_back();
            env = fn->definitionEnvironment()->derive();
            callStack.push_back({ip, addr, env});
            ip = addr;
        } else {
            auto result = env->getNull();
            {
                Arguments args(*env, argc);
                operandStack.pop_back();
                result = fn->directCall(args);
            }
            operandStack.push_back(result);
        }
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Recur) {
        ++ip;
        env->getVars().clear();
        ip = callStack.back().functionTop_;
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Return) {
        auto retAddr = callStack.back().returnAddress_;
        callStack.pop_back();
        env = callStack.back().env_;
        ip = retAddr;
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(EnterLet) {
        ++ip;
        env = env->derive();
        callStack.push_back({0, 0, env});
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(ExitLet) {
        ++ip;
        callStack.pop_back();
        env = env->parent();
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Jump) {
        ++ip;
        const auto jumpOffset = readParam<uint16_t>(bc, ip);
        ip += jumpOffset;
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(JumpIfFalse) {
        ++ip;
        const auto jumpOffset = readParam<uint16_t>(bc, ip);
        if (operandStack.back() == env->getBool(false)) {
            ip += jumpOffset;
        }
        operandStack.pop_back();
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Load0Fast) {
        ++ip;
        const auto offset = readParam<uint8_t>(bc, ip);
        operandStack.push_back(env->getVars()[offset]);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Load1Fast) {
        ++ip;
        const auto offset = readParam<uint8_t>(bc, ip);
        operandStack.push_back(env->parent()->getVars()[offset]);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Load1) {
        ++ip;
        const auto offset = readParam<StackLoc>(bc, ip);
        operandStack.push_back(env->parent()->getVars()[offset]);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Load2) {
        ++ip;
        const auto offset = readParam<StackLoc>(bc, ip);
        operandStack.push_back(env->parent()->parent()->getVars()[offset]);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(PushI) {
        ++ip;
        auto param = readParam<ImmediateId>(bc, ip);
        operandStack.push_back(context->immediates()[param]);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Store) {
        ++ip;
        env->push(operandStack.back());
        operandStack.pop_back();
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Discard) {
        ++ip;
        operandStack.pop_back();
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(PushNull) {
        ++ip;
        operandStack.push_back(env->getNull());
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(PushTrue) {
        ++ip;
        operandStack.push_back(env->getBool(true));
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(PushFalse) {
        operandStack.push_back(env->getBool(false));
        ++ip;
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(PushLambda) {
        ++ip;
        auto argc = readParam<uint8_t>(bc, ip);
        const size_t addr = ip + sizeof(Opcode::Jump) + sizeof(uint16_t);
        auto lambda =
            env->create<Function>(env->getNull(), (size_t)argc, addr);
        operandStack.push_back(lambda);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(PushDocumentedLambda) {
        ++ip;
        auto argc = readParam<uint8_t>(bc, ip);
        auto docLoc = readParam<uint16_t>(bc, ip);
        const size_t addr = ip + sizeof(Opcode::Jump) + sizeof(uint16_t);
        auto lambda = env->create<Function>(context->immediates()[docLoc],
                                            (size_t)argc, addr);
        operandStack.push_back(lambda);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Load0) {
        ++ip;
        const auto offset = readParam<StackLoc>(bc, ip);
        operandStack.push_back(env->getVars()[offset]);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Load) {
        ++ip;
        VarLoc param;
        param.frameDist_ = readParam<FrameDist>(bc, ip);
        param.offset_ = readParam<StackLoc>(bc, ip);
        operandStack.push_back(env->load(param));
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Rebind) {
        ++ip;
        VarLoc param;
        param.frameDist_ = readParam<FrameDist>(bc, ip);
        param.offset_ = readParam<StackLoc>(bc, ip);
        auto value = operandStack.back();
        operandStack.pop_back();
        env->store(param, value);
    } VM_BLOCK_END();


    VM_BLOCK_BEGIN(Exit) {
        return;
    } VM_BLOCK_END();


    VM_DISPATCH_END();
}


} // namespace lisp
