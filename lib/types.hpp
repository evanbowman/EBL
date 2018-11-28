#pragma once

#include <array>
#include <complex>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <tuple>
#include <vector>

#include "extlib/smallVector.hpp"
#include "memory.hpp"
#include "utility.hpp"


namespace lisp {

class Environment;
using EnvPtr = std::shared_ptr<Environment>;

using TypeId = uint8_t;


class Object {
private:
    struct Header {
        const TypeId typeInfoIndex;
        uint8_t marked : 1;
    } header_;

public:
    inline Object(TypeId id) : header_{id, 0}
    {
    }
    inline TypeId typeId() const
    {
        return header_.typeInfoIndex;
    }
    inline void mark()
    {
        header_.marked = true;
    }
    inline void unmark()
    {
        header_.marked = false;
    }
    inline bool marked()
    {
        return header_.marked;
    }
};


using ObjectPtr = Heap::Ptr<Object>;


struct EqualTo {
    bool operator()(ObjectPtr lhs, ObjectPtr rhs) const;
};


struct Hash {
    size_t operator()(ObjectPtr obj) const noexcept;
};


struct TypeError : std::runtime_error {
    TypeError(TypeId t, const std::string& reason);
};


struct ConversionError : TypeError {
    ConversionError(TypeId from, TypeId to);
};


class Null : public Object {
public:
    inline Null(TypeId tp) : Object{tp}
    {
    }
    static constexpr const char* name()
    {
        return "<Null>";
    }
};


class Pair : public Object {
public:
    inline Pair(TypeId tp, ObjectPtr car, ObjectPtr cdr)
        : Object{tp}, car_(car), cdr_(cdr)
    {
    }

    static constexpr const char* name()
    {
        return "<Pair>";
    }

    inline ObjectPtr getCar() const
    {
        return car_;
    }
    inline ObjectPtr getCdr() const
    {
        return cdr_;
    }

    inline void setCar(ObjectPtr value)
    {
        car_ = value;
    }
    inline void setCdr(ObjectPtr value)
    {
        cdr_ = value;
    }

private:
    ObjectPtr car_;
    ObjectPtr cdr_;
};


class Boolean : public Object {
public:
    inline Boolean(TypeId tp, bool value) : Object{tp}, value_(value)
    {
    }

    static constexpr const char* name()
    {
        return "<Boolean>";
    }

    inline operator bool() const
    {
        return value_;
    }

    inline bool value() const
    {
        return value_;
    }

private:
    bool value_;
};


class Integer : public Object {
public:
    using Rep = int32_t;
    using Input = Rep;

    inline Integer(TypeId tp, Input value) : Object{tp}, value_(value)
    {
    }

    static constexpr const char* name()
    {
        return "<Integer>";
    }

    inline Rep value() const
    {
        return value_;
    }

private:
    Rep value_;
};


class Double : public Object {
public:
    using Rep = double;
    using Input = Rep;

    inline Double(TypeId tp, Input value) : Object{tp}, value_(value)
    {
    }

    static constexpr const char* name()
    {
        return "<Double>";
    }

    inline Rep value() const
    {
        return value_;
    }

private:
    Rep value_;
};


class Complex : public Object {
public:
    using Rep = std::complex<double>;
    using Input = Rep;

    inline Complex(TypeId tp, Input value) : Object{tp}, value_(value)
    {
    }

    static constexpr const char* name()
    {
        return "<Complex>";
    }

    inline Rep value() const
    {
        return value_;
    }

private:
    Rep value_;
};


class Character : public Object {
public:
    using Rep = std::array<char, 4>;
    using Input = Rep;

    inline Character(TypeId tp, const Input& value) : Object{tp}, value_(value)
    {
    }

    static constexpr const char* name()
    {
        return "<Character>";
    }

    inline const Rep& value() const
    {
        return value_;
    }

private:
    Rep value_;
};


class String : public Object {
public:
    using Input = std::string;

    using Rep = String;

    static constexpr const char* name()
    {
        return "<String>";
    }

    inline const String& value() const
    {
        return *this;
    }

    enum class Encoding { binary, utf8 };

    String(TypeId tp, const char* data, size_t length,
           Encoding enc = Encoding::utf8);
    String(TypeId tp, const Input& str, Encoding enc = Encoding::utf8);

    Heap::Ptr<Character> operator[](size_t index) const;

    size_t length() const;

    std::string toAscii() const;

    bool operator==(const Input& other) const;
    bool operator==(const String& other) const;

private:
    void initialize(const char* data, size_t len, Encoding enc);
    Heap storage_;
};


class Symbol : public Object {
public:
    inline Symbol(TypeId tp, Heap::Ptr<String> str) : Object{tp}, str_(str)
    {
    }

    static constexpr const char* name()
    {
        return "<Symbol>";
    }

    inline Heap::Ptr<String> value()
    {
        return str_;
    }

private:
    Heap::Ptr<String> str_;
};


class RawPointer : public Object {
public:
    inline RawPointer(TypeId tp, void* p) : Object{tp}, value_(p)
    {
    }

    static constexpr const char* name()
    {
        return "<RawPointer>";
    }

    void* value() const
    {
        return value_;
    }

private:
    void* value_;
};


using Arguments = Ogre::SmallVector<ObjectPtr, 3>;
using CFunction = std::function<ObjectPtr(Environment&, const Arguments&)>;
struct InvalidArgumentError : std::runtime_error {
    InvalidArgumentError(const char* msg) : std::runtime_error(msg)
    {
    }
};


class Function : public Object {
public:
    class Impl {
    public:
        virtual ~Impl()
        {
        }
        virtual ObjectPtr call(Environment& env, Arguments&) = 0;
    };

    Function(TypeId tp, Environment& env, ObjectPtr docstring,
             size_t requiredArgs, CFunction cFn);

    Function(TypeId tp, Environment& env, ObjectPtr docstring,
             size_t requiredArgs, std::unique_ptr<Impl> impl);

    static constexpr const char* name()
    {
        return "<Function>";
    }

    // TODO: call function should be defined differently?
    inline ObjectPtr call(Arguments& params)
    {
        if (params.size() < requiredArgs_) {
            throw InvalidArgumentError("too few args");
        }
        return impl_->call(*envPtr_, params);
    }

    inline ObjectPtr getDocstring()
    {
        return docstring_;
    }

    inline size_t argCount()
    {
        return requiredArgs_;
    }

    inline EnvPtr definitionEnvironment()
    {
        return envPtr_;
    }

private:
    ObjectPtr docstring_;
    size_t requiredArgs_;
    std::unique_ptr<Impl> impl_;
    EnvPtr envPtr_;
};


struct TypeInfo {
    size_t size_;
    const char* name_;
};


template <typename T> constexpr TypeInfo makeInfo()
{
    return {sizeof(T), T::name()};
}


template <typename... Builtins> struct TypeInfoTable {
    template <typename T> constexpr const TypeInfo& get() const
    {
        return table[typeId<T>()];
    }
    const TypeInfo& operator[](TypeId index) const
    {
        return table[index];
    }
    template <typename T> constexpr TypeId typeId() const
    {
        static_assert(sizeof...(Builtins) < std::numeric_limits<TypeId>::max(),
                      "TypeInfoTable parameter count exhausts the range of "
                      "TypeId");
        return ::Index<T, Builtins...>::value;
    }
    constexpr TypeInfoTable() : table{makeInfo<Builtins>()...}
    {
    }
    TypeInfo table[sizeof...(Builtins)];
};


constexpr TypeInfoTable<Null, Pair, Boolean, Integer, Double, Complex, String,
                        Character, Symbol, RawPointer, Function>
    typeInfo;


template <typename T> bool isType(ObjectPtr obj)
{
    return typeInfo.typeId<T>() == obj->typeId();
}


template <typename T> Heap::Ptr<T> checkedCast(ObjectPtr obj)
{
    if (isType<T>(obj)) {
        return obj.cast<T>();
    }
    throw ConversionError{obj->typeId(), typeInfo.typeId<T>()};
}

std::ostream& operator<<(std::ostream& out, const String& str);
std::ostream& operator<<(std::ostream& out, const Character& c);

} // namespace lisp
