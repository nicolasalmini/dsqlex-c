#include "dsqlex/evaluator.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
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

void Context::set_list(const std::string& key, std::vector<Context> items) {
    lists[key] = std::move(items);
}

void Context::set_date(const std::string& key, Date val) {
    fields[key] = Value{val};
}

void Context::set_datetime(const std::string& key, DateTime val) {
    fields[key] = Value{val};
}

void Context::set_naive_datetime(const std::string& key, NaiveDateTime val) {
    fields[key] = Value{val};
}

void Context::set_time(const std::string& key, Time val) {
    fields[key] = Value{val};
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
    if (auto* dt = std::get_if<Date>(&v)) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", dt->year, dt->month, dt->day);
        return buf;
    }
    if (auto* dt = std::get_if<DateTime>(&v)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                      dt->year, dt->month, dt->day, dt->hour, dt->minute, dt->second);
        return buf;
    }
    if (auto* dt = std::get_if<NaiveDateTime>(&v)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                      dt->year, dt->month, dt->day, dt->hour, dt->minute, dt->second);
        return buf;
    }
    if (auto* t = std::get_if<Time>(&v)) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->hour, t->minute, t->second);
        return buf;
    }
    if (auto* l = std::get_if<std::shared_ptr<ValueList>>(&v)) {
        std::string out;
        for (size_t i = 0; i < (*l)->items.size(); ++i) {
            if (i) out += ',';
            out += val_to_string((*l)->items[i]);
        }
        return out;
    }
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
        int c = l.compare(r);
        return c < 0 ? -1 : c > 0 ? 1 : 0;
    }

    if (auto* a = std::get_if<Date>(&lhs)) {
        if (auto* b = std::get_if<Date>(&rhs)) {
            auto lt = std::tie(a->year, a->month, a->day);
            auto rt = std::tie(b->year, b->month, b->day);
            return lt < rt ? -1 : lt > rt ? 1 : 0;
        }
    }
    if (auto* a = std::get_if<DateTime>(&lhs)) {
        if (auto* b = std::get_if<DateTime>(&rhs)) {
            auto lt = std::tie(a->year, a->month, a->day, a->hour, a->minute, a->second);
            auto rt = std::tie(b->year, b->month, b->day, b->hour, b->minute, b->second);
            return lt < rt ? -1 : lt > rt ? 1 : 0;
        }
    }
    if (auto* a = std::get_if<NaiveDateTime>(&lhs)) {
        if (auto* b = std::get_if<NaiveDateTime>(&rhs)) {
            auto lt = std::tie(a->year, a->month, a->day, a->hour, a->minute, a->second);
            auto rt = std::tie(b->year, b->month, b->day, b->hour, b->minute, b->second);
            return lt < rt ? -1 : lt > rt ? 1 : 0;
        }
    }
    if (auto* a = std::get_if<Time>(&lhs)) {
        if (auto* b = std::get_if<Time>(&rhs)) {
            auto lt = std::tie(a->hour, a->minute, a->second);
            auto rt = std::tie(b->hour, b->minute, b->second);
            return lt < rt ? -1 : lt > rt ? 1 : 0;
        }
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
    int fc = ls.compare(rs);
    return fc < 0 ? -1 : fc > 0 ? 1 : 0;
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

struct PathAcc {
    const Context* ctx = nullptr;
    const Value* val = nullptr;
    const std::vector<Context>* list = nullptr;
};

bool is_decimal_like(const Value& v) {
    if (std::holds_alternative<decimal::Decimal>(v)) return true;
    if (auto* s = std::get_if<std::string>(&v)) {
        try {
            make_decimal(*s);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

Value resolve_dot_path(const std::vector<std::string>& parts, size_t idx,
                       PathAcc acc, const std::string& path,
                       const EvalOptions& opts) {
    if (idx >= parts.size()) {
        if (acc.val) return *acc.val;
        if (acc.ctx)
            return Value{std::make_shared<Context>(*acc.ctx)};
        if (acc.list) {
            ValueList out;
            for (auto& item : *acc.list)
                out.items.push_back(Value{std::make_shared<Context>(item)});
            return Value{std::make_shared<ValueList>(std::move(out))};
        }
        throw std::runtime_error("Cannot access non-value at path '" + path + "'");
    }

    if (acc.list) {
        std::vector<Value> results;
        for (auto& item : *acc.list) {
            PathAcc item_acc;
            item_acc.ctx = &item;
            results.push_back(resolve_dot_path(parts, idx, item_acc, path, opts));
        }
        bool all_numeric = true;
        for (auto& r : results) {
            if (!is_decimal_like(r)) {
                all_numeric = false;
                break;
            }
        }
        if (!all_numeric)
            return Value{std::make_shared<ValueList>(ValueList{std::move(results)})};
        decimal::Decimal sum("0");
        for (auto& r : results)
            sum += value_to_decimal(r);
        return Value{sum};
    }

    if (acc.ctx) {
        const std::string& key = parts[idx];
        auto fit = acc.ctx->fields.find(key);
        if (fit != acc.ctx->fields.end()) {
            PathAcc next;
            next.val = &fit->second;
            return resolve_dot_path(parts, idx + 1, next, path, opts);
        }
        auto nit = acc.ctx->nested.find(key);
        if (nit != acc.ctx->nested.end()) {
            PathAcc next;
            next.ctx = &nit->second;
            return resolve_dot_path(parts, idx + 1, next, path, opts);
        }
        auto lit = acc.ctx->lists.find(key);
        if (lit != acc.ctx->lists.end()) {
            PathAcc next;
            next.list = &lit->second;
            return resolve_dot_path(parts, idx + 1, next, path, opts);
        }
        throw std::runtime_error(
            "Unknown field: " + path + " (failed at '" + key + "')");
    }

    throw std::runtime_error(
        "Cannot access '" + parts[idx] + "' on non-map value in path '" + path + "'");
}

// Resolve a dot-path identifier from context.
Value resolve_identifier(const std::string& name, const Context& ctx,
                         const EvalOptions& opts) {
    // Simple lookup first
    auto it = ctx.fields.find(name);
    if (it != ctx.fields.end())
        return it->second;

    // Dot-path resolution
    if (name.find('.') != std::string::npos) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            auto dot = name.find('.', start);
            if (dot == std::string::npos) {
                parts.push_back(name.substr(start));
                break;
            }
            parts.push_back(name.substr(start, dot - start));
            start = dot + 1;
        }
        PathAcc acc;
        acc.ctx = &ctx;
        return resolve_dot_path(parts, 0, acc, name, opts);
    }

    auto nit = ctx.nested.find(name);
    if (nit != ctx.nested.end())
        return Value{std::make_shared<Context>(nit->second)};
    auto lit = ctx.lists.find(name);
    if (lit != ctx.lists.end()) {
        ValueList out;
        for (auto& item : lit->second)
            out.items.push_back(Value{std::make_shared<Context>(item)});
        return Value{std::make_shared<ValueList>(std::move(out))};
    }

    // Try custom resolver
    if (opts.resolver) {
        if (opts.visited.count(name))
            throw std::runtime_error("Circular reference detected: " + name);
        return opts.resolver(name, opts.visited);
    }

    throw std::runtime_error("Unknown field: " + name);
}

// Forward declaration
Value eval_node(const ASTNode& node, const Context& ctx, const EvalOptions& opts);
Value resolve_event(const std::string& type_str, const std::string& subtype_str,
                    const Context& ctx, const EvalOptions& opts);

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
        case BinOp::Minus:
        case BinOp::Multiply:
        case BinOp::Divide:
            if (is_null(left) || is_null(right))
                return Value{NullValue{}};
            break;
        default: break;
    }
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
        if (is_null(val) || is_null(prec)) return Value{NullValue{}};

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

    if (name == "LEAST" || name == "GREATEST") {
        if (args.empty())
            throw std::runtime_error("LEAST/GREATEST requires at least one argument");

        std::vector<Value> vals;
        vals.reserve(args.size());
        bool has_null = false;
        for (auto& arg : args) {
            auto v = eval_node(*arg, ctx, opts);
            if (is_null(v)) has_null = true;
            vals.push_back(std::move(v));
        }
        if (has_null) return Value{NullValue{}};

        int target = (name == "LEAST") ? -1 : 1;
        const Value* best = &vals[0];
        for (size_t i = 1; i < vals.size(); ++i) {
            int c = compare_values(vals[i], *best);
            if (c == -2) continue;
            if (c == target)
                best = &vals[i];
        }
        return *best;
    }

    if (name == "EVENT") {
        bool valid = args.size() == 2 || args.size() == 3;
        if (valid) {
            for (auto& a : args) {
                if (a->kind != NodeKind::Identifier) {
                    valid = false;
                    break;
                }
            }
        }
        if (!valid)
            throw std::runtime_error(
                "EVENT requires 2 or 3 arguments: EVENT(type, subtype) "
                "or EVENT(type, subtype, context_source)");

        const std::string& type_str = args[0]->text;
        const std::string& subtype_str = args[1]->text;

        if (args.size() == 2)
            return resolve_event(type_str, subtype_str, ctx, opts);

        const std::string& source = args[2]->text;
        auto list_it = ctx.lists.find(source);
        if (list_it != ctx.lists.end()) {
            decimal::Decimal sum("0");
            for (auto& item : list_it->second) {
                auto v = resolve_event(type_str, subtype_str, item, opts);
                sum += value_to_decimal(v);
            }
            return Value{sum};
        }
        auto nested_it = ctx.nested.find(source);
        if (nested_it != ctx.nested.end())
            return resolve_event(type_str, subtype_str, nested_it->second, opts);
        if (ctx.fields.count(source))
            throw std::runtime_error(
                "EVENT context source '" + source + "' must be a map or list of maps");
        throw std::runtime_error(
            "EVENT context source '" + source + "' not found in context");
    }

    throw std::runtime_error("Unknown function: " + name);
}

Value resolve_event(const std::string& type_str, const std::string& subtype_str,
                    const Context& ctx, const EvalOptions& opts) {
    if (!opts.event_resolver)
        throw std::runtime_error("EVENT() calls require an :event_resolver option");

    auto event_key = type_str + "." + subtype_str;
    if (opts.visited.count(event_key))
        throw std::runtime_error("Circular reference detected: " + event_key);

    auto new_visited = opts.visited;
    new_visited.insert(event_key);
    return opts.event_resolver(type_str, subtype_str, ctx, new_visited);
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

        case NodeKind::UnaryOp: {
            auto operand = eval_node(*node.expr, ctx, opts);
            if (is_null(operand)) return Value{NullValue{}};
            return Value{-value_to_decimal(operand)};
        }

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
