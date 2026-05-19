#include "dsqlex/evaluator.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace dsqlex {

// ---- Context ----------------------------------------------------------------

void Context::set(const std::string& key, Value val) {
    fields[key] = std::move(val);
}

void Context::set_nested(const std::string& key, Context ctx) {
    nested[key] = std::move(ctx);
}

// ---- Helpers ----------------------------------------------------------------

namespace {

bool is_truthy(const Value& v) {
    if (is_null(v)) return false;
    if (auto* b = std::get_if<bool>(&v)) return *b;
    return true; // Elixir semantics: everything else is truthy
}

decimal::Decimal make_decimal(const std::string& s) {
    return decimal::Decimal(s);
}

// Convert a Value to Decimal. Throws if not convertible.
decimal::Decimal value_to_decimal(const Value& v) {
    if (auto* d = std::get_if<decimal::Decimal>(&v))
        return *d;
    if (auto* s = std::get_if<std::string>(&v))
        return make_decimal(*s);
    if (is_null(v))
        throw std::runtime_error("Cannot convert NULL to decimal");
    if (auto* b = std::get_if<bool>(&v))
        throw std::runtime_error("Cannot convert boolean to decimal");
    throw std::runtime_error("Cannot convert value to decimal");
}

// Convert a Value to string representation.
std::string val_to_string(const Value& v) {
    if (is_null(v)) return "NULL";
    if (auto* d = std::get_if<decimal::Decimal>(&v))
        return d->to_sci();
    if (auto* s = std::get_if<std::string>(&v))
        return *s;
    if (auto* b = std::get_if<bool>(&v))
        return *b ? "TRUE" : "FALSE";
    return "";
}

// Compare two values for ordering. Returns -1, 0, or 1.
// Returns 0 for equal, handles null comparisons.
int compare_values(const Value& lhs, const Value& rhs) {
    // null == null -> 0, null vs anything -> not equal
    if (is_null(lhs) && is_null(rhs)) return 0;
    if (is_null(lhs) || is_null(rhs)) return -2; // sentinel: not comparable

    // Both decimal
    if (std::holds_alternative<decimal::Decimal>(lhs) &&
        std::holds_alternative<decimal::Decimal>(rhs)) {
        auto& l = std::get<decimal::Decimal>(lhs);
        auto& r = std::get<decimal::Decimal>(rhs);
        if (l < r) return -1;
        if (l > r) return 1;
        return 0;
    }

    // Both string
    if (std::holds_alternative<std::string>(lhs) &&
        std::holds_alternative<std::string>(rhs)) {
        auto& l = std::get<std::string>(lhs);
        auto& r = std::get<std::string>(rhs);
        return l.compare(r);
    }

    // Both bool
    if (std::holds_alternative<bool>(lhs) &&
        std::holds_alternative<bool>(rhs)) {
        bool l = std::get<bool>(lhs);
        bool r = std::get<bool>(rhs);
        if (l == r) return 0;
        return l ? 1 : -1;
    }

    // Mixed: try decimal conversion
    try {
        auto ld = value_to_decimal(lhs);
        auto rd = value_to_decimal(rhs);
        if (ld < rd) return -1;
        if (ld > rd) return 1;
        return 0;
    } catch (...) {}

    // Fallback: string comparison
    auto ls = val_to_string(lhs);
    auto rs = val_to_string(rhs);
    return ls.compare(rs);
}

// SQL LIKE pattern matching (case-insensitive).
bool like_match(const std::string& text, const std::string& pattern) {
    // Convert SQL LIKE pattern to regex.
    // % -> .* , _ -> .
    std::string regex_str;
    regex_str.reserve(pattern.size() * 2);
    regex_str += '^';
    for (char c : pattern) {
        if (c == '%') {
            regex_str += ".*";
        } else if (c == '_') {
            regex_str += '.';
        } else if (std::string("\\^$.|?*+()[]{}").find(c) != std::string::npos) {
            regex_str += '\\';
            regex_str += c;
        } else {
            regex_str += c;
        }
    }
    regex_str += '$';

    std::regex re(regex_str, std::regex_constants::icase);
    return std::regex_match(text, re);
}

// Resolve a dot-path identifier from context.
Value resolve_identifier(const std::string& name, const Context& ctx,
                         const EvalOptions& opts) {
    // Check circular references
    if (opts.visited.count(name))
        throw std::runtime_error("Circular reference detected: " + name);

    // Simple lookup first
    auto it = ctx.fields.find(name);
    if (it != ctx.fields.end())
        return it->second;

    // Dot-path resolution
    auto dot_pos = name.find('.');
    if (dot_pos != std::string::npos) {
        std::string first_key = name.substr(0, dot_pos);
        std::string rest = name.substr(dot_pos + 1);

        auto nested_it = ctx.nested.find(first_key);
        if (nested_it != ctx.nested.end()) {
            // Recurse into nested context
            return resolve_identifier(rest, nested_it->second, opts);
        }

        // Check if the first part is in fields
        auto field_it = ctx.fields.find(first_key);
        if (field_it != ctx.fields.end()) {
            throw std::runtime_error(
                "Cannot access '" + rest + "' on non-map value in path '" + name + "'");
        }

        throw std::runtime_error(
            "Unknown field: " + name + " (failed at '" + first_key + "')");
    }

    // Try custom resolver
    if (opts.resolver) {
        auto new_visited = opts.visited;
        new_visited.insert(name);
        return opts.resolver(name, new_visited);
    }

    throw std::runtime_error("Unknown field: " + name);
}

// Forward declaration
Value eval_node(const ASTNode& node, const Context& ctx, const EvalOptions& opts);

// Evaluate a binary operation
Value eval_binary_op(BinOp op, const ASTNode& left_node, const ASTNode& right_node,
                     const Context& ctx, const EvalOptions& opts) {
    // Short-circuit for logical operators
    if (op == BinOp::And) {
        auto left = eval_node(left_node, ctx, opts);
        if (!is_truthy(left)) return left;
        return eval_node(right_node, ctx, opts);
    }
    if (op == BinOp::Or) {
        auto left = eval_node(left_node, ctx, opts);
        if (is_truthy(left)) return left;
        return eval_node(right_node, ctx, opts);
    }

    auto left = eval_node(left_node, ctx, opts);
    auto right = eval_node(right_node, ctx, opts);

    // Arithmetic
    switch (op) {
        case BinOp::Plus:
            return Value{value_to_decimal(left) + value_to_decimal(right)};
        case BinOp::Minus:
            return Value{value_to_decimal(left) - value_to_decimal(right)};
        case BinOp::Multiply:
            return Value{value_to_decimal(left) * value_to_decimal(right)};
        case BinOp::Divide:
            return Value{value_to_decimal(left) / value_to_decimal(right)};
        default: break;
    }

    // Comparison
    int cmp = compare_values(left, right);

    switch (op) {
        case BinOp::Eq:  return Value{cmp == 0};
        case BinOp::Neq: return Value{cmp != 0};
        case BinOp::Lt:  return Value{cmp == -2 ? false : cmp < 0};
        case BinOp::Gt:  return Value{cmp == -2 ? false : cmp > 0};
        case BinOp::Lte: return Value{cmp == -2 ? false : cmp <= 0};
        case BinOp::Gte: return Value{cmp == -2 ? false : cmp >= 0};
        default: break;
    }

    throw std::runtime_error("Unknown binary operator");
}

// Evaluate a CASE expression
Value eval_case(const ASTNode& node, const Context& ctx, const EvalOptions& opts) {
    for (auto& w : node.whens) {
        auto cond = eval_node(*w.condition, ctx, opts);
        if (is_truthy(cond))
            return eval_node(*w.result, ctx, opts);
    }
    if (node.else_clause)
        return eval_node(*node.else_clause, ctx, opts);
    return Value{NullValue{}};
}

// Evaluate a function call
Value eval_function(const ASTNode& node, const Context& ctx, const EvalOptions& opts) {
    const auto& name = node.func_name;
    auto& args = node.items;

    if (name == "ROUND") {
        if (args.size() != 2)
            throw std::runtime_error("ROUND requires 2 arguments");
        auto val = eval_node(*args[0], ctx, opts);
        auto prec = eval_node(*args[1], ctx, opts);
        if (is_null(val)) return Value{NullValue{}};

        auto d = value_to_decimal(val);
        auto p = value_to_decimal(prec);

        // quantize to 10^(-precision)
        // Build "1E-<precision>" string
        int prec_int = std::stoi(p.to_sci());
        std::string quant_str = "1E-" + std::to_string(prec_int);

        decimal::Context round_ctx;
        round_ctx.round(decimal::ROUND_HALF_UP);
        return Value{d.quantize(decimal::Decimal(quant_str), round_ctx)};
    }

    if (name == "COALESCE") {
        for (auto& arg : args) {
            auto val = eval_node(*arg, ctx, opts);
            if (!is_null(val)) return val;
        }
        return Value{NullValue{}};
    }

    if (name == "UPPER") {
        if (args.size() != 1)
            throw std::runtime_error("UPPER requires 1 argument");
        auto val = eval_node(*args[0], ctx, opts);
        if (is_null(val)) return Value{NullValue{}};
        auto s = val_to_string(val);
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return Value{std::move(s)};
    }

    if (name == "LOWER") {
        if (args.size() != 1)
            throw std::runtime_error("LOWER requires 1 argument");
        auto val = eval_node(*args[0], ctx, opts);
        if (is_null(val)) return Value{NullValue{}};
        auto s = val_to_string(val);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return Value{std::move(s)};
    }

    if (name == "ABS") {
        if (args.size() != 1)
            throw std::runtime_error("ABS requires 1 argument");
        auto val = eval_node(*args[0], ctx, opts);
        if (is_null(val)) return Value{NullValue{}};
        auto d = value_to_decimal(val);
        return Value{d.copy_abs()};
    }

    if (name == "CONCAT") {
        std::string result;
        for (auto& arg : args) {
            auto val = eval_node(*arg, ctx, opts);
            result += val_to_string(val);
        }
        return Value{std::move(result)};
    }

    if (name == "EVENT") {
        if (args.size() < 2 || args.size() > 3)
            throw std::runtime_error(
                "EVENT requires 2 or 3 arguments: EVENT(type, subtype) "
                "or EVENT(type, subtype, context_source)");

        if (!opts.event_resolver)
            throw std::runtime_error("EVENT() calls require an :event_resolver option");

        auto type_val = eval_node(*args[0], ctx, opts);
        auto subtype_val = eval_node(*args[1], ctx, opts);

        auto type_str = val_to_string(type_val);
        auto subtype_str = val_to_string(subtype_val);

        // Check circular reference
        auto event_key = type_str + "." + subtype_str;
        if (opts.visited.count(event_key))
            throw std::runtime_error("Circular reference detected: " + event_key);

        auto new_opts = opts;
        new_opts.visited.insert(event_key);

        if (args.size() == 2) {
            return opts.event_resolver(type_str, subtype_str, ctx, new_opts.visited);
        }

        // 3-arg form: EVENT(type, subtype, context_source)
        auto source_val = eval_node(*args[2], ctx, opts);
        auto source_name = val_to_string(source_val);

        // Look up the source in context nested
        auto nested_it = ctx.nested.find(source_name);
        if (nested_it == ctx.nested.end()) {
            // Check fields
            auto field_it = ctx.fields.find(source_name);
            if (field_it == ctx.fields.end())
                throw std::runtime_error(
                    "EVENT context source '" + source_name + "' not found in context");
            throw std::runtime_error(
                "EVENT context source '" + source_name + "' must be a map or list of maps");
        }

        return opts.event_resolver(type_str, subtype_str, nested_it->second, new_opts.visited);
    }

    throw std::runtime_error("Unknown function: " + name);
}

// Main evaluation dispatch
Value eval_node(const ASTNode& node, const Context& ctx, const EvalOptions& opts) {
    switch (node.kind) {
        case NodeKind::Select:
            return eval_node(*node.expr, ctx, opts);

        case NodeKind::NumberLit:
            return Value{make_decimal(node.text)};

        case NodeKind::StringLit:
            return Value{node.text};

        case NodeKind::BoolLit:
            return Value{node.bool_val};

        case NodeKind::NullLit:
            return Value{NullValue{}};

        case NodeKind::Identifier:
            return resolve_identifier(node.text, ctx, opts);

        case NodeKind::BinaryOp:
            return eval_binary_op(node.op, *node.left, *node.right, ctx, opts);

        case NodeKind::CaseExpr:
            return eval_case(node, ctx, opts);

        case NodeKind::FunctionCall:
            return eval_function(node, ctx, opts);

        case NodeKind::InExpr:
        case NodeKind::NotInExpr: {
            auto subject = eval_node(*node.expr, ctx, opts);
            bool found = false;
            for (auto& item : node.items) {
                auto item_val = eval_node(*item, ctx, opts);
                if (compare_values(subject, item_val) == 0) {
                    found = true;
                    break;
                }
            }
            if (node.kind == NodeKind::NotInExpr) found = !found;
            return Value{found};
        }

        case NodeKind::LikeExpr:
        case NodeKind::NotLikeExpr: {
            auto subject = eval_node(*node.expr, ctx, opts);
            auto pattern = eval_node(*node.right, ctx, opts);
            if (is_null(subject) || is_null(pattern))
                return Value{NullValue{}};
            bool matched = like_match(val_to_string(subject), val_to_string(pattern));
            if (node.kind == NodeKind::NotLikeExpr) matched = !matched;
            return Value{matched};
        }
    }

    throw std::runtime_error("Unknown AST node kind");
}

} // anonymous namespace

// ---- Public API -------------------------------------------------------------

Value evaluate(const ASTNode& ast, const Context& ctx, EvalOptions opts) {
    return eval_node(ast, ctx, opts);
}

std::string value_to_string(const Value& v) {
    return val_to_string(v);
}

decimal::Decimal to_decimal(const Value& v) {
    return value_to_decimal(v);
}

} // namespace dsqlex
