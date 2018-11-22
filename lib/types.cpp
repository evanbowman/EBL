#include "types.hpp"
#include "lisp.hpp"

#include <map>
#include <memory>

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

Function::Function(TypeId tp, Environment& env, ObjectPtr docstring,
                   size_t requiredArgs, CFunction impl)
    : Object{tp}, docstring_(docstring), requiredArgs_(requiredArgs),
      impl_(std::unique_ptr<CFunctionImpl>(new CFunctionImpl(impl))),
      envPtr_(env.reference())
{
}

Function::Function(TypeId tp, Environment& env, ObjectPtr docstring,
                   size_t requiredArgs, std::unique_ptr<Impl> impl)
    : Object{tp}, docstring_(docstring), requiredArgs_(requiredArgs),
      impl_(std::move(impl)), envPtr_(env.reference())
{
}

} // namespace lisp
