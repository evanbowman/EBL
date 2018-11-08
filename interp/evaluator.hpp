#pragma once

#include "lib/lisp.hpp"

namespace lisp {

ObjectPtr eval(Environment& env, const std::string& code);

} // namespace lisp
