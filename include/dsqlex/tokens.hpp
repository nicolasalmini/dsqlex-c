#pragma once
#include <string>
#include <variant>
#include <vector>

namespace dsqlex {

enum class TokenType {
    // Operators
    Plus, Minus, Multiply, Divide,
    Eq, Neq, Lt, Gt, Lte, Gte,
    // Keywords
    Select, Case, When, Then, Else, End,
    And, Or, Not,
    Null, True, False,
    Is, In, Like,
    // Functions
    FnUpper, FnLower, FnRound, FnCoalesce, FnAbs, FnConcat, FnEvent,
    FnLeast, FnGreatest,
    // Literals & identifiers
    Number,     // value in Token::text
    String,     // value in Token::text
    Identifier, // value in Token::text
    // Structural
    LParen, RParen, Comma,
};

struct Token {
    TokenType type;
    std::string text; // payload for Number, String, Identifier
};

} // namespace dsqlex
