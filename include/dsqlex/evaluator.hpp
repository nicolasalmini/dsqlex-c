#pragma once
#include "dsqlex/ast.hpp"
#include <decimal.hh>  // mpdecimal C++ wrapper
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace dsqlex {

/// A DSQLEX value: decimal, string, bool, or null.
struct NullValue {};

struct Context;
struct ValueList;

struct Date {
    int year = 0, month = 0, day = 0;
    bool operator==(const Date& o) const {
        return std::tie(year, month, day) == std::tie(o.year, o.month, o.day);
    }
};

struct DateTime {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    bool operator==(const DateTime& o) const {
        return std::tie(year, month, day, hour, minute, second)
            == std::tie(o.year, o.month, o.day, o.hour, o.minute, o.second);
    }
};

struct NaiveDateTime {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    bool operator==(const NaiveDateTime& o) const {
        return std::tie(year, month, day, hour, minute, second)
            == std::tie(o.year, o.month, o.day, o.hour, o.minute, o.second);
    }
};

struct Time {
    int hour = 0, minute = 0, second = 0;
    bool operator==(const Time& o) const {
        return std::tie(hour, minute, second) == std::tie(o.hour, o.minute, o.second);
    }
};

using Value = std::variant<decimal::Decimal, std::string, bool, NullValue,
                           Date, DateTime, NaiveDateTime, Time,
                           std::shared_ptr<ValueList>,
                           std::shared_ptr<Context>>;

struct ValueList {
    std::vector<Value> items;
};

inline bool is_null(const Value& v) { return std::holds_alternative<NullValue>(v); }

/// Context: flat key-value map (dot-paths resolved at eval time).
/// Values can also be nested maps for dot-path resolution.
struct Context {
    std::map<std::string, Value> fields;

    /// Nested contexts for dot-path access (e.g. config.pricing.margin_rate).
    std::map<std::string, Context> nested;

    std::map<std::string, std::vector<Context>> lists;

    void set(const std::string& key, Value val);
    void set_nested(const std::string& key, Context ctx);
    void set_list(const std::string& key, std::vector<Context> items);
    void set_date(const std::string& key, Date val);
    void set_datetime(const std::string& key, DateTime val);
    void set_naive_datetime(const std::string& key, NaiveDateTime val);
    void set_time(const std::string& key, Time val);
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
