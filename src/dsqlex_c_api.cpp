#include "dsqlex/dsqlex.h"
#include "dsqlex/ast.hpp"
#include "dsqlex/evaluator.hpp"
#include "dsqlex/parser.hpp"

#include <string>
#include <utility>
#include <vector>

// ---- Opaque handle implementations -----------------------------------------

struct dsqlex_ast {
    dsqlex::ASTPtr ptr;
};

struct dsqlex_context {
    dsqlex::Context ctx;
    // Track child contexts with their keys for sync before eval
    std::vector<std::pair<std::string, dsqlex_context*>> children;
    std::vector<std::pair<std::string, dsqlex_context*>> list_children;
    ~dsqlex_context() {
        for (auto& [k, c] : children) delete c;
        for (auto& [k, c] : list_children) delete c;
    }

    // Recursively sync children's ctx into parent's nested map
    void sync() {
        for (auto& [key, child] : children) {
            child->sync();
            ctx.set_nested(key, child->ctx);
        }
        std::map<std::string, std::vector<dsqlex_context*>> grouped;
        for (auto& [key, child] : list_children)
            grouped[key].push_back(child);
        for (auto& [key, group] : grouped) {
            std::vector<dsqlex::Context> items;
            for (auto* child : group) {
                child->sync();
                items.push_back(child->ctx);
            }
            ctx.set_list(key, std::move(items));
        }
    }
};

struct dsqlex_result {
    dsqlex::Value value;
    std::string str_cache; // cached string for decimal/string getters
};

// ---- Thread-local error string ---------------------------------------------

static thread_local std::string tl_error;

static void set_error(const std::string& msg) {
    tl_error = msg;
}

static void clear_error() {
    tl_error.clear();
}

// ---- C API implementation ---------------------------------------------------

extern "C" {

const char* dsqlex_last_error(void) {
    if (tl_error.empty()) return nullptr;
    return tl_error.c_str();
}

dsqlex_ast* dsqlex_parse(const char* expression) {
    clear_error();
    try {
        auto ast = dsqlex::parse(std::string(expression));
        auto* handle = new dsqlex_ast{std::move(ast)};
        return handle;
    } catch (const std::exception& e) {
        set_error(e.what());
        return nullptr;
    }
}

void dsqlex_ast_free(dsqlex_ast* ast) {
    delete ast;
}

dsqlex_context* dsqlex_context_new(void) {
    return new dsqlex_context{};
}

void dsqlex_context_set_decimal(dsqlex_context* ctx, const char* key, const char* value) {
    ctx->ctx.set(key, dsqlex::Value{decimal::Decimal(std::string(value))});
}

void dsqlex_context_set_string(dsqlex_context* ctx, const char* key, const char* value) {
    ctx->ctx.set(key, dsqlex::Value{std::string(value)});
}

void dsqlex_context_set_bool(dsqlex_context* ctx, const char* key, bool value) {
    ctx->ctx.set(key, dsqlex::Value{value});
}

void dsqlex_context_set_null(dsqlex_context* ctx, const char* key) {
    ctx->ctx.set(key, dsqlex::Value{dsqlex::NullValue{}});
}

dsqlex_context* dsqlex_context_set_nested(dsqlex_context* ctx, const char* key) {
    auto* child = new dsqlex_context{};
    ctx->children.emplace_back(std::string(key), child);
    return child;
}

dsqlex_context* dsqlex_context_list_add(dsqlex_context* ctx, const char* key) {
    auto* child = new dsqlex_context{};
    ctx->list_children.emplace_back(std::string(key), child);
    return child;
}

void dsqlex_context_set_empty_list(dsqlex_context* ctx, const char* key) {
    ctx->ctx.set_list(key, {});
}

void dsqlex_context_set_date(dsqlex_context* ctx, const char* key,
                             int year, int month, int day) {
    ctx->ctx.set(key, dsqlex::Value{dsqlex::Date{year, month, day}});
}

void dsqlex_context_set_datetime(dsqlex_context* ctx, const char* key,
                                 int year, int month, int day,
                                 int hour, int minute, int second) {
    ctx->ctx.set(key, dsqlex::Value{dsqlex::DateTime{year, month, day, hour, minute, second}});
}

void dsqlex_context_set_naive_datetime(dsqlex_context* ctx, const char* key,
                                       int year, int month, int day,
                                       int hour, int minute, int second) {
    ctx->ctx.set(key, dsqlex::Value{dsqlex::NaiveDateTime{year, month, day, hour, minute, second}});
}

void dsqlex_context_set_time(dsqlex_context* ctx, const char* key,
                             int hour, int minute, int second) {
    ctx->ctx.set(key, dsqlex::Value{dsqlex::Time{hour, minute, second}});
}

void dsqlex_context_free(dsqlex_context* ctx) {
    delete ctx;
}

dsqlex_result* dsqlex_eval(const dsqlex_ast* ast, const dsqlex_context* ctx) {
    clear_error();
    try {
        const_cast<dsqlex_context*>(ctx)->sync();
        auto value = dsqlex::evaluate(*ast->ptr, ctx->ctx);
        auto* r = new dsqlex_result{std::move(value), {}};
        return r;
    } catch (const std::exception& e) {
        set_error(e.what());
        return nullptr;
    }
}

dsqlex_result* dsqlex_eval_with_options(const dsqlex_ast* ast,
                                        const dsqlex_context* ctx,
                                        const dsqlex_eval_options* opts) {
    clear_error();
    try {
        const_cast<dsqlex_context*>(ctx)->sync();
        dsqlex::EvalOptions eval_opts;

        if (opts && opts->resolver) {
            auto resolver_fn = opts->resolver;
            auto user_data = opts->user_data;
            eval_opts.resolver = [resolver_fn, user_data](
                const std::string& name,
                const std::set<std::string>& /*visited*/) -> dsqlex::Value {
                auto* result = resolver_fn(name.c_str(), user_data);
                if (!result)
                    throw std::runtime_error("Resolver returned NULL for: " + name);
                auto val = std::move(result->value);
                delete result;
                return val;
            };
        }

        if (opts && opts->event_resolver) {
            auto event_fn = opts->event_resolver;
            auto user_data = opts->user_data;
            eval_opts.event_resolver = [event_fn, user_data](
                const std::string& type,
                const std::string& subtype,
                const dsqlex::Context& eval_ctx,
                const std::set<std::string>& /*visited*/) -> dsqlex::Value {
                // Create a temporary dsqlex_context for the callback
                dsqlex_context tmp_ctx;
                tmp_ctx.ctx = eval_ctx;
                auto* result = event_fn(type.c_str(), subtype.c_str(), &tmp_ctx, user_data);
                if (!result)
                    throw std::runtime_error(
                        "Event resolver returned NULL for: " + type + "." + subtype);
                auto val = std::move(result->value);
                delete result;
                return val;
            };
        }

        auto value = dsqlex::evaluate(*ast->ptr, ctx->ctx, eval_opts);
        return new dsqlex_result{std::move(value), {}};
    } catch (const std::exception& e) {
        set_error(e.what());
        return nullptr;
    }
}

dsqlex_type dsqlex_result_type(const dsqlex_result* r) {
    if (std::holds_alternative<decimal::Decimal>(r->value)) return DSQLEX_TYPE_DECIMAL;
    if (std::holds_alternative<std::string>(r->value)) return DSQLEX_TYPE_STRING;
    if (std::holds_alternative<bool>(r->value)) return DSQLEX_TYPE_BOOL;
    if (std::holds_alternative<dsqlex::Date>(r->value)) return DSQLEX_TYPE_DATE;
    if (std::holds_alternative<dsqlex::DateTime>(r->value)) return DSQLEX_TYPE_DATETIME;
    if (std::holds_alternative<dsqlex::NaiveDateTime>(r->value)) return DSQLEX_TYPE_NAIVE_DATETIME;
    if (std::holds_alternative<dsqlex::Time>(r->value)) return DSQLEX_TYPE_TIME;
    if (std::holds_alternative<std::shared_ptr<dsqlex::ValueList>>(r->value))
        return DSQLEX_TYPE_LIST;
    if (std::holds_alternative<std::shared_ptr<dsqlex::Context>>(r->value))
        return DSQLEX_TYPE_MAP;
    return DSQLEX_TYPE_NULL;
}

const char* dsqlex_result_decimal(const dsqlex_result* r) {
    auto* d = std::get_if<decimal::Decimal>(&r->value);
    if (!d) return nullptr;
    const_cast<dsqlex_result*>(r)->str_cache = d->to_sci();
    return r->str_cache.c_str();
}

const char* dsqlex_result_string(const dsqlex_result* r) {
    auto* s = std::get_if<std::string>(&r->value);
    if (s) return s->c_str();
    if (std::holds_alternative<dsqlex::Date>(r->value) ||
        std::holds_alternative<dsqlex::DateTime>(r->value) ||
        std::holds_alternative<dsqlex::NaiveDateTime>(r->value) ||
        std::holds_alternative<dsqlex::Time>(r->value) ||
        std::holds_alternative<std::shared_ptr<dsqlex::ValueList>>(r->value)) {
        const_cast<dsqlex_result*>(r)->str_cache = dsqlex::value_to_string(r->value);
        return r->str_cache.c_str();
    }
    return nullptr;
}

bool dsqlex_result_bool(const dsqlex_result* r) {
    auto* b = std::get_if<bool>(&r->value);
    return b ? *b : false;
}

void dsqlex_result_free(dsqlex_result* r) {
    delete r;
}

dsqlex_result* dsqlex_eval_string(const char* expression, const dsqlex_context* ctx) {
    auto* ast = dsqlex_parse(expression);
    if (!ast) return nullptr;
    auto* result = dsqlex_eval(ast, ctx);
    dsqlex_ast_free(ast);
    return result;
}

dsqlex_result* dsqlex_result_new_decimal(const char* value) {
    return new dsqlex_result{dsqlex::Value{decimal::Decimal(std::string(value))}, {}};
}

dsqlex_result* dsqlex_result_new_string(const char* value) {
    return new dsqlex_result{dsqlex::Value{std::string(value)}, {}};
}

dsqlex_result* dsqlex_result_new_bool(bool value) {
    return new dsqlex_result{dsqlex::Value{value}, {}};
}

dsqlex_result* dsqlex_result_new_null(void) {
    return new dsqlex_result{dsqlex::Value{dsqlex::NullValue{}}, {}};
}

} // extern "C"
