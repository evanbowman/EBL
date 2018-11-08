#include "lisp.hpp"
#include "types.hpp"

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
    switch (type) {
    case lisp::typeInfo.typeId<lisp::Integer>(): {
        if (lhs.cast<lisp::Integer>()->value() not_eq
            rhs.cast<lisp::Integer>()->value()) {
            return false;
        }
    } break;
    default:
        std::cerr << "bad input" << std::endl;
        break;
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
    default:
        std::cerr << "bad input" << std::endl;
        return 0;
        break;
    }
}

class CFunctionImpl : public Function::Impl {
public:
    CFunctionImpl(CFunction fn) : fn_(fn)
    {
    }

    ObjectPtr call(Environment& env, std::vector<ObjectPtr>& params) override
    {
        return fn_(env, Arguments{(int)params.size(), params.data()});
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

    ObjectPtr call(Environment& env, std::vector<ObjectPtr>& params) override
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
        bool operator()(const std::vector<ObjectPtr>& lhs,
                        const std::vector<ObjectPtr>& rhs) const
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
        size_t operator()(const std::vector<lisp::ObjectPtr>& key) const
        {
            Hash hash;
            size_t res = 17;
            for (auto& elem : key) {
                res = res * 31 + hash(elem);
            }
            return res;
        }
    };

    std::unordered_map<std::vector<ObjectPtr>, ObjectPtr, ParamHash, ParamEq>
        results_;
    std::unique_ptr<Function::Impl> wrapped_;
};

class BytecodeFunctionImpl : public Function::Impl {
public:
    ObjectPtr call(Environment& env, std::vector<ObjectPtr>& params) override
    {
        auto derivedEnv = env.derive(/* TODO: stackSize_ */);
        // ... TODO ...
        return env.getNull();
    }

private:
    size_t stackSize_;
};

Function::Function(TypeId tp, Environment& env, const char* docstring,
                   Arguments::Count requiredArgs, CFunction impl)
    : Object{tp}, docstring_(docstring), requiredArgs_(requiredArgs),
      impl_(std::unique_ptr<CFunctionImpl>(new CFunctionImpl(impl))),
      envPtr_(env.reference())
{
}

Function::Function(TypeId tp, Environment& env, const char* docstring,
                   Arguments::Count requiredArgs, std::unique_ptr<Impl> impl)
    : Object{tp}, docstring_(docstring), requiredArgs_(requiredArgs),
      impl_(std::move(impl)), envPtr_(env.reference())
{
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
