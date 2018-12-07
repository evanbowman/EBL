#include "vm.hpp"
#include "environment.hpp"
#include "bytecode.hpp"
#include <iostream>

namespace lisp {

template <typename T>
T readParam(const Bytecode& bc, size_t& ip)
{
    const auto result = *reinterpret_cast<const T*>(bc.data() + ip);
    ip += sizeof(T);
    return result;
}

void VM::execute(Environment& environment, const Bytecode& bc, size_t start)
{
    auto env = environment.reference();
    Context* const context = env->getContext();
    context->callStack().push_back({0, env});
    size_t ip = start;
    while (true) {
        switch ((Opcode)bc[ip]) {
        case Opcode::Call: {
            ++ip;
            auto argc = readParam<uint8_t>(bc, ip);
            auto target = context->operandStack().back();
            auto fn = checkedCast<Function>(target);
            if (auto addr = fn->getBytecodeAddress()) {
                if (argc not_eq fn->argCount()) {
                    throw std::runtime_error("wrong number of arguments");
                }
                context->operandStack().pop_back();
                env = fn->definitionEnvironment()->derive();
                context->callStack().push_back({ip, env});
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
            auto retAddr = context->callStack().back().first;
            context->callStack().pop_back();
            env = context->callStack().back().second;
            ip = retAddr;
        } break;

        case Opcode::EnterLet: {
            ++ip;
            env = env->derive();
            context->callStack().push_back({ip, env});
        } break;

        case Opcode::ExitLet: {
            ++ip;
            context->callStack().pop_back();
            env = context->callStack().back().second;
        } break;

        case Opcode::Jump: {
            ++ip;
            const auto jumpOffset = readParam<uint16_t>(bc, ip);
            ip += jumpOffset;
        } break;

        case Opcode::JumpIfFalse: {
            ++ip;
            const auto jumpOffset = readParam<uint16_t>(bc, ip);
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
            context->operandStack().push_back(env->load(param));
        } break;

        case Opcode::PushI: {
            ++ip;
            auto param = readParam<ImmediateId>(bc, ip);
            context->operandStack().push_back(context->immediates()[param]);
        } break;

        case Opcode::Store: {
            env->push(context->operandStack().back());
            context->operandStack().pop_back();
            ++ip;
        } break;

        case Opcode::Pop:
            context->operandStack().pop_back();
            ++ip;
            break;

        case Opcode::PushNull:
            context->operandStack().push_back(env->getNull());
            ++ip;
            break;

        case Opcode::PushTrue:
            context->operandStack().push_back(env->getBool(true));
            ++ip;
            break;

        case Opcode::PushFalse:
            context->operandStack().push_back(env->getBool(false));
            ++ip;
            break;

        case Opcode::PushLambda: {
            ++ip;
            auto argc = readParam<uint8_t>(bc, ip);
            const size_t addr = ip + sizeof(Opcode::Jump) + sizeof(uint16_t);
            auto lambda = env->create<Function>(env->getNull(), (size_t)argc, addr);
            context->operandStack().push_back(lambda);
            } break;

        case Opcode::Exit: {
            return;
        }

        default:
            throw std::runtime_error("invalid opcode");
        }
    }
}


}
