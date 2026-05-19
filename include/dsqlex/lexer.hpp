#pragma once
#include "dsqlex/tokens.hpp"
#include <string>
#include <vector>

namespace dsqlex {

/// Tokenize a DSQLEX expression string.
/// Returns a vector of tokens on success.
/// Throws std::runtime_error on lexer errors.
std::vector<Token> tokenize(const std::string& input);

} // namespace dsqlex
