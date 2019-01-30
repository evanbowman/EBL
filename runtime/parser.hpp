#pragma once

#include "ast.hpp"

namespace ebl {

ast::Ptr<ast::TopLevel> parse(const std::string& code);
}
