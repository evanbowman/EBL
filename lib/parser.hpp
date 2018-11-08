#pragma once

#include "ast.hpp"

namespace lisp {

ast::Ptr<ast::TopLevel> parse(const std::string& code);

}
