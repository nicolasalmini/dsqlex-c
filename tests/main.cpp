// Minimal test harness (no external dependency)
#include "dsqlex/lexer.hpp"
#include "dsqlex/parser.hpp"
#include "dsqlex/evaluator.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        ++tests_run; \
        std::cout << "  " << #name << "... "; \
        try { \
            test_##name(); \
            ++tests_passed; \
            std::cout << "OK\n"; \
        } catch (const std::exception& e) { \
            ++tests_failed; \
            std::cout << "FAIL: " << e.what() << "\n"; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { if (!((a) == (b))) throw std::runtime_error( \
        std::string("Assertion failed: ") + #a + " == " + #b); } while (0)

#define ASSERT_TRUE(x) \
    do { if (!(x)) throw std::runtime_error( \
        std::string("Assertion failed: ") + #x); } while (0)

#define ASSERT_THROWS(expr) \
    do { bool caught = false; \
         try { expr; } catch (...) { caught = true; } \
         if (!caught) throw std::runtime_error( \
             std::string("Expected exception from: ") + #expr); \
    } while (0)

using namespace dsqlex;

// Helper: evaluate expression with context, return Value
static Value eval(const std::string& expr, const Context& ctx = {}) {
    auto ast = parse(expr);
    return evaluate(*ast, ctx);
}

static decimal::Decimal dec(const std::string& s) {
    return decimal::Decimal(s);
}

static decimal::Decimal get_decimal(const Value& v) {
    return std::get<decimal::Decimal>(v);
}

static std::string get_string(const Value& v) {
    return std::get<std::string>(v);
}

static bool get_bool(const Value& v) {
    return std::get<bool>(v);
}

// ===== LEXER TESTS ==========================================================

void test_lexer_simple_tokens() {
    auto tokens = tokenize("+ - * / = != < > <= >=");
    ASSERT_EQ(tokens.size(), 10u);
    ASSERT_EQ(tokens[0].type, TokenType::Plus);
    ASSERT_EQ(tokens[4].type, TokenType::Eq);
    ASSERT_EQ(tokens[5].type, TokenType::Neq);
    ASSERT_EQ(tokens[8].type, TokenType::Lte);
    ASSERT_EQ(tokens[9].type, TokenType::Gte);
}

void test_lexer_keywords() {
    auto tokens = tokenize("SELECT CASE WHEN THEN ELSE END AND OR NULL TRUE FALSE");
    ASSERT_EQ(tokens.size(), 11u);
    ASSERT_EQ(tokens[0].type, TokenType::Select);
    ASSERT_EQ(tokens[1].type, TokenType::Case);
    ASSERT_EQ(tokens[6].type, TokenType::And);
    ASSERT_EQ(tokens[9].type, TokenType::True);
}

void test_lexer_case_insensitive() {
    auto tokens = tokenize("select Case WHEN true false null");
    ASSERT_EQ(tokens[0].type, TokenType::Select);
    ASSERT_EQ(tokens[1].type, TokenType::Case);
    ASSERT_EQ(tokens[2].type, TokenType::When);
    ASSERT_EQ(tokens[3].type, TokenType::True);
}

void test_lexer_numbers() {
    auto tokens = tokenize("42 3.14 100.00");
    ASSERT_EQ(tokens.size(), 3u);
    ASSERT_EQ(tokens[0].type, TokenType::Number);
    ASSERT_EQ(tokens[0].text, "42");
    ASSERT_EQ(tokens[1].text, "3.14");
    ASSERT_EQ(tokens[2].text, "100.00");
}

void test_lexer_strings() {
    auto tokens = tokenize("'hello' 'world' ''");
    ASSERT_EQ(tokens.size(), 3u);
    ASSERT_EQ(tokens[0].type, TokenType::String);
    ASSERT_EQ(tokens[0].text, "hello");
    ASSERT_EQ(tokens[1].text, "world");
    ASSERT_EQ(tokens[2].text, "");
}

void test_lexer_identifiers() {
    auto tokens = tokenize("amount currency_rate config.pricing.margin");
    ASSERT_EQ(tokens.size(), 3u);
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[0].text, "amount");
    ASSERT_EQ(tokens[1].text, "currency_rate");
    ASSERT_EQ(tokens[2].text, "config.pricing.margin");
}

void test_lexer_functions() {
    auto tokens = tokenize("ROUND COALESCE NVL UPPER LOWER ABS CONCAT EVENT");
    ASSERT_EQ(tokens.size(), 8u);
    ASSERT_EQ(tokens[0].type, TokenType::FnRound);
    ASSERT_EQ(tokens[1].type, TokenType::FnCoalesce);
    ASSERT_EQ(tokens[2].type, TokenType::FnCoalesce); // NVL alias
    ASSERT_EQ(tokens[3].type, TokenType::FnUpper);
}

void test_lexer_comments() {
    auto tokens = tokenize("amount -- this is a comment\n+ rate");
    ASSERT_EQ(tokens.size(), 3u);
    ASSERT_EQ(tokens[0].text, "amount");
    ASSERT_EQ(tokens[1].type, TokenType::Plus);

    tokens = tokenize("amount # hash comment\n+ rate");
    ASSERT_EQ(tokens.size(), 3u);

    tokens = tokenize("amount /* block comment */ + rate");
    ASSERT_EQ(tokens.size(), 3u);
}

void test_lexer_unterminated_string() {
    ASSERT_THROWS(tokenize("'unterminated"));
}

void test_lexer_unterminated_block_comment() {
    ASSERT_THROWS(tokenize("/* unterminated"));
}

void test_lexer_least_greatest() {
    auto tokens = tokenize("LEAST GREATEST least");
    ASSERT_EQ(tokens.size(), 3u);
    ASSERT_EQ(tokens[0].type, TokenType::FnLeast);
    ASSERT_EQ(tokens[1].type, TokenType::FnGreatest);
    ASSERT_EQ(tokens[2].type, TokenType::FnLeast);
}

void test_lexer_trailing_question_mark() {
    auto tokens = tokenize("active?");
    ASSERT_EQ(tokens.size(), 1u);
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[0].text, "active?");

    tokens = tokenize("user.active?");
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[0].text, "user.active?");
}

void test_lexer_question_mark_not_keyword() {
    auto tokens = tokenize("select?");
    ASSERT_EQ(tokens.size(), 1u);
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[0].text, "select?");
}

void test_lexer_double_question_mark_rejected() {
    ASSERT_THROWS(tokenize("a??"));
}

// ===== PARSER TESTS ==========================================================

void test_parser_simple_field() {
    auto ast = parse("field1");
    ASSERT_EQ(ast->kind, NodeKind::Select);
    ASSERT_EQ(ast->expr->kind, NodeKind::Identifier);
    ASSERT_EQ(ast->expr->text, "field1");
}

void test_parser_select_optional() {
    auto ast = parse("SELECT amount");
    ASSERT_EQ(ast->kind, NodeKind::Select);
    ASSERT_EQ(ast->expr->kind, NodeKind::Identifier);

    auto ast2 = parse("amount");
    ASSERT_EQ(ast2->kind, NodeKind::Select);
    ASSERT_EQ(ast2->expr->kind, NodeKind::Identifier);
}

void test_parser_arithmetic() {
    auto ast = parse("a + b");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::BinaryOp);
    ASSERT_EQ(inner.op, BinOp::Plus);
}

void test_parser_arithmetic_chaining() {
    auto ast = parse("a + b + c");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::BinaryOp);
    ASSERT_EQ(inner.op, BinOp::Plus);
    // Left should be (a + b)
    ASSERT_EQ(inner.left->kind, NodeKind::BinaryOp);
    ASSERT_EQ(inner.left->op, BinOp::Plus);
}

void test_parser_mixed_arithmetic_rejected() {
    ASSERT_THROWS(parse("a + b * c"));
    ASSERT_THROWS(parse("a * b + c"));
}

void test_parser_parenthesized_mixed() {
    // With parens, mixed ops are fine
    auto ast = parse("(a + b) * c");
    ASSERT_EQ(ast->expr->kind, NodeKind::BinaryOp);
    ASSERT_EQ(ast->expr->op, BinOp::Multiply);
}

void test_parser_case() {
    auto ast = parse("CASE WHEN x = 1 THEN 'one' ELSE 'other' END");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::CaseExpr);
    ASSERT_EQ(inner.whens.size(), 1u);
    ASSERT_TRUE(inner.else_clause != nullptr);
}

void test_parser_function() {
    auto ast = parse("ROUND(amount, 2)");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::FunctionCall);
    ASSERT_EQ(inner.func_name, "ROUND");
    ASSERT_EQ(inner.items.size(), 2u);
}

void test_parser_in_expr() {
    auto ast = parse("status IN ('active', 'pending')");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::InExpr);
    ASSERT_EQ(inner.items.size(), 2u);
}

void test_parser_not_in_expr() {
    auto ast = parse("status NOT IN ('deleted')");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::NotInExpr);
}

void test_parser_like() {
    auto ast = parse("name LIKE '%test%'");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::LikeExpr);
}

void test_parser_not_like() {
    auto ast = parse("name NOT LIKE '%test%'");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::NotLikeExpr);
}

void test_parser_is_null() {
    auto ast = parse("x IS NULL");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::BinaryOp);
    ASSERT_EQ(inner.op, BinOp::Eq);
    ASSERT_EQ(inner.right->kind, NodeKind::NullLit);
}

void test_parser_is_not_null() {
    auto ast = parse("x IS NOT NULL");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::BinaryOp);
    ASSERT_EQ(inner.op, BinOp::Neq);
}

void test_parser_mixed_logical_rejected() {
    ASSERT_THROWS(parse("a = 1 AND b = 2 OR c = 3"));
}

void test_parser_same_logical_ok() {
    auto ast = parse("a = 1 AND b = 2 AND c = 3");
    ASSERT_EQ(ast->expr->kind, NodeKind::BinaryOp);
    ASSERT_EQ(ast->expr->op, BinOp::And);
}

void test_parser_comparison_no_chain() {
    ASSERT_THROWS(parse("a = 1 = 2"));
}

void test_parser_unary_minus() {
    auto ast = parse("-1");
    ASSERT_EQ(ast->expr->kind, NodeKind::UnaryOp);
    ASSERT_EQ(ast->expr->expr->kind, NodeKind::NumberLit);
}

void test_parser_unary_minus_multiply() {
    auto ast = parse("amount * -1");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::BinaryOp);
    ASSERT_EQ(inner.op, BinOp::Multiply);
    ASSERT_EQ(inner.right->kind, NodeKind::UnaryOp);
}

void test_parser_unary_minus_paren() {
    auto ast = parse("-(1 + 2)");
    ASSERT_EQ(ast->expr->kind, NodeKind::UnaryOp);
    ASSERT_EQ(ast->expr->expr->kind, NodeKind::BinaryOp);
    ASSERT_EQ(ast->expr->expr->op, BinOp::Plus);
}

void test_parser_subtract_negative() {
    auto ast = parse("5 - - 2");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::BinaryOp);
    ASSERT_EQ(inner.op, BinOp::Minus);
    ASSERT_EQ(inner.right->kind, NodeKind::UnaryOp);
}

void test_parser_nested_unary_minus() {
    auto ast = parse("- -5");
    ASSERT_EQ(ast->expr->kind, NodeKind::UnaryOp);
    ASSERT_EQ(ast->expr->expr->kind, NodeKind::UnaryOp);
}

void test_parser_double_minus_is_comment() {
    ASSERT_THROWS(parse("--5"));
}

void test_parser_negative_in_list() {
    auto ast = parse("x IN (1, -2)");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::InExpr);
    ASSERT_EQ(inner.items.size(), 2u);
    ASSERT_EQ(inner.items[1]->kind, NodeKind::UnaryOp);
}

void test_parser_empty_in_list() {
    auto ast = parse("x IN ()");
    ASSERT_EQ(ast->expr->kind, NodeKind::InExpr);
    ASSERT_EQ(ast->expr->items.size(), 0u);
}

void test_parser_mixed_group_unary_rejected() {
    ASSERT_THROWS(parse("1 + 2 * -3"));
}

void test_parser_least_greatest() {
    auto ast = parse("LEAST(a, 1, 2)");
    auto& inner = *ast->expr;
    ASSERT_EQ(inner.kind, NodeKind::FunctionCall);
    ASSERT_EQ(inner.func_name, "LEAST");
    ASSERT_EQ(inner.items.size(), 3u);
}

// ===== EVALUATOR TESTS =======================================================

void test_eval_number_literal() {
    auto v = eval("42");
    ASSERT_EQ(get_decimal(v), dec("42"));
}

void test_eval_string_literal() {
    auto v = eval("'hello'");
    ASSERT_EQ(get_string(v), "hello");
}

void test_eval_bool_literals() {
    ASSERT_EQ(get_bool(eval("TRUE")), true);
    ASSERT_EQ(get_bool(eval("FALSE")), false);
}

void test_eval_null_literal() {
    ASSERT_TRUE(is_null(eval("NULL")));
}

void test_eval_field_lookup() {
    Context ctx;
    ctx.set("amount", Value{dec("500.00")});
    auto v = eval("amount", ctx);
    ASSERT_EQ(get_decimal(v), dec("500.00"));
}

void test_eval_arithmetic() {
    Context ctx;
    ctx.set("a", Value{dec("10")});
    ctx.set("b", Value{dec("3")});

    ASSERT_EQ(get_decimal(eval("a + b", ctx)), dec("13"));
    ASSERT_EQ(get_decimal(eval("a - b", ctx)), dec("7"));
    ASSERT_EQ(get_decimal(eval("a * b", ctx)), dec("30"));
}

void test_eval_division() {
    Context ctx;
    ctx.set("a", Value{dec("10")});
    ctx.set("b", Value{dec("3")});

    auto result = get_decimal(eval("a / b", ctx));
    // Should be a high-precision decimal
    ASSERT_TRUE(result > dec("3.33") && result < dec("3.34"));
}

void test_eval_comparison() {
    Context ctx;
    ctx.set("a", Value{dec("10")});
    ctx.set("b", Value{dec("20")});

    ASSERT_EQ(get_bool(eval("a = a", ctx)), true);
    ASSERT_EQ(get_bool(eval("a != b", ctx)), true);
    ASSERT_EQ(get_bool(eval("a < b", ctx)), true);
    ASSERT_EQ(get_bool(eval("a > b", ctx)), false);
    ASSERT_EQ(get_bool(eval("a <= a", ctx)), true);
    ASSERT_EQ(get_bool(eval("a >= b", ctx)), false);
}

void test_eval_string_comparison() {
    Context ctx;
    ctx.set("s", Value{std::string("hello")});

    ASSERT_EQ(get_bool(eval("s = 'hello'", ctx)), true);
    ASSERT_EQ(get_bool(eval("s != 'world'", ctx)), true);
}

void test_eval_null_comparison() {
    Context ctx;
    ctx.set("x", Value{NullValue{}});

    ASSERT_EQ(get_bool(eval("x IS NULL", ctx)), true);
    ASSERT_EQ(get_bool(eval("x IS NOT NULL", ctx)), false);
}

void test_eval_logical_and() {
    ASSERT_EQ(get_bool(eval("TRUE AND TRUE")), true);
    ASSERT_EQ(get_bool(eval("TRUE AND FALSE")), false);
    ASSERT_EQ(get_bool(eval("FALSE AND TRUE")), false);
}

void test_eval_logical_or() {
    ASSERT_EQ(get_bool(eval("TRUE OR FALSE")), true);
    ASSERT_EQ(get_bool(eval("FALSE OR FALSE")), false);
}

void test_eval_case_simple() {
    Context ctx;
    ctx.set("status", Value{std::string("active")});
    ctx.set("amount", Value{dec("100")});

    auto v = eval(
        "CASE WHEN status = 'active' THEN amount ELSE 0 END",
        ctx);
    ASSERT_EQ(get_decimal(v), dec("100"));
}

void test_eval_case_no_match_returns_null() {
    Context ctx;
    ctx.set("status", Value{std::string("unknown")});

    auto v = eval(
        "CASE WHEN status = 'active' THEN 1 WHEN status = 'pending' THEN 2 END",
        ctx);
    ASSERT_TRUE(is_null(v));
}

void test_eval_round() {
    auto v = eval("ROUND(3.14159, 2)");
    ASSERT_EQ(get_decimal(v), dec("3.14"));

    v = eval("ROUND(2.555, 2)");
    ASSERT_EQ(get_decimal(v), dec("2.56")); // ROUND_HALF_UP
}

void test_eval_coalesce() {
    auto v = eval("COALESCE(NULL, NULL, 42)");
    ASSERT_EQ(get_decimal(v), dec("42"));

    v = eval("COALESCE(NULL, 'hello')");
    ASSERT_EQ(get_string(v), "hello");

    v = eval("COALESCE(NULL, NULL)");
    ASSERT_TRUE(is_null(v));
}

void test_eval_upper_lower() {
    ASSERT_EQ(get_string(eval("UPPER('hello')")), "HELLO");
    ASSERT_EQ(get_string(eval("LOWER('HELLO')")), "hello");
}

void test_eval_abs() {
    Context ctx;
    ctx.set("x", Value{dec("-42.5")});
    ASSERT_EQ(get_decimal(eval("ABS(x)", ctx)), dec("42.5"));
}

void test_eval_concat() {
    Context ctx;
    ctx.set("first", Value{std::string("Hello")});
    ctx.set("last", Value{std::string("World")});

    auto v = eval("CONCAT(first, ' ', last)", ctx);
    ASSERT_EQ(get_string(v), "Hello World");
}

void test_eval_in() {
    Context ctx;
    ctx.set("status", Value{std::string("active")});

    ASSERT_EQ(get_bool(eval("status IN ('active', 'pending')", ctx)), true);
    ASSERT_EQ(get_bool(eval("status IN ('deleted', 'archived')", ctx)), false);
}

void test_eval_not_in() {
    Context ctx;
    ctx.set("status", Value{std::string("active")});

    ASSERT_EQ(get_bool(eval("status NOT IN ('deleted')", ctx)), true);
}

void test_eval_like() {
    Context ctx;
    ctx.set("name", Value{std::string("Hello World")});

    ASSERT_EQ(get_bool(eval("name LIKE '%world%'", ctx)), true); // case-insensitive
    ASSERT_EQ(get_bool(eval("name LIKE 'hello%'", ctx)), true);
    ASSERT_EQ(get_bool(eval("name LIKE '%xyz%'", ctx)), false);
}

void test_eval_not_like() {
    Context ctx;
    ctx.set("name", Value{std::string("Hello")});

    ASSERT_EQ(get_bool(eval("name NOT LIKE '%xyz%'", ctx)), true);
}

void test_eval_dot_path() {
    Context ctx;
    Context nested;
    nested.set("rate", Value{dec("5.00")});
    ctx.set_nested("config", std::move(nested));

    auto v = eval("config.rate", ctx);
    ASSERT_EQ(get_decimal(v), dec("5.00"));
}

void test_eval_nested_dot_path() {
    Context ctx;
    Context pricing;
    pricing.set("margin", Value{dec("0.15")});
    Context config;
    config.set_nested("pricing", std::move(pricing));
    ctx.set_nested("config", std::move(config));

    auto v = eval("config.pricing.margin", ctx);
    ASSERT_EQ(get_decimal(v), dec("0.15"));
}

void test_eval_unknown_field() {
    ASSERT_THROWS(eval("nonexistent"));
}

void test_eval_complex_expression() {
    Context ctx;
    ctx.set("price", Value{dec("100.00")});
    ctx.set("quantity", Value{dec("5")});
    ctx.set("discount_rate", Value{dec("0.1")});

    auto v = eval("(price * quantity) + (price * quantity * discount_rate)", ctx);
    // 500 + 50 = 550
    ASSERT_EQ(get_decimal(v), dec("550.000"));
}

void test_eval_real_world_case() {
    Context ctx;
    ctx.set("currency", Value{std::string("BRL")});
    ctx.set("amount_local", Value{dec("500.00")});
    ctx.set("amount_usd", Value{dec("100.00")});

    auto v = eval(
        "CASE "
        "WHEN currency = 'USD' THEN amount_usd "
        "WHEN currency = 'BRL' THEN amount_local "
        "ELSE NULL "
        "END",
        ctx);
    ASSERT_EQ(get_decimal(v), dec("500.00"));
}

void test_eval_coalesce_with_field() {
    Context ctx;
    ctx.set("base_amount", Value{NullValue{}});
    ctx.set("rate", Value{dec("1.5")});

    auto v = eval("ROUND(COALESCE(base_amount, 0) * COALESCE(rate, 1), 4)", ctx);
    ASSERT_EQ(get_decimal(v), dec("0.0000"));
}

void test_eval_resolver() {
    Context ctx;
    ctx.set("amount", Value{dec("100")});

    EvalOptions opts;
    opts.resolver = [](const std::string& name,
                       const std::set<std::string>&) -> Value {
        if (name == "external_rate")
            return Value{dec("1.5")};
        throw std::runtime_error("Unknown field: " + name);
    };

    auto ast = parse("amount * external_rate");
    auto v = evaluate(*ast, ctx, opts);
    ASSERT_EQ(get_decimal(v), dec("150.0"));
}

void test_eval_unary_minus() {
    Context ctx;
    ctx.set("x", Value{dec("100.00")});

    ASSERT_EQ(get_decimal(eval("-5", ctx)), dec("-5"));
    ASSERT_EQ(get_decimal(eval("-x", ctx)), dec("-100.00"));
    ASSERT_EQ(get_decimal(eval("-(1 + 2)", ctx)), dec("-3"));
    ASSERT_EQ(get_decimal(eval("- -5", ctx)), dec("5"));
    ASSERT_EQ(get_decimal(eval("5 - - 2", ctx)), dec("7"));
}

void test_eval_unary_minus_null() {
    Context ctx;
    ctx.set("n", Value{NullValue{}});
    ASSERT_TRUE(is_null(eval("-n", ctx)));
    ASSERT_TRUE(is_null(eval("-NULL", ctx)));
}

void test_eval_unary_minus_nonnumeric() {
    ASSERT_THROWS(eval("-'abc'"));
}

void test_eval_arithmetic_null() {
    Context ctx;
    ctx.set("n", Value{NullValue{}});
    ctx.set("x", Value{dec("10")});

    ASSERT_TRUE(is_null(eval("n + 1", ctx)));
    ASSERT_TRUE(is_null(eval("1 + n", ctx)));
    ASSERT_TRUE(is_null(eval("n - 1", ctx)));
    ASSERT_TRUE(is_null(eval("n * 2", ctx)));
    ASSERT_TRUE(is_null(eval("n / 2", ctx)));
    ASSERT_TRUE(is_null(eval("x * n", ctx)));
}

void test_eval_round_null() {
    Context ctx;
    ctx.set("n", Value{NullValue{}});
    ctx.set("x", Value{dec("3.14159")});

    ASSERT_TRUE(is_null(eval("ROUND(n, 2)", ctx)));
    ASSERT_TRUE(is_null(eval("ROUND(x, n)", ctx)));
}

void test_eval_abs_null() {
    Context ctx;
    ctx.set("n", Value{NullValue{}});
    ASSERT_TRUE(is_null(eval("ABS(n)", ctx)));
}

void test_eval_least_greatest() {
    Context ctx;
    ctx.set("x", Value{dec("100.00")});
    ctx.set("y", Value{dec("20.00")});
    ctx.set("n", Value{NullValue{}});

    ASSERT_EQ(get_decimal(eval("LEAST(3, 1, 2)", ctx)), dec("1"));
    ASSERT_EQ(get_decimal(eval("GREATEST(3, 1, 2)", ctx)), dec("3"));
    ASSERT_EQ(get_decimal(eval("LEAST(x, y)", ctx)), dec("20.00"));
    ASSERT_EQ(get_decimal(eval("LEAST(7)", ctx)), dec("7"));
    ASSERT_TRUE(is_null(eval("LEAST(x, n)", ctx)));
    ASSERT_TRUE(is_null(eval("GREATEST(1, n)", ctx)));
}

void test_eval_least_greatest_strings() {
    ASSERT_EQ(get_string(eval("LEAST('banana', 'apple', 'cherry')")), "apple");
    ASSERT_EQ(get_string(eval("GREATEST('banana', 'apple', 'cherry')")), "cherry");
}

void test_eval_least_first_on_tie() {
    ASSERT_EQ(get_decimal(eval("LEAST(1, 1.0)")), dec("1"));
}

void test_eval_least_greatest_no_args() {
    ASSERT_THROWS(eval("LEAST()"));
    ASSERT_THROWS(eval("GREATEST()"));
}

void test_eval_question_mark_identifier() {
    Context ctx;
    ctx.set("eligible?", Value{true});
    ASSERT_EQ(get_bool(eval("SELECT eligible?", ctx)), true);
}

void test_eval_negative_in_list() {
    Context ctx;
    ctx.set("balance", Value{dec("-42")});
    ASSERT_EQ(get_bool(eval("balance IN (-42, 0)", ctx)), true);
    ASSERT_EQ(get_bool(eval("balance IN (-41, 0)", ctx)), false);
}

void test_eval_resolver_precedence() {
    Context ctx;
    ctx.set("amount", Value{dec("100")});

    EvalOptions opts;
    opts.resolver = [](const std::string&,
                       const std::set<std::string>&) -> Value {
        return Value{dec("999")};
    };

    auto ast = parse("amount");
    auto v = evaluate(*ast, ctx, opts);
    ASSERT_EQ(get_decimal(v), dec("100"));
}

void test_eval_resolver_visited() {
    Context ctx;
    EvalOptions opts;
    bool got_visited = false;
    opts.resolver = [&](const std::string& name,
                        const std::set<std::string>& visited) -> Value {
        got_visited = visited.count(name) == 0;
        return Value{dec("1")};
    };

    auto ast = parse("missing");
    evaluate(*ast, ctx, opts);
    ASSERT_TRUE(got_visited);
}

void test_eval_resolver_circular() {
    Context ctx;
    EvalOptions opts;
    opts.resolver = [&](const std::string& name,
                        const std::set<std::string>& visited) -> Value {
        EvalOptions inner = opts;
        inner.visited = visited;
        inner.visited.insert(name);
        auto sub = parse("loop");
        return evaluate(*sub, ctx, inner);
    };

    auto ast = parse("loop");
    ASSERT_THROWS(evaluate(*ast, ctx, opts));
}

void test_eval_event_two_arg() {
    Context ctx;
    ctx.set("click", Value{dec("1")});
    ctx.set("purchase", Value{dec("2")});

    EvalOptions opts;
    opts.event_resolver = [](const std::string& t, const std::string& s,
                             const Context&,
                             const std::set<std::string>& visited) -> Value {
        std::string key = t + "." + s;
        return Value{std::string(t + ":" + s + ":" +
                     std::to_string(visited.count(key)))};
    };

    auto ast = parse("EVENT(click, purchase)");
    auto v = evaluate(*ast, ctx, opts);
    ASSERT_EQ(get_string(v), "click:purchase:1");
}

void test_eval_event_three_arg_nested() {
    Context ctx;
    Context order;
    order.set("base", Value{dec("7")});
    ctx.set_nested("order", order);

    EvalOptions opts;
    opts.event_resolver = [](const std::string&, const std::string&,
                             const Context& c,
                             const std::set<std::string>&) -> Value {
        auto it = c.fields.find("base");
        if (it != c.fields.end()) return it->second;
        return Value{dec("0")};
    };

    auto ast = parse("EVENT(t, s, order)");
    auto v = evaluate(*ast, ctx, opts);
    ASSERT_EQ(get_decimal(v), dec("7"));
}

void test_eval_event_list() {
    Context ctx;
    std::vector<Context> items(3);
    ctx.set_list("lines", items);

    EvalOptions opts;
    opts.event_resolver = [](const std::string&, const std::string&,
                             const Context&,
                             const std::set<std::string>&) -> Value {
        return Value{dec("10")};
    };

    auto ast = parse("EVENT(t, s, lines)");
    auto v = evaluate(*ast, ctx, opts);
    ASSERT_EQ(get_decimal(v), dec("30"));
}

void test_eval_event_empty_list() {
    Context ctx;
    ctx.set_list("lines", {});

    EvalOptions opts;
    opts.event_resolver = [](const std::string&, const std::string&,
                             const Context&,
                             const std::set<std::string>&) -> Value {
        return Value{dec("10")};
    };

    auto ast = parse("EVENT(t, s, lines)");
    auto v = evaluate(*ast, ctx, opts);
    ASSERT_EQ(get_decimal(v), dec("0"));
}

void test_eval_event_errors() {
    Context ctx;
    ctx.set("plain", Value{dec("5")});

    EvalOptions opts;
    opts.event_resolver = [](const std::string&, const std::string&,
                             const Context&,
                             const std::set<std::string>&) -> Value {
        return Value{dec("0")};
    };

    ASSERT_THROWS(evaluate(*parse("EVENT(t, s, missing)"), ctx, opts));
    ASSERT_THROWS(evaluate(*parse("EVENT(t, s, plain)"), ctx, opts));
    ASSERT_THROWS(evaluate(*parse("EVENT(t)"), ctx, opts));
    ASSERT_THROWS(evaluate(*parse("EVENT(t, s, x, y)"), ctx, opts));
    ASSERT_THROWS(evaluate(*parse("EVENT('t', s)"), ctx, opts));
    ASSERT_THROWS(evaluate(*parse("EVENT(t, 1)"), ctx, opts));
}

void test_eval_event_no_resolver() {
    Context ctx;
    ASSERT_THROWS(eval("EVENT(a, b)", ctx));
}

void test_eval_event_cycle() {
    Context ctx;
    EvalOptions opts;
    opts.event_resolver = [&](const std::string&, const std::string&,
                              const Context& c,
                              const std::set<std::string>& visited) -> Value {
        EvalOptions inner = opts;
        inner.visited = visited;
        auto sub = parse("EVENT(t, s)");
        return evaluate(*sub, c, inner);
    };

    auto ast = parse("EVENT(t, s)");
    ASSERT_THROWS(evaluate(*ast, ctx, opts));
}

void test_eval_dotpath_list_numeric() {
    Context ctx;
    Context i1, i2;
    i1.set("amt", Value{dec("10.5")});
    i2.set("amt", Value{std::string("4.5")});
    ctx.set_list("lines", {i1, i2});

    ASSERT_EQ(get_decimal(eval("lines.amt", ctx)), dec("15.0"));

    Context empty_ctx;
    empty_ctx.set_list("lines", {});
    ASSERT_EQ(get_decimal(eval("lines.amt", empty_ctx)), dec("0"));
}

void test_eval_dotpath_list_nonnumeric() {
    Context ctx;
    Context i1, i2;
    i1.set("name", Value{std::string("a")});
    i2.set("name", Value{std::string("b")});
    ctx.set_list("lines", {i1, i2});

    auto v = eval("lines.name", ctx);
    auto* l = std::get_if<std::shared_ptr<ValueList>>(&v);
    ASSERT_TRUE(l != nullptr && *l != nullptr);
    ASSERT_EQ((*l)->items.size(), static_cast<size_t>(2));
    ASSERT_EQ(get_string((*l)->items[0]), "a");
}

void test_eval_nested_map_value() {
    Context ctx;
    Context order;
    order.set("base", Value{dec("7")});
    ctx.set_nested("order", order);

    auto v = eval("order", ctx);
    auto* m = std::get_if<std::shared_ptr<Context>>(&v);
    ASSERT_TRUE(m != nullptr && *m != nullptr);
    ASSERT_EQ(get_decimal((*m)->fields.at("base")), dec("7"));
}

void test_eval_least_greatest_numeric_strings() {
    ASSERT_EQ(get_string(eval("LEAST('2', '10')")), "10");
    ASSERT_EQ(get_string(eval("GREATEST('2', '10')")), "2");
}

void test_eval_least_greatest_temporal() {
    Context ctx;
    ctx.set_date("d1", Date{2024, 1, 1});
    ctx.set_date("d2", Date{2024, 6, 15});
    ASSERT_EQ(value_to_string(eval("LEAST(d1, d2)", ctx)), "2024-01-01");
    ASSERT_EQ(value_to_string(eval("GREATEST(d1, d2)", ctx)), "2024-06-15");

    Context ctx2;
    ctx2.set_datetime("t1", DateTime{2024, 1, 1, 0, 0, 0});
    ctx2.set_datetime("t2", DateTime{2024, 1, 1, 12, 30, 0});
    ASSERT_EQ(value_to_string(eval("GREATEST(t1, t2)", ctx2)),
              "2024-01-01T12:30:00Z");

    Context ctx3;
    ctx3.set_naive_datetime("n1", NaiveDateTime{2024, 1, 1, 0, 0, 0});
    ctx3.set_naive_datetime("n2", NaiveDateTime{2023, 12, 31, 23, 59, 59});
    ASSERT_EQ(value_to_string(eval("LEAST(n1, n2)", ctx3)),
              "2023-12-31 23:59:59");

    Context ctx4;
    ctx4.set_time("h1", Time{9, 0, 0});
    ctx4.set_time("h2", Time{18, 30, 0});
    ASSERT_EQ(value_to_string(eval("GREATEST(h1, h2)", ctx4)), "18:30:00");
}

void test_eval_round_missing_precision() {
    ASSERT_THROWS(eval("ROUND(NULL, missing_prec)"));
}

// ===== C API TESTS ===========================================================

#include "dsqlex/dsqlex.h"

void test_c_api_parse_eval() {
    auto* ast = dsqlex_parse("(10 + 20) * 3");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    auto* result = dsqlex_eval(ast, ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_DECIMAL);

    auto* dec_str = dsqlex_result_decimal(result);
    ASSERT_TRUE(dec_str != nullptr);
    ASSERT_EQ(std::string(dec_str), "90");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

void test_c_api_context() {
    auto* ast = dsqlex_parse("CASE WHEN active THEN amount ELSE 0 END");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    dsqlex_context_set_bool(ctx, "active", true);
    dsqlex_context_set_decimal(ctx, "amount", "500.00");

    auto* result = dsqlex_eval(ast, ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_DECIMAL);
    ASSERT_EQ(std::string(dsqlex_result_decimal(result)), "500.00");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

void test_c_api_error() {
    auto* ast = dsqlex_parse("'unterminated string");
    ASSERT_TRUE(ast == nullptr);
    ASSERT_TRUE(dsqlex_last_error() != nullptr);
}

void test_c_api_string_result() {
    auto* ast = dsqlex_parse("UPPER('hello')");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    auto* result = dsqlex_eval(ast, ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_STRING);
    ASSERT_EQ(std::string(dsqlex_result_string(result)), "HELLO");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

void test_c_api_eval_string_convenience() {
    auto* ctx = dsqlex_context_new();
    dsqlex_context_set_decimal(ctx, "x", "7");

    auto* result = dsqlex_eval_string("x * x", ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(std::string(dsqlex_result_decimal(result)), "49");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
}

void test_c_api_date_value() {
    auto* ast = dsqlex_parse("d");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    dsqlex_context_set_date(ctx, "d", 2024, 3, 5);

    auto* result = dsqlex_eval(ast, ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_DATE);
    ASSERT_EQ(std::string(dsqlex_result_string(result)), "2024-03-05");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

void test_c_api_list_sum() {
    auto* ast = dsqlex_parse("items.amt");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    auto* a = dsqlex_context_list_add(ctx, "items");
    dsqlex_context_set_decimal(a, "amt", "2.5");
    auto* b = dsqlex_context_list_add(ctx, "items");
    dsqlex_context_set_decimal(b, "amt", "3.5");

    auto* result = dsqlex_eval(ast, ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_DECIMAL);
    ASSERT_EQ(std::string(dsqlex_result_decimal(result)), "6.0");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

void test_c_api_empty_list() {
    auto* ast = dsqlex_parse("items.amt");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    dsqlex_context_set_empty_list(ctx, "items");

    auto* result = dsqlex_eval(ast, ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_DECIMAL);
    ASSERT_EQ(std::string(dsqlex_result_decimal(result)), "0");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

void test_c_api_map_result() {
    auto* ast = dsqlex_parse("order");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    auto* order = dsqlex_context_set_nested(ctx, "order");
    dsqlex_context_set_decimal(order, "base", "7");

    auto* result = dsqlex_eval(ast, ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_MAP);
    ASSERT_TRUE(dsqlex_result_string(result) == nullptr);

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

static dsqlex_result* test_event_cb(const char*, const char*,
                                    const dsqlex_context*, void*) {
    return dsqlex_result_new_decimal("7");
}

void test_c_api_event_empty_list() {
    auto* ast = dsqlex_parse("EVENT(t, s, lines)");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    dsqlex_context_set_empty_list(ctx, "lines");

    dsqlex_eval_options opts{nullptr, test_event_cb, nullptr};
    auto* result = dsqlex_eval_with_options(ast, ctx, &opts);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_DECIMAL);
    ASSERT_EQ(std::string(dsqlex_result_decimal(result)), "0");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

void test_c_api_time_value() {
    auto* ast = dsqlex_parse("GREATEST(h1, h2)");
    ASSERT_TRUE(ast != nullptr);

    auto* ctx = dsqlex_context_new();
    dsqlex_context_set_time(ctx, "h1", 9, 0, 0);
    dsqlex_context_set_time(ctx, "h2", 18, 30, 0);

    auto* result = dsqlex_eval(ast, ctx);
    ASSERT_TRUE(result != nullptr);
    ASSERT_EQ(dsqlex_result_type(result), DSQLEX_TYPE_TIME);
    ASSERT_EQ(std::string(dsqlex_result_string(result)), "18:30:00");

    dsqlex_result_free(result);
    dsqlex_context_free(ctx);
    dsqlex_ast_free(ast);
}

// ===== MAIN ==================================================================

int main() {
    std::cout << "\n=== DSQLEX C++ Test Suite ===\n\n";

    std::cout << "-- Lexer --\n";
    TEST(lexer_simple_tokens);
    TEST(lexer_keywords);
    TEST(lexer_case_insensitive);
    TEST(lexer_numbers);
    TEST(lexer_strings);
    TEST(lexer_identifiers);
    TEST(lexer_functions);
    TEST(lexer_comments);
    TEST(lexer_unterminated_string);
    TEST(lexer_unterminated_block_comment);

    std::cout << "\n-- Parser --\n";
    TEST(parser_simple_field);
    TEST(parser_select_optional);
    TEST(parser_arithmetic);
    TEST(parser_arithmetic_chaining);
    TEST(parser_mixed_arithmetic_rejected);
    TEST(parser_parenthesized_mixed);
    TEST(parser_case);
    TEST(parser_function);
    TEST(parser_in_expr);
    TEST(parser_not_in_expr);
    TEST(parser_like);
    TEST(parser_not_like);
    TEST(parser_is_null);
    TEST(parser_is_not_null);
    TEST(parser_mixed_logical_rejected);
    TEST(parser_same_logical_ok);
    TEST(parser_comparison_no_chain);

    std::cout << "\n-- Evaluator --\n";
    TEST(eval_number_literal);
    TEST(eval_string_literal);
    TEST(eval_bool_literals);
    TEST(eval_null_literal);
    TEST(eval_field_lookup);
    TEST(eval_arithmetic);
    TEST(eval_division);
    TEST(eval_comparison);
    TEST(eval_string_comparison);
    TEST(eval_null_comparison);
    TEST(eval_logical_and);
    TEST(eval_logical_or);
    TEST(eval_case_simple);
    TEST(eval_case_no_match_returns_null);
    TEST(eval_round);
    TEST(eval_coalesce);
    TEST(eval_upper_lower);
    TEST(eval_abs);
    TEST(eval_concat);
    TEST(eval_in);
    TEST(eval_not_in);
    TEST(eval_like);
    TEST(eval_not_like);
    TEST(eval_dot_path);
    TEST(eval_nested_dot_path);
    TEST(eval_unknown_field);
    TEST(eval_complex_expression);
    TEST(eval_real_world_case);
    TEST(eval_coalesce_with_field);
    TEST(eval_resolver);
    TEST(eval_unary_minus);
    TEST(eval_unary_minus_null);
    TEST(eval_unary_minus_nonnumeric);
    TEST(eval_arithmetic_null);
    TEST(eval_round_null);
    TEST(eval_abs_null);
    TEST(eval_least_greatest);
    TEST(eval_least_greatest_strings);
    TEST(eval_least_first_on_tie);
    TEST(eval_least_greatest_no_args);
    TEST(eval_question_mark_identifier);
    TEST(eval_negative_in_list);
    TEST(eval_resolver_precedence);
    TEST(eval_resolver_visited);
    TEST(eval_resolver_circular);
    TEST(eval_event_two_arg);
    TEST(eval_event_three_arg_nested);
    TEST(eval_event_list);
    TEST(eval_event_empty_list);
    TEST(eval_event_errors);
    TEST(eval_event_no_resolver);
    TEST(eval_event_cycle);
    TEST(eval_dotpath_list_numeric);
    TEST(eval_dotpath_list_nonnumeric);
    TEST(eval_nested_map_value);
    TEST(eval_least_greatest_numeric_strings);
    TEST(eval_least_greatest_temporal);
    TEST(eval_round_missing_precision);

    std::cout << "\n-- C API --\n";
    TEST(c_api_parse_eval);
    TEST(c_api_context);
    TEST(c_api_error);
    TEST(c_api_string_result);
    TEST(c_api_eval_string_convenience);
    TEST(c_api_date_value);
    TEST(c_api_list_sum);
    TEST(c_api_empty_list);
    TEST(c_api_map_result);
    TEST(c_api_event_empty_list);
    TEST(c_api_time_value);

    std::cout << "\n=== Results: " << tests_passed << "/" << tests_run
              << " passed";
    if (tests_failed > 0)
        std::cout << " (" << tests_failed << " FAILED)";
    std::cout << " ===\n\n";

    return tests_failed > 0 ? 1 : 0;
}
