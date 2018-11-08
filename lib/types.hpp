#pragma once

#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <tuple>
#include <vector>

#include "memory.hpp"
#include "utility.hpp"

namespace lisp {

class Environment;
using EnvPtr = std::shared_ptr<Environment>;

using TypeId = uint8_t;

class Object {
public:
    inline Object(TypeId id) : header_{id, Header::Flags::None}
    {
    }
    inline TypeId typeId() const
    {
        return header_.typeInfoIndex;
    }

private:
    struct Header {
        const TypeId typeInfoIndex;
        enum Flags : uint8_t { None = 0, Marked = 1 << 0 } gcFlags_;
    } header_;
};

using ObjectPtr = Heap::Ptr<Object>;

struct EqualTo {
    bool operator()(ObjectPtr lhs, ObjectPtr rhs) const;
};

struct Hash {
    size_t operator()(ObjectPtr obj) const noexcept;
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

private:
    bool value_;
};

class Integer : public Object {
public:
    using Rep = int32_t;

    inline Integer(TypeId tp, Rep value) : Object{tp}, value_(value)
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

class String : public Object {
public:
    inline String(TypeId tp, const char* data, size_t length)
        : Object{tp}, data_(data, length)
    {
    }

    static constexpr const char* name()
    {
        return "<String>";
    }

    inline const std::string& value() const
    {
        return data_;
    }

private:
    std::string data_;
};

class Arguments {
public:
    using Count = int64_t;

    inline Count count() const
    {
        return argc;
    }
    inline ObjectPtr operator[](size_t index) const
    {
        return args[index];
    }

    Count argc;
    ObjectPtr* args;
};

using CFunction = std::function<ObjectPtr(Environment&, const Arguments&)>;

class Function : public Object {
public:
    // TODO: a variant + enum might be faster, time it and see.
    class Impl {
    public:
        virtual ~Impl()
        {
        }
        virtual ObjectPtr call(Environment& env, std::vector<ObjectPtr>&) = 0;
    };

    Function(TypeId tp, Environment& env, const char* docstring,
             Arguments::Count requiredArgs, CFunction cFn);

    Function(TypeId tp, Environment& env, const char* docstring,
             Arguments::Count requiredArgs, std::unique_ptr<Impl> impl);

    static constexpr const char* name()
    {
        return "<Function>";
    }

    // TODO: call function should be defined differently?
    inline ObjectPtr call(std::vector<ObjectPtr>& params)
    {
        if ((int)params.size() < requiredArgs_) {
            throw std::runtime_error("not enough params for fn!");
        }
        return impl_->call(*envPtr_, params);
    }

    void memoize();
    void unmemoize();
    bool isMemoized() const;

    inline const char* getDocstring()
    {
        return docstring_;
    }
    inline Arguments::Count argCount()
    {
        return requiredArgs_;
    }

private:
    const char* docstring_;
    Arguments::Count requiredArgs_;
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
    TypeInfo table[sizeof...(Builtins)] = {makeInfo<Builtins>()...};
};

constexpr TypeInfoTable<Null, Pair, Boolean, Integer, String, Function>
    typeInfo{};

template <typename T> bool isType(Environment& env, ObjectPtr obj)
{
    return typeInfo.typeId<T>() == obj->typeId();
}

struct ConversionError : public std::runtime_error {
    ConversionError(TypeId from, TypeId to)
        : std::runtime_error(std::string("bad cast from ") +
                             typeInfo[from].name_ + " to " + typeInfo[to].name_)
    {
    }
};

template <typename T> Heap::Ptr<T> checkedCast(Environment& env, ObjectPtr obj)
{
    if (isType<T>(env, obj)) {
        return obj.cast<T>();
    }
    throw ConversionError{obj->typeId(), typeInfo.typeId<T>()};
}

} // namespace lisp