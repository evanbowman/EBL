#include "types.hpp"
#include "lisp.hpp"
#include "utility.hpp"

#include <map>
#include <memory>

#include <bitset>
#include <iostream>

namespace lisp {

bool EqualTo::operator()(ObjectPtr lhs, ObjectPtr rhs) const
{
    const auto type = lhs->typeId();
    if (type not_eq rhs->typeId()) {
        return false;
    }
#define LISP_EQ_CASE(T)                                                        \
    case typeInfo.typeId<T>():                                                 \
        return lhs.cast<T>()->value() == rhs.cast<T>()->value();
    switch (type) {
        LISP_EQ_CASE(Integer);
        LISP_EQ_CASE(Double);
        LISP_EQ_CASE(String);
        LISP_EQ_CASE(Boolean);
        LISP_EQ_CASE(Complex);
        LISP_EQ_CASE(Character);
    case typeInfo.typeId<Symbol>():
        return lhs == rhs;
    default:
        throw TypeError(type, "no equalto defined for input");
    }
    return true;
}

class CFunctionImpl : public Function::Impl {
public:
    CFunctionImpl(CFunction fn) : fn_(fn)
    {
    }

    ObjectPtr call(Environment& env, Arguments& params) override
    {
        return fn_(env, params);
    }

private:
    CFunction fn_;
};

Function::Function(Environment& env, ObjectPtr docstring, size_t requiredArgs,
                   CFunction impl)
    : docstring_(docstring), requiredArgs_(requiredArgs),
      impl_(std::unique_ptr<CFunctionImpl>(new CFunctionImpl(impl))),
      envPtr_(env.reference())
{
}

Function::Function(Environment& env, ObjectPtr docstring, size_t requiredArgs,
                   std::unique_ptr<Impl> impl)
    : docstring_(docstring), requiredArgs_(requiredArgs),
      impl_(std::move(impl)), envPtr_(env.reference())
{
}

TypeError::TypeError(TypeId t, const std::string& reason)
    : std::runtime_error(std::string("for type ") + typeInfo[t].name_ + ": " +
                         reason)
{
}

ConversionError::ConversionError(TypeId from, TypeId to)
    : TypeError(from, std::string("invalid cast to ") + typeInfo[to].name_)
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
            auto charMem = storage_.alloc<sizeof(Character)>();
            new (charMem.cast<Character>().get()) Character({data[i], 0, 0, 0});
        }
    } break;

    case Encoding::utf8: {
        storage_.init(utf8Len(data, len) * sizeof(Character));
        foreachUtf8Glyph(
            [&](const Character::Rep& val) {
                auto charMem = storage_.alloc<sizeof(Character)>();
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

Arguments::~Arguments()
{
    auto& opStack = ctx_->operandStack();
    for (size_t i = 0; i < count_; ++i) {
        opStack.pop_back();
    }
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

} // namespace lisp
