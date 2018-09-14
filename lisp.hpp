#pragma once

#include <tuple>
#include <string>
#include <new>
#include <vector>
#include <unordered_map>
#include "memory.hpp"
#include "utility.hpp"
#include <functional>
#include <limits>

namespace lisp {

using TypeId = uint8_t;

class Object {
public:
    Object(TypeId id) : header_{id, Header::Flags::None} {}

    TypeId typeId() const { return header_.typeInfoIndex; }

private:
    struct Header {
        const TypeId typeInfoIndex;
        enum Flags : uint8_t {
            None = 0,
            Marked = 1 << 0
        } gcFlags_;
    } header_;
};


using ObjectPtr = Heap::Ptr<Object>;


class Null : public Object {
public:
    Null(TypeId tp)
        : Object{tp} {}

    static constexpr auto name() { return "<Null>"; }
};


class Pair : public Object {
public:
    Pair(TypeId tp, ObjectPtr car, ObjectPtr cdr)
        : Object{tp}, car_(car), cdr_(cdr) {}

    static constexpr auto name() { return "<Pair>"; }

    ObjectPtr getCar() const { return car_; }
    ObjectPtr getCdr() const { return cdr_; }

    void setCar(ObjectPtr value) { car_ = value; }
    void setCdr(ObjectPtr value) { cdr_ = value; }

private:
    ObjectPtr car_;
    ObjectPtr cdr_;
};


class Boolean : public Object {
public:
    Boolean(TypeId tp, bool value)
        : Object{tp}, value_(value) {}

    static constexpr auto name() { return "<Boolean>"; }

    operator bool() const { return value_; }

private:
    bool value_;
};


class FixNum : public Object {
public:
    using Rep = uint32_t;

    FixNum(TypeId tp, Rep value)
        : Object{tp}, value_(value) {}

    static constexpr auto name() { return "<FixNum>"; }

    Rep value() const { return value_; }

private:
    Rep value_;
};


class Subr : public Object {
public:
    using ArgVec = ObjectPtr[];
    using ArgCount = int;
    using Impl = std::function<ObjectPtr(Environment&,
                                         ArgCount,
                                         ArgVec)>;

    Subr(TypeId tp,
         const char* docstring,
         ArgCount requiredArgs,
         Impl impl)
        : Object{tp},
          docstring_(docstring),
          requiredArgs_(requiredArgs),
          impl_(impl) {}

    static constexpr auto name() { return "<Subr>"; }

    const char* getDocstring() { return docstring_; }

    ObjectPtr call(Environment& env, std::vector<ObjectPtr>& params) {
        if ((int)params.size() < requiredArgs_) {
            throw std::runtime_error("not enough params for subr!");
        }
        return impl_(env, params.size(), params.data());
    }

private:
    const char* docstring_;
    ArgCount requiredArgs_;
    Impl impl_;
};


struct TypeInfo {
    size_t size_;
    const char* name_;
};

template <typename T>
constexpr TypeInfo makeInfo() { return { sizeof(T), T::name() }; }

template <typename ...Builtins>
struct TypeInfoTable {
    template <typename T>
    constexpr const TypeInfo& get() const {
        return table[typeId<T>()];
    }
    const TypeInfo& operator[](TypeId index) const { return table[index]; }
    template <typename T>
    constexpr TypeId typeId() const {
        static_assert(sizeof...(Builtins) < std::numeric_limits<TypeId>::max(),
                      "TypeInfoTable parameter count exhausts the range of "
                      "TypeId");
        return ::Index<T, Builtins...>::value;
    }
    TypeInfo table[sizeof...(Builtins)] = { makeInfo<Builtins>()... };
};

constexpr TypeInfoTable<Null,
                        Pair,
                        Boolean,
                        FixNum,
                        Subr> typeInfo{};


struct Environment {
    Stack stack_;
    Heap heap_;
    Persistent<Boolean> booleans_[2];
    Persistent<Null> nullValue_;
    // TODO: of course we'll need something better than this!
    std::unordered_map<std::string, ObjectPtr> topLevel_;
    Environment();
};


template <typename T>
bool isType(Environment& env, ObjectPtr obj) {
    return typeInfo.typeId<T>() == obj.deref(env).typeId();
}

struct ConversionError : public std::runtime_error {
    ConversionError(TypeId from, TypeId to) :
        std::runtime_error(std::string("bad cast from ") + typeInfo[from].name_
                           + " to " + typeInfo[to].name_) {}
};


template <typename T>
Heap::Ptr<T> checkedCast(Environment& env, ObjectPtr obj) {
    if (isType<T>(env, obj)) {
        return obj.cast<T>();
    }
    throw ConversionError { obj.deref(env).typeId(), typeInfo.typeId<T>() };
}


template <typename T, typename ...Args>
Heap::Ptr<T> create(Environment& env, Args&& ...args) {
    auto allocObj = [&] {
        return env.heap_.alloc<typeInfo.get<T>().size_>().template cast<T>();
    };
    auto mem = [&] {
        try {
            return allocObj();
        } catch (const Heap::OOM& oom) {
            GC gc;
            gc.run(env);
            return allocObj();
        }
    }();
    new (&mem.deref(env)) T{typeInfo.typeId<T>(), std::forward<Args>(args)...};
    return mem;
}

} // lisp
