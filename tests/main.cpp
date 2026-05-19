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

    std::cout << "\n-- C API --\n";
    TEST(c_api_parse_eval);
    TEST(c_api_context);
    TEST(c_api_error);
    TEST(c_api_string_result);
    TEST(c_api_eval_string_convenience);

    std::cout << "\n=== Results: " << tests_passed << "/" << tests_run
              << " passed";
    if (tests_failed > 0)
        std::cout << " (" << tests_failed << " FAILED)";
    std::cout << " ===\n\n";

    return tests_failed > 0 ? 1 : 0;
}
