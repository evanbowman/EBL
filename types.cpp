#include "types.hpp"
#include "lisp.hpp"

#include <memory>

namespace lisp {

class CFunctionImpl : public Subr::Impl {
public:
    CFunctionImpl(Subr::CFunction fn) : fn_(fn) {}

    ObjectPtr call(Environment& env,
                   std::vector<ObjectPtr>& params) override {
        return fn_(env, Arguments{(int)params.size(), params.data()});
    }

private:
    Subr::CFunction fn_;
};


class BytecodeFunctionImpl : public Subr::Impl {
    // TODO...
};


Subr::Subr(TypeId tp,
           Environment& env,
           const char* docstring,
           Arguments::Count requiredArgs,
           CFunction impl) :
    Object{tp},
    docstring_(docstring),
    requiredArgs_(requiredArgs),
    impl_(std::unique_ptr<CFunctionImpl>(new CFunctionImpl(impl))),
    envPtr_(env.reference())
{}

}
