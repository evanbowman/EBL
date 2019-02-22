#include "types.hpp"
#include "bytecode.hpp"
#include "ebl.hpp"
#include "utility.hpp"
#include "vm.hpp"
#include <map>
#include <memory>
#include <bitset>

namespace ebl {

bool EqualTo::operator()(ObjectPtr lhs, ObjectPtr rhs) const
{
    const auto type = lhs->typeId();
    if (type not_eq rhs->typeId()) {
        return false;
    }
#define EBL_EQ_CASE(T)                                                        \
    case typeId<T>():                                                          \
        return lhs.cast<T>()->value() == rhs.cast<T>()->value();
    switch (type) {
        EBL_EQ_CASE(Integer);
        EBL_EQ_CASE(Float);
        EBL_EQ_CASE(String);
        EBL_EQ_CASE(Boolean);
        EBL_EQ_CASE(Complex);
        EBL_EQ_CASE(Character);
    case typeId<Symbol>():
        return lhs == rhs;
    default:
        throw TypeError(type, "no equalto defined for input");
    }
    return true;
}

ObjectPtr Function::call(Arguments& params)
{
    switch (model_) {
    case InvocationModel::Bytecode: {
        if (UNLIKELY(params.count() != requiredArgs_)) {
            failedToApply(*envPtr_, this, params.count(), requiredArgs_);
        }
        Context* const ctx = envPtr_->getContext();
        auto derived = envPtr_->derive();
        ctx->callStack().push_back(
            {ctx->getProgram().size() - 1, bytecodeAddress_, derived});
        VM::execute(*derived, ctx->getProgram(), bytecodeAddress_);
        auto ret = ctx->operandStack().back();
        // The bytecode function would have taken the args off of the
        // operand stack, so we need to clear out the argument
        // vector's count.
        ctx->operandStack().pop_back();
        params.consumed();
        return ret;
    } break;

    case InvocationModel::BytecodeVariadic:
        throw std::runtime_error("TODO: support VA calls from native code");

    case InvocationModel::Wrapped: {
        if (UNLIKELY(params.count() < requiredArgs_)) {
            throw InvalidArgumentError("too few args, expected " +
                                       std::to_string(requiredArgs_) + " got " +
                                       std::to_string(params.count()));
        }
        return (*nativeFn_)(*envPtr_, params);
    } break;
    }
}

Function::Function(Environment& env, ObjectPtr docstring, size_t requiredArgs,
                   CFunction impl)
    : model_(InvocationModel::Wrapped), docstring_(docstring),
      requiredArgs_(requiredArgs), nativeFn_(impl), bytecodeAddress_(0),
      envPtr_(env.reference())
{
}

Function::Function(Environment& env, ObjectPtr docstring, size_t requiredArgs,
                   size_t bytecodeAddress)
    : model_(InvocationModel::Bytecode), docstring_(docstring),
      requiredArgs_(requiredArgs), nativeFn_(),
      bytecodeAddress_(bytecodeAddress), envPtr_(env.reference())
{
}

Function::Function(Environment& env, ObjectPtr docstring, size_t requiredArgs,
                   size_t bytecodeAddress, bool variadic)
    : model_([variadic] {
          if (variadic) {
              return InvocationModel::BytecodeVariadic;
          } else {
              return InvocationModel::Bytecode;
          }
      }()),
      docstring_(docstring), requiredArgs_(requiredArgs), nativeFn_(),
      bytecodeAddress_(bytecodeAddress), envPtr_(env.reference())
{
}


TypeError::TypeError(TypeId t, const std::string& reason)
    : std::runtime_error(std::string("for type ") + typeInfoTable[t].name_ +
                         ": " + reason)
{
}

ConversionError::ConversionError(TypeId from, TypeId to)
    : TypeError(from, std::string("invalid cast to ") + typeInfoTable[to].name_)
{
}

String::String(const char* data, size_t length, Encoding enc)
{
    initialize(data, length, enc);
}

String::String(const Input& str, Encoding enc)
{
    initialize(str.c_str(), str.length(), enc);
}

void String::initialize(const char* data, size_t len, Encoding enc)
{
    switch (enc) {
    case Encoding::binary: {
        storage_.init(len * sizeof(Character));
        for (size_t i = 0; i < len; ++i) {
            auto charMem = storage_.alloc<Character>();
            new (charMem.cast<Character>().get()) Character({data[i], 0, 0, 0});
        }
    } break;

    case Encoding::utf8: {
        storage_.init(utf8Len(data, len) * sizeof(Character));
        foreachUtf8Glyph(
            [&](const Character::Rep& val) {
                auto charMem = storage_.alloc<Character>();
                new (charMem.cast<Character>().get()) Character(val);
            },
            data, len);
    } break;
    }
}

bool String::operator==(const Input& other) const
{
    auto glyphs = reinterpret_cast<Character*>(storage_.begin());
    size_t index = 0;
    bool equal = true;
    foreachUtf8Glyph(
        [&](const Character::Rep& val) {
            if (glyphs[index].value() != val) {
                equal = false;
            }
            index += 1;
        },
        other.c_str(), other.length());
    return equal;
}

bool String::operator==(const String& other) const
{
    const auto lhsLen = length();
    const auto rhsLen = other.length();
    if (lhsLen != rhsLen) {
        return false;
    }
    auto glyphs = reinterpret_cast<Character*>(storage_.begin());
    auto otherGlyphs = reinterpret_cast<Character*>(other.storage_.begin());
    for (size_t i = 0; i < lhsLen and i < rhsLen; ++i) {
        if (glyphs[i].value() not_eq otherGlyphs[i].value()) {
            return false;
        }
    }
    return true;
}

std::string String::toAscii() const
{
    std::string ret;
    const auto len = length();
    auto glyphs = reinterpret_cast<Character*>(storage_.begin());
    for (size_t i = 0; i < len; ++i) {
        if (glyphs[i].value()[1] == 0) {
            ret.push_back(glyphs[i].value()[0]);
        } else {
            throw std::runtime_error("failed to convert String to ascii");
        }
    }
    return ret;
}

size_t String::length() const
{
    return storage_.capacity() / sizeof(Character);
}

Heap::Ptr<Character> String::operator[](size_t index) const
{
    if (index < length()) {
        return storage_.arrayElemAt<Character>(index);
    }
    throw std::runtime_error("invalid index to String");
}

std::ostream& operator<<(std::ostream& out, const String& str)
{
    const auto len = str.length();
    for (size_t i = 0; i < len; ++i) {
        out << *str[i];
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const Character& c)
{
    for (char cp : c.value()) {
        if (cp == 0) {
            break;
        }
        out << cp;
    }
    return out;
}

Arguments::Arguments(Environment& env)
    : ctx_(env.getContext()),
      startIdx_(env.getContext()->operandStack().size()), count_(0)
{
}

Arguments::Arguments(Environment& env, size_t count)
    : ctx_(env.getContext()),
      startIdx_(env.getContext()->operandStack().size() - (count + 1)),
      count_(count)
{
}


Arguments::~Arguments()
{
    auto& opStack = ctx_->operandStack();
    for (size_t i = 0; i < count_; ++i) {
        opStack.pop_back();
    }
}

void Arguments::consumed()
{
    count_ = 0;
}

void Arguments::push(ObjectPtr arg)
{
    ctx_->operandStack().push_back(arg);
    ++count_;
}

std::vector<ObjectPtr>::iterator Arguments::begin() const
{
    return ctx_->operandStack().begin() + startIdx_;
}

std::vector<ObjectPtr>::iterator Arguments::end() const
{
    return begin() + count_;
}

ObjectPtr Arguments::operator[](size_t index) const
{
    return ctx_->operandStack()[startIdx_ + index];
}

size_t Arguments::count() const
{
    return count_;
}

Heap::Ptr<Null> Null::clone(Environment& env) const
{
    return env.getNull().cast<Null>();
}

Heap::Ptr<Pair> Pair::clone(Environment& env) const
{
    // Be careful when implementing clone for pair, the gc could move
    // stuff around during allocation, leaving the parameters invalid.
    throw std::runtime_error("Deep clone unimplemented for Pair");
}

Heap::Ptr<Box> Box::clone(Environment& env) const
{
    throw std::runtime_error("Deep clone unimplemented for Box");
}

Heap::Ptr<Boolean> Boolean::clone(Environment& env) const
{
    return env.getBool(value_).cast<Boolean>();
}

Heap::Ptr<Integer> Integer::clone(Environment& env) const
{
    return env.create<Integer>(value_);
}

Heap::Ptr<Float> Float::clone(Environment& env) const
{
    return env.create<Float>(value_);
}

Heap::Ptr<Complex> Complex::clone(Environment& env) const
{
    return env.create<Complex>(value_);
}

Heap::Ptr<Character> Character::clone(Environment& env) const
{
    return env.create<Character>(value_);
}

Heap::Ptr<String> String::clone(Environment& env) const
{
    throw std::runtime_error("Deep clone unimplemented for String");
}

Heap::Ptr<Symbol> Symbol::clone(Environment& env) const
{
    throw std::runtime_error("Deep clone unimplemented for Symbol");
}

Heap::Ptr<RawPointer> RawPointer::clone(Environment& env) const
{
    throw std::runtime_error("Deep clone unimplemented for RawPointer");
}

Heap::Ptr<Function> Function::clone(Environment& env) const
{
    throw std::runtime_error("Deep clone unimplemented for Function");
}

} // namespace ebl
