#include "dsqlex/parser.hpp"
#include "dsqlex/lexer.hpp"
#include <stdexcept>
#include <string>

namespace dsqlex {

// ---- AST factory implementations -------------------------------------------

ASTPtr ASTNode::make_select(ASTPtr inner) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::Select;
    n->expr = std::move(inner);
    return n;
}
ASTPtr ASTNode::make_number(const std::string& val) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::NumberLit;
    n->text = val;
    return n;
}
ASTPtr ASTNode::make_string(const std::string& val) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::StringLit;
    n->text = val;
    return n;
}
ASTPtr ASTNode::make_bool(bool val) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::BoolLit;
    n->bool_val = val;
    return n;
}
ASTPtr ASTNode::make_null() {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::NullLit;
    return n;
}
ASTPtr ASTNode::make_identifier(const std::string& name) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::Identifier;
    n->text = name;
    return n;
}
ASTPtr ASTNode::make_binary_op(BinOp op, ASTPtr left, ASTPtr right) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::BinaryOp;
    n->op = op;
    n->left = std::move(left);
    n->right = std::move(right);
    return n;
}
ASTPtr ASTNode::make_case(std::vector<WhenClause> whens, ASTPtr else_clause) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::CaseExpr;
    n->whens = std::move(whens);
    n->else_clause = std::move(else_clause);
    return n;
}
ASTPtr ASTNode::make_function(const std::string& name, std::vector<ASTPtr> args) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::FunctionCall;
    n->func_name = name;
    n->items = std::move(args);
    return n;
}
ASTPtr ASTNode::make_in(ASTPtr expr, std::vector<ASTPtr> items) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::InExpr;
    n->expr = std::move(expr);
    n->items = std::move(items);
    return n;
}
ASTPtr ASTNode::make_not_in(ASTPtr expr, std::vector<ASTPtr> items) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::NotInExpr;
    n->expr = std::move(expr);
    n->items = std::move(items);
    return n;
}
ASTPtr ASTNode::make_like(ASTPtr expr, ASTPtr pattern) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::LikeExpr;
    n->expr = std::move(expr);
    n->right = std::move(pattern);
    return n;
}
ASTPtr ASTNode::make_not_like(ASTPtr expr, ASTPtr pattern) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::NotLikeExpr;
    n->expr = std::move(expr);
    n->right = std::move(pattern);
    return n;
}
ASTPtr ASTNode::make_unary_op(BinOp op, ASTPtr operand) {
    auto n = std::make_unique<ASTNode>();
    n->kind = NodeKind::UnaryOp;
    n->op = op;
    n->expr = std::move(operand);
    return n;
}

// ---- Recursive Descent Parser -----------------------------------------------

namespace {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens), pos_(0) {}

    ASTPtr parse_program() {
        // SELECT is optional
        if (peek_is(TokenType::Select))
            advance();

        auto expr = parse_logical();

        if (pos_ < tokens_.size()) {
            throw std::runtime_error(
                "Unexpected tokens after expression");
        }

        return ASTNode::make_select(std::move(expr));
    }

private:
    const std::vector<Token>& tokens_;
    size_t pos_;

    // -- Helpers -------
    bool at_end() const { return pos_ >= tokens_.size(); }

    const Token& peek() const {
        static Token eof{TokenType::RParen, ""}; // sentinel
        return at_end() ? eof : tokens_[pos_];
    }

    bool peek_is(TokenType t) const { return !at_end() && tokens_[pos_].type == t; }

    const Token& advance() {
        return tokens_[pos_++];
    }

    void expect(TokenType t, const std::string& msg) {
        if (at_end() || tokens_[pos_].type != t)
            throw std::runtime_error(msg);
        ++pos_;
    }

    // Is the token a function name?
    static bool is_function(TokenType t) {
        switch (t) {
            case TokenType::FnUpper: case TokenType::FnLower:
            case TokenType::FnRound: case TokenType::FnCoalesce:
            case TokenType::FnAbs:   case TokenType::FnConcat:
            case TokenType::FnLeast: case TokenType::FnGreatest:
            case TokenType::FnEvent:
                return true;
            default: return false;
        }
    }

    static std::string fn_name(TokenType t) {
        switch (t) {
            case TokenType::FnUpper:    return "UPPER";
            case TokenType::FnLower:    return "LOWER";
            case TokenType::FnRound:    return "ROUND";
            case TokenType::FnCoalesce: return "COALESCE";
            case TokenType::FnAbs:      return "ABS";
            case TokenType::FnConcat:   return "CONCAT";
            case TokenType::FnLeast:    return "LEAST";
            case TokenType::FnGreatest: return "GREATEST";
            case TokenType::FnEvent:    return "EVENT";
            default: return "UNKNOWN";
        }
    }

    // Is the token a comparison operator?
    static bool is_comparison(TokenType t) {
        switch (t) {
            case TokenType::Eq: case TokenType::Neq:
            case TokenType::Lt: case TokenType::Gt:
            case TokenType::Lte: case TokenType::Gte:
                return true;
            default: return false;
        }
    }

    static BinOp to_comparison_op(TokenType t) {
        switch (t) {
            case TokenType::Eq:  return BinOp::Eq;
            case TokenType::Neq: return BinOp::Neq;
            case TokenType::Lt:  return BinOp::Lt;
            case TokenType::Gt:  return BinOp::Gt;
            case TokenType::Lte: return BinOp::Lte;
            case TokenType::Gte: return BinOp::Gte;
            default: throw std::runtime_error("Not a comparison operator");
        }
    }

    // -- Grammar rules (lowest to highest precedence) -------

    // logical := comparison ( (AND|OR) comparison )*
    // Same-op chaining allowed, mixed AND/OR requires parentheses.
    ASTPtr parse_logical() {
        auto left = parse_comparison();

        if (at_end() || (!peek_is(TokenType::And) && !peek_is(TokenType::Or)))
            return left;

        auto first_op_type = peek().type;
        while (!at_end() && (peek_is(TokenType::And) || peek_is(TokenType::Or))) {
            auto op_type = peek().type;

            if (op_type != first_op_type)
                throw std::runtime_error(
                    "Ambiguous expression: mixing AND/OR requires parentheses");

            advance();
            auto right = parse_comparison();

            BinOp bop = (op_type == TokenType::And) ? BinOp::And : BinOp::Or;
            left = ASTNode::make_binary_op(bop, std::move(left), std::move(right));
        }

        return left;
    }

    // comparison := arithmetic ( (=|!=|<|>|<=|>=) arithmetic )?
    // Also handles: IS [NOT] NULL/TRUE/FALSE, [NOT] IN (...), [NOT] LIKE
    ASTPtr parse_comparison() {
        auto left = parse_arithmetic();

        // IS [NOT] NULL/TRUE/FALSE
        if (peek_is(TokenType::Is)) {
            advance();
            bool negated = false;
            if (peek_is(TokenType::Not)) {
                negated = true;
                advance();
            }
            ASTPtr rhs;
            if (peek_is(TokenType::Null)) {
                advance();
                rhs = ASTNode::make_null();
            } else if (peek_is(TokenType::True)) {
                advance();
                rhs = ASTNode::make_bool(true);
            } else if (peek_is(TokenType::False)) {
                advance();
                rhs = ASTNode::make_bool(false);
            } else {
                throw std::runtime_error("Expected NULL, TRUE, or FALSE after IS");
            }
            auto result = ASTNode::make_binary_op(BinOp::Eq, std::move(left), std::move(rhs));
            if (negated)
                result = ASTNode::make_binary_op(BinOp::Neq,
                    std::move(result->left), std::move(result->right));
            return result;
        }

        // [NOT] IN (...)
        bool not_prefix = false;
        if (peek_is(TokenType::Not)) {
            // look ahead for IN or LIKE
            if (pos_ + 1 < tokens_.size() &&
                (tokens_[pos_ + 1].type == TokenType::In ||
                 tokens_[pos_ + 1].type == TokenType::Like)) {
                not_prefix = true;
                advance(); // consume NOT
            }
        }

        if (peek_is(TokenType::In)) {
            advance();
            expect(TokenType::LParen, "Expected '(' after IN");
            std::vector<ASTPtr> items;
            if (!peek_is(TokenType::RParen)) {
                items.push_back(parse_primary());
                while (peek_is(TokenType::Comma)) {
                    advance();
                    items.push_back(parse_primary());
                }
            }
            expect(TokenType::RParen, "Expected closing parenthesis ')' after IN list");
            if (not_prefix)
                return ASTNode::make_not_in(std::move(left), std::move(items));
            return ASTNode::make_in(std::move(left), std::move(items));
        }

        // [NOT] LIKE
        if (peek_is(TokenType::Like)) {
            advance();
            auto pattern = parse_primary();
            if (not_prefix)
                return ASTNode::make_not_like(std::move(left), std::move(pattern));
            return ASTNode::make_like(std::move(left), std::move(pattern));
        }

        // Standard comparison operators
        if (!at_end() && is_comparison(peek().type)) {
            auto op_tok = advance();
            auto right = parse_arithmetic();

            // No chaining: a = 1 = 2 is an error
            if (!at_end() && is_comparison(peek().type))
                throw std::runtime_error(
                    "Cannot chain comparison operators. Use parentheses.");

            return ASTNode::make_binary_op(to_comparison_op(op_tok.type),
                                           std::move(left), std::move(right));
        }

        return left;
    }

    // arithmetic := primary ( op primary )*
    // Matches Elixir semantics: same-group chaining allowed, but mixing
    // additive (+/-) and multiplicative (*/) is an error.
    // `a + b + c` OK, `a * b / c` OK, `a + b * c` ERROR.
    ASTPtr parse_arithmetic() {
        auto left = parse_primary();

        if (at_end()) return left;

        // Determine which group we're in based on the first operator
        bool is_additive_first = peek_is(TokenType::Plus) || peek_is(TokenType::Minus);
        bool is_mult_first = peek_is(TokenType::Multiply) || peek_is(TokenType::Divide);

        if (!is_additive_first && !is_mult_first)
            return left;

        if (is_additive_first) {
            // Additive chain: only + and - allowed
            while (!at_end() && (peek_is(TokenType::Plus) || peek_is(TokenType::Minus))) {
                auto op_tok = advance();
                auto right = parse_primary();
                BinOp bop = (op_tok.type == TokenType::Plus) ? BinOp::Plus : BinOp::Minus;
                left = ASTNode::make_binary_op(bop, std::move(left), std::move(right));
            }
            // If next token is * or /, that's ambiguous
            if (!at_end() && (peek_is(TokenType::Multiply) || peek_is(TokenType::Divide)))
                throw std::runtime_error(
                    "Ambiguous expression: mixing +/- and *// requires parentheses");
        } else {
            // Multiplicative chain: only * and / allowed
            while (!at_end() && (peek_is(TokenType::Multiply) || peek_is(TokenType::Divide))) {
                auto op_tok = advance();
                auto right = parse_primary();
                BinOp bop = (op_tok.type == TokenType::Multiply) ? BinOp::Multiply : BinOp::Divide;
                left = ASTNode::make_binary_op(bop, std::move(left), std::move(right));
            }
            // If next token is + or -, that's ambiguous
            if (!at_end() && (peek_is(TokenType::Plus) || peek_is(TokenType::Minus)))
                throw std::runtime_error(
                    "Ambiguous expression: mixing +/- and *// requires parentheses");
        }

        return left;
    }

    // primary := literal | identifier | '(' logical ')' | CASE | function_call
    ASTPtr parse_primary() {
        if (at_end())
            throw std::runtime_error("Unexpected end of expression");

        auto& tok = peek();

        if (tok.type == TokenType::Minus) {
            advance();
            return ASTNode::make_unary_op(BinOp::Minus, parse_primary());
        }

        // Parenthesized expression
        if (tok.type == TokenType::LParen) {
            advance();
            auto inner = parse_logical();
            expect(TokenType::RParen, "Expected closing parenthesis ')'");
            return inner;
        }

        // CASE expression
        if (tok.type == TokenType::Case) {
            return parse_case();
        }

        // Function call
        if (is_function(tok.type)) {
            return parse_function_call();
        }

        // Literals
        if (tok.type == TokenType::Number) {
            auto val = tok.text;
            advance();
            return ASTNode::make_number(val);
        }
        if (tok.type == TokenType::String) {
            auto val = tok.text;
            advance();
            return ASTNode::make_string(val);
        }
        if (tok.type == TokenType::True) {
            advance();
            return ASTNode::make_bool(true);
        }
        if (tok.type == TokenType::False) {
            advance();
            return ASTNode::make_bool(false);
        }
        if (tok.type == TokenType::Null) {
            advance();
            return ASTNode::make_null();
        }

        // Identifier
        if (tok.type == TokenType::Identifier) {
            auto name = tok.text;
            advance();
            return ASTNode::make_identifier(name);
        }

        throw std::runtime_error("Unexpected token: " + tok.text);
    }

    // CASE WHEN cond THEN result [WHEN ...] [ELSE result] END
    ASTPtr parse_case() {
        expect(TokenType::Case, "Expected CASE");

        std::vector<WhenClause> whens;
        while (peek_is(TokenType::When)) {
            advance();
            auto cond = parse_logical();
            expect(TokenType::Then, "Expected THEN after WHEN condition");
            auto result = parse_logical();
            whens.push_back({std::move(cond), std::move(result)});
        }

        ASTPtr else_clause;
        if (peek_is(TokenType::Else)) {
            advance();
            else_clause = parse_logical();
        }

        expect(TokenType::End, "Expected END to close CASE expression");
        return ASTNode::make_case(std::move(whens), std::move(else_clause));
    }

    // func_name '(' [arg (',' arg)*] ')'
    ASTPtr parse_function_call() {
        auto name = fn_name(peek().type);
        advance();
        expect(TokenType::LParen, "Expected '(' after function name");

        std::vector<ASTPtr> args;
        if (!peek_is(TokenType::RParen)) {
            args.push_back(parse_logical());
            while (peek_is(TokenType::Comma)) {
                advance();
                args.push_back(parse_logical());
            }
        }

        expect(TokenType::RParen,
               "Expected closing parenthesis ')' after function arguments");
        return ASTNode::make_function(name, std::move(args));
    }
};

} // anonymous namespace

// ---- Public API -----

ASTPtr parse(const std::vector<Token>& tokens) {
    Parser p(tokens);
    return p.parse_program();
}

ASTPtr parse(const std::string& expression) {
    auto tokens = tokenize(expression);
    return parse(tokens);
}

} // namespace dsqlex
