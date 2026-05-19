#pragma once
#include "dsqlex/ast.hpp"
#include <decimal.hh>  // mpdecimal C++ wrapper
#include <functional>
#include <map>
#include <set>
#include <string>
#include <variant>

namespace dsqlex {

/// A DSQLEX value: decimal, string, bool, or null.
struct NullValue {};
using Value = std::variant<decimal::Decimal, std::string, bool, NullValue>;

inline bool is_null(const Value& v) { return std::holds_alternative<NullValue>(v); }

/// Context: flat key-value map (dot-paths resolved at eval time).
/// Values can also be nested maps for dot-path resolution.
struct Context {
    std::map<std::string, Value> fields;

    /// Nested contexts for dot-path access (e.g. config.pricing.margin_rate).
    std::map<std::string, Context> nested;

    void set(const std::string& key, Value val);
    void set_nested(const std::string& key, Context ctx);
};

/// Callback type for custom identifier resolution.
/// Receives (name, visited_set) -> Value. Throw to signal error.
using ResolverFn = std::function<Value(const std::string& name,
                                       const std::set<std::string>& visited)>;

/// Callback type for EVENT() resolution.
/// Receives (type, subtype, context, visited_set) -> Value.
using EventResolverFn = std::function<Value(const std::string& type,
                                            const std::string& subtype,
                                            const Context& ctx,
                                            const std::set<std::string>& visited)>;

struct EvalOptions {
    ResolverFn resolver;
    EventResolverFn event_resolver;
    std::set<std::string> visited;
};

/// Evaluate an AST against a context.
/// Throws std::runtime_error on evaluation errors.
Value evaluate(const ASTNode& ast, const Context& ctx, EvalOptions opts = {});

/// Convert a Value to a printable string representation.
std::string value_to_string(const Value& v);

/// Convert a Value to decimal (throws if not possible).
decimal::Decimal to_decimal(const Value& v);

} // namespace dsqlex
