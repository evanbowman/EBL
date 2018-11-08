#pragma once

#include "ast.hpp"

namespace lisp {

ast::Ptr<ast::Begin> parse(const std::string& code);

}
