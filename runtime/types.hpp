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

// FIXME!!!
#include "../extlib/smallVector.hpp"

#include "macros.hpp"
#include "memory.hpp"
#include "utility.hpp"

namespace ebl {

class Environment;
class Context;
using EnvPtr = std::shared_ptr<Environment>;

using TypeId = uint8_t;


class Object {
private:
    struct Header {
        const TypeId typeInfoIndex;
        uint8_t marked : 1;
        uint8_t reserved : 7;
    } header_;

public:
    inline Object(TypeId id) : header_{id, 0, 0}
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
    inline bool marked() const
    {
        return header_.marked;
    }
};


using ObjectPtr = Heap::Ptr<Object>;


template <typename T> class ObjectTemplate : public Object {
public:
    ObjectTemplate();

    static void finalize(Object* obj)
    {
        reinterpret_cast<T*>(obj)->~T();
    }

    static void relocate(Object* obj, uint8_t* dest)
    {
        // NOTE: due to potentially overlapping memory, we need to first move
        // the object to a temporary buffer, destruct the original, then move
        // the buffer contents to the final location, and then destruct the
        // object in the buffer.
        alignas(T) std::array<uint8_t, sizeof(T)> buffer;
        new ((T*)buffer.data()) T(std::move(*reinterpret_cast<T*>(obj)));
        ((T*)obj)->~T();
        new ((T*)dest) T(std::move(*reinterpret_cast<T*>(buffer.data())));
        ((T*)buffer.data())->~T();
    }

    static ObjectPtr cloneInterface(Environment& env, ObjectPtr obj)
    {
        return obj.cast<T>()->clone(env);
    }
};


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


// NOTE: All Objects are allocated from single contiguous and
// compacted heap, and therefore need to share the same alignment
// requirement. There're static asserts for this elsewhere.
class alignas(8) Null : public ObjectTemplate<Null> {
public:
    static constexpr const char* name()
    {
        return "<Null>";
    }

    Heap::Ptr<Null> clone(Environment& env) const;
};


class Pair : public ObjectTemplate<Pair> {
public:
    inline Pair(ObjectPtr car, ObjectPtr cdr) : car_(car), cdr_(cdr)
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

    Heap::Ptr<Pair> clone(Environment& env) const;

private:
    ObjectPtr car_;
    ObjectPtr cdr_;
};


class alignas(8) Box : public ObjectTemplate<Box> {
 public:
    inline Box(ObjectPtr value) : value_(value)
    {

    }

    static constexpr const char* name()
    {
        return "<Box>";
    }

    void set(ObjectPtr value)
    {
        value_ = value;
    }

    ObjectPtr get() const
    {
        return value_;
    }

    Heap::Ptr<Box> clone(Environment& env) const;

 private:
    ObjectPtr value_;
};


class alignas(8) Boolean : public ObjectTemplate<Boolean> {
public:
    inline Boolean(bool value) : value_(value)
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

    Heap::Ptr<Boolean> clone(Environment& env) const;

private:
    bool value_;
};


class alignas(8) Integer : public ObjectTemplate<Integer> {
public:
    using Rep = int32_t;
    using Input = Rep;

    inline Integer(Input value) : value_(value)
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

    Heap::Ptr<Integer> clone(Environment& env) const;

private:
    Rep value_;
};


class Float : public ObjectTemplate<Float> {
public:
    using Rep = double;
    using Input = Rep;

    inline Float(Input value) : value_(value)
    {
    }

    static constexpr const char* name()
    {
        return "<Float>";
    }

    inline Rep value() const
    {
        return value_;
    }

    Heap::Ptr<Float> clone(Environment& env) const;

private:
    Rep value_;
};


class Complex : public ObjectTemplate<Complex> {
public:
    using Rep = std::complex<double>;
    using Input = Rep;

    inline Complex(Input value) : value_(value)
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

    Heap::Ptr<Complex> clone(Environment& env) const;

private:
    Rep value_;
};


class alignas(8) Character : public ObjectTemplate<Character> {
public:
    using Rep = std::array<char, 4>;
    using Input = Rep;

    inline Character(const Input& value) : value_(value)
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

    Heap::Ptr<Character> clone(Environment& env) const;

private:
    Rep value_;
};


class String : public ObjectTemplate<String> {
public:
    using Input = std::string;

    static constexpr const char* name()
    {
        return "<String>";
    }

    inline const String& value() const
    {
        return *this;
    }

    enum class Encoding { binary, utf8 };

    String(const char* data, size_t length, Encoding enc = Encoding::utf8);
    String(const Input& str, Encoding enc = Encoding::utf8);

    Heap::Ptr<Character> operator[](size_t index) const;

    size_t length() const;

    std::string toAscii() const;

    bool operator==(const Input& other) const;
    bool operator==(const String& other) const;

    Heap::Ptr<String> clone(Environment& env) const;

private:
    void initialize(const char* data, size_t len, Encoding enc);
    Heap storage_;
};


class Symbol : public ObjectTemplate<Symbol> {
public:
    using Input = Heap::Ptr<String>;

    inline Symbol(Input str) : str_(str)
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

    inline void set(Heap::Ptr<String> val)
    {
        str_ = val;
    }

    Heap::Ptr<Symbol> clone(Environment& env) const;

private:
    Heap::Ptr<String> str_;
};


class RawPointer : public ObjectTemplate<RawPointer> {
public:
    inline RawPointer(void* p) : value_(p)
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

    Heap::Ptr<RawPointer> clone(Environment& env) const;

private:
    void* value_;
};


// IMPORTANT: You should not associate multiple Arguments with the same
// environment at the same time, and doing so is undefined behavior. In terms of
// implementation, Arguments is an adaptor that places the inputs onto the
// runtime operand stack, and if you push to two different Arguments objects,
// your inputs may interleave.
class Arguments {
public:
    Arguments(Environment& env);
    Arguments(const Arguments&) = delete;
    Arguments(Environment& env, size_t count);
    ~Arguments();

    size_t count() const;

    void push(ObjectPtr arg);

    ObjectPtr operator[](size_t index) const;

    void consumed();

    std::vector<ObjectPtr>::iterator begin() const;
    std::vector<ObjectPtr>::iterator end() const;

private:
    Context* ctx_;
    size_t startIdx_;
    size_t count_;
};


typedef ObjectPtr (*CFunction)(Environment&, const Arguments&);

struct InvalidArgumentError : std::runtime_error {
    InvalidArgumentError(const std::string& msg) : std::runtime_error(msg)
    {
    }
};

class Function;

void failedToApply(Environment& env,
                   Function* function,
                   size_t suppliedArgs,
                   size_t expectedArgs);

class Function : public ObjectTemplate<Function> {
public:
    Function(Environment& env, ObjectPtr docstring, size_t requiredArgs,
             CFunction cFn);

    Function(Environment& env, ObjectPtr docstring, size_t requiredArgs,
             size_t bytecodeAddress);

    Function(Environment& env, ObjectPtr docstring, size_t requiredArgs,
             size_t bytecodeAddress, bool variadic);

    static constexpr const char* name()
    {
        return "<Function>";
    }

    ObjectPtr call(Arguments& params);

    // When a VM is executing functions, it will try to extract a function's
    // bytecode address and avoid entering a new execution environment. If the
    // function is defined in native code, the vm will instead call
    // directCall. If you're a user of the library, and are trying to simply
    // call a function, the regular call() method is probably what you're
    // looking for.
    inline ObjectPtr directCall(Arguments& params)
    {
        if (UNLIKELY(params.count() < requiredArgs_)) {
            failedToApply(*envPtr_, this, params.count(), requiredArgs_);
        }
        return (*nativeFn_)(*envPtr_, params);
    }

    inline size_t getBytecodeAddress() const
    {
        return bytecodeAddress_;
    }

    inline ObjectPtr getDocstring()
    {
        return docstring_;
    }

    inline void setDocstring(ObjectPtr val)
    {
        docstring_ = val;
    }

    inline size_t argCount()
    {
        return requiredArgs_;
    }

    inline EnvPtr definitionEnvironment()
    {
        return envPtr_;
    }

    Heap::Ptr<Function> clone(Environment& env) const;

    enum InvocationModel { Wrapped, Bytecode, BytecodeVariadic };

    InvocationModel getInvocationModel() const
    {
        return model_;
    }

private:
    InvocationModel model_;
    ObjectPtr docstring_;
    size_t requiredArgs_;
    CFunction nativeFn_;
    size_t bytecodeAddress_;
    EnvPtr envPtr_;
};


struct TypeInfo {
    size_t size_;
    const char* name_;
    void (*finalizer)(Object*);
    void (*relocatePolicy)(Object*, uint8_t*);
    ObjectPtr (*clonePolicy)(Environment&, ObjectPtr);
};


template <typename T> constexpr TypeInfo makeInfo()
{
    return TypeInfo{sizeof(T), T::name(), T::finalize, T::relocate,
                    T::cloneInterface};
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
        return ::ebl::Index<T, Builtins...>::value;
    }
    constexpr TypeInfoTable() : table{makeInfo<Builtins>()...}
    {
    }
    TypeInfo table[sizeof...(Builtins)];
};


constexpr TypeInfoTable<Null, Pair, Boolean, Integer, Float, Complex, String,
                        Character, Symbol, RawPointer, Function, Box>
    typeInfoTable;


template <typename Ptr> const TypeInfo& typeInfo(Ptr obj)
{
    return typeInfoTable[obj->typeId()];
}


template <typename T> constexpr TypeId typeId()
{
    return typeInfoTable.typeId<T>();
}


inline ObjectPtr clone(Environment& env, ObjectPtr obj)
{
    return typeInfoTable[obj->typeId()].clonePolicy(env, obj);
}


template <typename T>
ObjectTemplate<T>::ObjectTemplate() : Object{::ebl::typeId<T>()}
{
}


template <typename T, typename Ptr> bool isType(Ptr obj)
{
    return typeId<T>() == obj->typeId();
}


template <typename T> Heap::Ptr<T> checkedCast(ObjectPtr obj)
{
    if (LIKELY(isType<T>(obj))) {
        return obj.cast<T>();
    }
    throw ConversionError{obj->typeId(), typeId<T>()};
}

std::ostream& operator<<(std::ostream& out, const String& str);
std::ostream& operator<<(std::ostream& out, const Character& c);

} // namespace ebl
