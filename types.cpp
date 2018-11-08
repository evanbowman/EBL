#include "types.hpp"
#include "lisp.hpp"

#include <memory>

namespace lisp {

class CFunctionSubrImpl : public Subr::Impl {
public:
    CFunctionSubrImpl(CFunction fn);

    ObjectPtr call(Environment& env, std::vector<ObjectPtr>& params) override;

private:
    CFunction fn_;
};

Heap::Ptr<Subr> makeCSubr(Environment& env, CFunction fn, const char* docstring,
                          Arguments::Count argc)
{
    using ImplT = CFunctionSubrImpl;
    auto impl = std::unique_ptr<ImplT>(new ImplT(fn));
    return env.create<Subr>(docstring, argc, std::move(impl));
}

CFunctionSubrImpl::CFunctionSubrImpl(CFunction fn) : fn_(fn)
{
}

ObjectPtr CFunctionSubrImpl::call(Environment& env,
                                  std::vector<ObjectPtr>& params)
{
    return fn_(env, Arguments{(int)params.size(), params.data()});
}

class MemoizedFunctionImpl : public Subr::Impl {
public:
    MemoizedFunctionImpl(std::unique_ptr<Subr::Impl> from)
        : wrapped_(std::move(from))
    {
    }

    ObjectPtr call(Environment& env, std::vector<ObjectPtr>& params) override
    {
        return wrapped_->call(env, params);
    }

private:
    std::unique_ptr<Subr::Impl> wrapped_;
};

class BytecodeFunctionImpl : public Subr::Impl {
    // TODO...
};

Subr::Subr(TypeId tp, Environment& env, const char* docstring,
           Arguments::Count requiredArgs, std::unique_ptr<Impl> impl)
    : Object{tp}, docstring_(docstring), requiredArgs_(requiredArgs),
      impl_(std::move(impl)), envPtr_(env.reference())
{
}

void Subr::memoize()
{
    if (not dynamic_cast<MemoizedFunctionImpl*>(impl_.get())) {
        impl_ =
            std::unique_ptr<Impl>(new MemoizedFunctionImpl(std::move(impl_)));
    }
}

} // namespace lisp
