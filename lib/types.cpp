#include "types.hpp"
#include "lisp.hpp"

#include <memory>
#include <unordered_map>

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
    default:
        throw TypeError(type, "no equalto defined for input");
    }
    return true;
}

size_t Hash::operator()(ObjectPtr obj) const noexcept
{
    switch (obj->typeId()) {
    case lisp::typeInfo.typeId<lisp::Integer>(): {
        std::hash<lisp::Integer::Rep> hash;
        return hash(obj.cast<lisp::Integer>()->value());
    } break;
    // case lisp::typeInfo.typeId<lisp::Complex>(): {
    //     std::hash<lisp::Complex::Rep> hash;
    //     return hash(obj.cast<lisp::Complex>()->value());
    // } break;
    default:
        throw TypeError(obj->typeId(), "no hash defined for input");
    }
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

// This is somewhat expensive, doesn't make sense for small functions.  FIXME:
// The GC needs to be able to see what results are stored in the internal
// table. The solution could be that a GC run triggers a clearing of the
// internal result table, which may be better than creating lots of persistents.
class MemoizedFunctionImpl : public Function::Impl {
public:
    MemoizedFunctionImpl(std::unique_ptr<Function::Impl> from)
        : wrapped_(std::move(from))
    {
    }

    ObjectPtr call(Environment& env, Arguments& params) override
    {
        auto found = results_.find(params);
        if (found not_eq results_.end()) {
            return found->second;
        } else {
            auto result = wrapped_->call(env, params);
            results_.insert({params, result});
            return result;
        }
    }

    std::unique_ptr<Impl> destroy()
    {
        return std::move(wrapped_);
    }

private:
    struct ParamEq {
        bool operator()(const Arguments& lhs, const Arguments& rhs) const
        {
            if (lhs.size() not_eq rhs.size()) {
                return false;
            }
            EqualTo eq;
            for (size_t i = 0; i < lhs.size(); ++i) {
                if (not eq(lhs[i], rhs[i])) {
                    return false;
                }
            }
            return true;
        }
    };

    struct ParamHash {
        size_t operator()(const Arguments& key) const
        {
            Hash hash;
            size_t res = 17;
            for (auto& elem : key) {
                res = res * 31 + hash(elem);
            }
            return res;
        }
    };

    std::unordered_map<Arguments, ObjectPtr, ParamHash, ParamEq> results_;
    std::unique_ptr<Function::Impl> wrapped_;
};

Function::Function(TypeId tp, Environment& env, const char* docstring,
                   size_t requiredArgs, CFunction impl)
    : Object{tp}, docstring_(env.getNull()), requiredArgs_(requiredArgs),
      impl_(std::unique_ptr<CFunctionImpl>(new CFunctionImpl(impl))),
      envPtr_(env.reference())
{
    if (docstring) {
        docstring_ = env.create<String>(docstring, strlen(docstring));
    }
}

Function::Function(TypeId tp, Environment& env, const char* docstring,
                   size_t requiredArgs, std::unique_ptr<Impl> impl)
    : Object{tp}, docstring_(env.getNull()), requiredArgs_(requiredArgs),
      impl_(std::move(impl)), envPtr_(env.reference())
{
    if (docstring) {
        docstring_ = env.create<String>(docstring, strlen(docstring));
    }
}

bool Function::isMemoized() const
{
    return dynamic_cast<MemoizedFunctionImpl*>(impl_.get());
}

void Function::memoize()
{
    if (not isMemoized()) {
        impl_ =
            std::unique_ptr<Impl>(new MemoizedFunctionImpl(std::move(impl_)));
    }
}

void Function::unmemoize()
{
    if (auto impl = dynamic_cast<MemoizedFunctionImpl*>(impl_.get())) {
        impl_ = impl->destroy();
    }
}

} // namespace lisp
