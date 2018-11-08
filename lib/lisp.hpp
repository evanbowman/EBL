#pragma once

#include "environment.hpp"


namespace lisp {

ObjectPtr exec(Environment& env, const std::string& code);

}
