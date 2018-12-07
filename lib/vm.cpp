#include "vm.hpp"
#include "bytecode.hpp"
#include "environment.hpp"
#include <iostream>

namespace lisp {

template <typename T> T readParam(const Bytecode& bc, size_t& ip)
{
    const auto result = *reinterpret_cast<const T*>(bc.data() + ip);
    ip += sizeof(T);
    return result;
}

void VM::execute(Environment& environment, const Bytecode& bc, size_t start)
{
    auto env = environment.reference();
    Context* const context = env->getContext();
    size_t ip = start;
    while (true) {
        switch ((Opcode)bc[ip]) {
        case Opcode::Call: {
            ++ip;
            auto argc = readParam<uint8_t>(bc, ip);
            // std::cout << ip << ": CALL " << (size_t)argc << std::endl;
            auto target = context->operandStack().back();
            auto fn = checkedCast<Function>(target);
            if (auto addr = fn->getBytecodeAddress()) {
                if (UNLIKELY(argc not_eq fn->argCount())) {
                    throw std::runtime_error("wrong number of arguments");
                }
                context->operandStack().pop_back();
                env = fn->definitionEnvironment()->derive();
                context->callStack().push_back({ip, addr, env});
                ip = addr;
            } else {
                auto result = env->getNull();
                {
                    Arguments args(*env, argc);
                    context->operandStack().pop_back();
                    result = fn->directCall(args);
                }
                context->operandStack().push_back(result);
            }
        } break;

        case Opcode::Return: {
            auto retAddr = context->callStack().back().returnAddress_;
            // std::cout << ip << ": RETURN " << retAddr << std::endl;
            context->callStack().pop_back();
            env = context->callStack().back().env_;
            ip = retAddr;
        } break;

        case Opcode::EnterLet: {
            ++ip;
            // std::cout << ip << ": ENLET" << std::endl;
            env = env->derive();
        } break;

        case Opcode::ExitLet: {
            ++ip;
            // std::cout << ip << ": EXLET" << std::endl;
            env = env->parent();
        } break;

        case Opcode::Jump: {
            ++ip;
            const auto jumpOffset = readParam<uint16_t>(bc, ip);
            // std::cout << ip << ": JUMP " << jumpOffset << std::endl;
            ip += jumpOffset;
        } break;

        case Opcode::JumpIfFalse: {
            ++ip;
            const auto jumpOffset = readParam<uint16_t>(bc, ip);
            // std::cout << ip << ": JIF " << jumpOffset << std::endl;
            if (context->operandStack().back() == env->getBool(false)) {
                ip += jumpOffset;
            }
            context->operandStack().pop_back();
        } break;

        case Opcode::Load: {
            ++ip;
            VarLoc param;
            param.frameDist_ = readParam<FrameDist>(bc, ip);
            param.offset_ = readParam<StackLoc>(bc, ip);
            // std::cout << ip << ": LOAD " << param.frameDist_ << ", " <<
            // param.offset_ << std::endl;
            context->operandStack().push_back(env->load(param));
        } break;

        case Opcode::Load0: {
            ++ip;
            const auto offset = readParam<StackLoc>(bc, ip);
            context->operandStack().push_back(env->getVars()[offset]);
        } break;

        case Opcode::Load1: {
            ++ip;
            const auto offset = readParam<StackLoc>(bc, ip);
            context->operandStack().push_back(env->parent()->getVars()[offset]);
        } break;

        case Opcode::Load2: {
            ++ip;
            const auto offset = readParam<StackLoc>(bc, ip);
            context->operandStack().push_back(env->parent()->parent()->getVars()[offset]);
        } break;

        case Opcode::PushI: {
            ++ip;
            auto param = readParam<ImmediateId>(bc, ip);
            // std::cout << ip << ": PUSHI " << param << std::endl;
            context->operandStack().push_back(context->immediates()[param]);
        } break;

        case Opcode::Store: {
            ++ip;
            env->push(context->operandStack().back());
            // std::cout << ip << ": STORE" << std::endl;
            context->operandStack().pop_back();
        } break;

        case Opcode::Pop:
            ++ip;
            context->operandStack().pop_back();
            // std::cout << ip << ": POP" << std::endl;
            break;

        case Opcode::PushNull:
            ++ip;
            context->operandStack().push_back(env->getNull());
            // std::cout << ip << ": PNULL" << std::endl;
            break;

        case Opcode::PushTrue:
            ++ip;
            context->operandStack().push_back(env->getBool(true));
            // std::cout << ip << ": PTRUE" << std::endl;
            break;

        case Opcode::PushFalse:
            context->operandStack().push_back(env->getBool(false));
            ++ip;
            // std::cout << ip << ": PFALSE" << std::endl;
            break;

        case Opcode::PushLambda: {
            ++ip;
            auto argc = readParam<uint8_t>(bc, ip);
            // std::cout << ip << ": PLAMBDA " << (size_t)argc << std::endl;
            const size_t addr = ip + sizeof(Opcode::Jump) + sizeof(uint16_t);
            auto lambda =
                env->create<Function>(env->getNull(), (size_t)argc, addr);
            context->operandStack().push_back(lambda);
        } break;

        case Opcode::Recur: {
            ++ip;
            // std::cout << ip << ": RECUR " <<
            // context->callStack().back().functionTop_ << std::endl;
            env->getVars().clear();
            ip = context->callStack().back().functionTop_;
        } break;

        case Opcode::Exit: {
            return;
        }

        default:
            throw std::runtime_error("invalid opcode");
        }
    }
}


} // namespace lisp
