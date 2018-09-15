#include "types.hpp"
#include "lisp.hpp"

namespace lisp {

Subr::Subr(TypeId tp,
           Environment& env,
           const char* docstring,
           Arguments::Count requiredArgs,
           Impl impl) :
    Object{tp},
    docstring_(docstring),
    requiredArgs_(requiredArgs),
    impl_(impl),
    envPtr_(env.reference()) {}


ObjectPtr Subr::call(std::vector<ObjectPtr>& params) {
    if ((int)params.size() < requiredArgs_) {
        throw std::runtime_error("not enough params for subr!");
    }
    return impl_(*envPtr_, Arguments{(int)params.size(), params.data()});
}


}
