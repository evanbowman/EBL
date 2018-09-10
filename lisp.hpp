#pragma once

#include <tuple>
#include <string>
#include <new>
#include <vector>
#include <unordered_map>
#include "memory.hpp"
#include "utility.hpp"
#include <functional>

namespace lisp {

struct TypeInfo {
    size_t size_;
    const char* name_;
};

class Object {
public:
    // TypeInfo _could_ be a smaller integer index into the type
    // info table, but due to alignment, this isn't likely to
    // save that many bytes, at least for cons cells. May be worth
    // doing for integers, because (for 64 bit) it could save four
    // bytes per integer.
    const TypeInfo* typeInfo_;
    enum GCFlags : uint8_t {
        None = 0,
        Marked = 1 << 0
    } gcFlags_ = GCFlags::None;

    bool isLive() { return gcFlags_ & GCFlags::Marked; }
};


using ObjectPtr = Heap::Ptr<Object>;


class Null : public Object {
public:
    Null(const TypeInfo* tp)
        : Object{tp} {}

    static constexpr auto name() { return "<Null>"; }
};


class Pair : public Object {
public:
    Pair(const TypeInfo* tp, ObjectPtr car, ObjectPtr cdr)
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
    Boolean(const TypeInfo* tp, bool value)
        : Object{tp}, value_(value) {}

    static constexpr auto name() { return "<Boolean>"; }

    operator bool() const { return value_; }

private:
    bool value_;
};


class FixNum : public Object {
public:
    using Rep = uint32_t;

    FixNum(const TypeInfo* tp, Rep value)
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

    Subr(const TypeInfo* tp,
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


// FIXME: incomplete!
class String : public Object {
public:
    String(const TypeInfo* tp, const char* value) :
        Object{tp},
        value_(value) {}

    static constexpr auto name() { return "<String>"; }

    const char* value() { return value_; }

private:
    const char* value_;
};


template <typename ...Builtins>
struct TypeInfoTable {
    template <typename T>
    constexpr const TypeInfo* get() const {
        return &std::get<Info<T>>(table).info;
    }
    template <typename T>
    constexpr int index() {
        return ::Index<T, Builtins...>::value;
    }
    template <typename T>
    struct Info {
        TypeInfo info = { sizeof(T), T::name() };
    };
    std::tuple<Info<Builtins>...> table;
};

constexpr TypeInfoTable<Null,
                        Pair,
                        Boolean,
                        FixNum,
                        Subr,
                        String> typeInfo{};


struct Environment {
    Stack stack_;
    Heap heap_;
    Persistent<Boolean> booleans_[2];
    Persistent<Null> nullValue_;
    // TODO: of course we'll need something better than this!
    std::unordered_map<std::string, ObjectPtr> variables_;
    Environment();
};


template <typename T>
bool isType(Environment& env, ObjectPtr obj) {
    return typeInfo.get<T>() == obj.deref(env).typeInfo_;
}


struct ConversionError : public std::runtime_error {
    ConversionError(const TypeInfo* from, const TypeInfo* to) :
        std::runtime_error(std::string("bad cast from ") + from->name_
                           + " to " + to->name_) {}
};


template <typename T>
Heap::Ptr<T> checkedCast(Environment& env, ObjectPtr obj) {
    if (isType<T>(env, obj)) {
        return obj.cast<T>();
    }
    throw ConversionError { obj.deref(env).typeInfo_, typeInfo.get<T>() };
}


template <typename T, typename ...Args>
Heap::Ptr<T> create(Environment& env, Args&& ...args) {
    auto allocObj = [&] {
        return env.heap_.alloc<typeInfo.get<T>()->size_>().template cast<T>();
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
    new (&mem.deref(env)) T{typeInfo.get<T>(), std::forward<Args>(args)...};
    return mem;
}

} // lisp
