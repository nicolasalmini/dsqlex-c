#pragma once
#include "dsqlex/ast.hpp"
#include "dsqlex/tokens.hpp"
#include <vector>

namespace dsqlex {

/// Parse a token stream into an AST.
/// Throws std::runtime_error on parse errors.
ASTPtr parse(const std::vector<Token>& tokens);

/// Convenience: tokenize + parse in one call.
ASTPtr parse(const std::string& expression);

} // namespace dsqlex
