#pragma once

#include "environment.hpp"

namespace lisp {

void print(Environment& env, ObjectPtr obj, std::ostream& out, bool showQuotes);
}
