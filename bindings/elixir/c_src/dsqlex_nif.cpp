/**
 * dsqlex_nif.cpp — Erlang NIF binding for libdsqlex.
 *
 * Exposes:
 *   dsqlex_c_nif:parse/1       -> {:ok, ast_ref} | {:error, reason}
 *   dsqlex_c_nif:eval_ast/2    -> {:ok, result}  | {:error, reason}
 *   dsqlex_c_nif:eval_string/2 -> {:ok, result}  | {:error, reason}
 */

#include <erl_nif.h>
#include <cstring>
#include <string>

// Use the C API (not C++ internals) for ABI stability
#include "dsqlex/dsqlex.h"

// ---- Resource types ---------------------------------------------------------

static ErlNifResourceType* AST_RESOURCE = nullptr;

struct ASTResource {
    dsqlex_ast* ast;
};

static void ast_resource_dtor(ErlNifEnv*, void* obj) {
    auto* r = static_cast<ASTResource*>(obj);
    if (r->ast) {
        dsqlex_ast_free(r->ast);
        r->ast = nullptr;
    }
}

// ---- Helpers ----------------------------------------------------------------

static ERL_NIF_TERM make_ok(ErlNifEnv* env, ERL_NIF_TERM value) {
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), value);
}

static ERL_NIF_TERM make_error(ErlNifEnv* env, const char* reason) {
    return enif_make_tuple2(env,
        enif_make_atom(env, "error"),
        enif_make_string(env, reason, ERL_NIF_LATIN1));
}

static ERL_NIF_TERM make_error_from_last(ErlNifEnv* env) {
    const char* err = dsqlex_last_error();
    return make_error(env, err ? err : "Unknown error");
}

// Build a dsqlex_context* from an Erlang map
static dsqlex_context* build_context_from_map(ErlNifEnv* env, ERL_NIF_TERM map);

static void populate_context(ErlNifEnv* env, dsqlex_context* ctx, ERL_NIF_TERM map) {
    ERL_NIF_TERM key, value;
    ErlNifMapIterator iter;

    if (!enif_map_iterator_create(env, map, &iter, ERL_NIF_MAP_ITERATOR_FIRST))
        return;

    while (enif_map_iterator_get_pair(env, &iter, &key, &value)) {
        // Get key as string
        ErlNifBinary key_bin;
        char key_buf[256];
        unsigned int key_len;

        if (enif_get_string(env, key, key_buf, sizeof(key_buf), ERL_NIF_LATIN1)) {
            // Already got it
        } else if (enif_inspect_binary(env, key, &key_bin)) {
            size_t len = key_bin.size < sizeof(key_buf) - 1 ? key_bin.size : sizeof(key_buf) - 1;
            memcpy(key_buf, key_bin.data, len);
            key_buf[len] = '\0';
        } else if (enif_get_atom(env, key, key_buf, sizeof(key_buf), ERL_NIF_LATIN1)) {
            // Already got it
        } else {
            enif_map_iterator_next(env, &iter);
            continue;
        }

        // Determine value type and set accordingly
        char val_buf[1024];
        double d;
        int i;
        ErlNifBinary val_bin;

        if (enif_is_identical(value, enif_make_atom(env, "nil")) ||
            enif_is_identical(value, enif_make_atom(env, "null"))) {
            dsqlex_context_set_null(ctx, key_buf);
        } else if (enif_is_identical(value, enif_make_atom(env, "true"))) {
            dsqlex_context_set_bool(ctx, key_buf, true);
        } else if (enif_is_identical(value, enif_make_atom(env, "false"))) {
            dsqlex_context_set_bool(ctx, key_buf, false);
        } else if (enif_get_int(env, value, &i)) {
            snprintf(val_buf, sizeof(val_buf), "%d", i);
            dsqlex_context_set_decimal(ctx, key_buf, val_buf);
        } else if (enif_get_double(env, value, &d)) {
            snprintf(val_buf, sizeof(val_buf), "%.17g", d);
            dsqlex_context_set_decimal(ctx, key_buf, val_buf);
        } else if (enif_get_string(env, value, val_buf, sizeof(val_buf), ERL_NIF_LATIN1)) {
            dsqlex_context_set_string(ctx, key_buf, val_buf);
        } else if (enif_inspect_binary(env, value, &val_bin)) {
            std::string s(reinterpret_cast<const char*>(val_bin.data), val_bin.size);
            dsqlex_context_set_string(ctx, key_buf, s.c_str());
        } else if (enif_is_map(env, value)) {
            auto* nested = dsqlex_context_set_nested(ctx, key_buf);
            populate_context(env, nested, value);
        }
        // Tuples like {:decimal, "123.45"} for explicit decimal
        else {
            int arity;
            const ERL_NIF_TERM* elements;
            if (enif_get_tuple(env, value, &arity, &elements) && arity == 2) {
                char tag[32];
                if (enif_get_atom(env, elements[0], tag, sizeof(tag), ERL_NIF_LATIN1) &&
                    strcmp(tag, "decimal") == 0) {
                    if (enif_get_string(env, elements[1], val_buf, sizeof(val_buf), ERL_NIF_LATIN1) ||
                        (enif_inspect_binary(env, elements[1], &val_bin) &&
                         (memcpy(val_buf, val_bin.data, val_bin.size < sizeof(val_buf) - 1 ? val_bin.size : sizeof(val_buf) - 1),
                          val_buf[val_bin.size < sizeof(val_buf) - 1 ? val_bin.size : sizeof(val_buf) - 1] = '\0', true))) {
                        dsqlex_context_set_decimal(ctx, key_buf, val_buf);
                    }
                }
            }
        }

        enif_map_iterator_next(env, &iter);
    }

    enif_map_iterator_destroy(env, &iter);
}

static dsqlex_context* build_context_from_map(ErlNifEnv* env, ERL_NIF_TERM map) {
    auto* ctx = dsqlex_context_new();
    populate_context(env, ctx, map);
    return ctx;
}

// Convert a dsqlex_result to an Erlang term
static ERL_NIF_TERM result_to_term(ErlNifEnv* env, dsqlex_result* result) {
    dsqlex_type t = dsqlex_result_type(result);

    ERL_NIF_TERM term;
    switch (t) {
        case DSQLEX_TYPE_DECIMAL: {
            const char* s = dsqlex_result_decimal(result);
            // Return as binary string (caller can convert to Decimal)
            ErlNifBinary bin;
            size_t len = strlen(s);
            enif_alloc_binary(len, &bin);
            memcpy(bin.data, s, len);
            // Return {:decimal, "value"}
            term = enif_make_tuple2(env,
                enif_make_atom(env, "decimal"),
                enif_make_binary(env, &bin));
            break;
        }
        case DSQLEX_TYPE_STRING: {
            const char* s = dsqlex_result_string(result);
            ErlNifBinary bin;
            size_t len = strlen(s);
            enif_alloc_binary(len, &bin);
            memcpy(bin.data, s, len);
            term = enif_make_binary(env, &bin);
            break;
        }
        case DSQLEX_TYPE_BOOL:
            term = enif_make_atom(env, dsqlex_result_bool(result) ? "true" : "false");
            break;
        case DSQLEX_TYPE_NULL:
        default:
            term = enif_make_atom(env, "nil");
            break;
    }

    dsqlex_result_free(result);
    return term;
}

// ---- NIF functions ----------------------------------------------------------

// parse(expression :: binary) -> {:ok, ast_ref} | {:error, reason}
static ERL_NIF_TERM nif_parse(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ErlNifBinary expr_bin;
    if (!enif_inspect_binary(env, argv[0], &expr_bin))
        return make_error(env, "expression must be a binary");

    std::string expr(reinterpret_cast<const char*>(expr_bin.data), expr_bin.size);

    dsqlex_ast* ast = dsqlex_parse(expr.c_str());
    if (!ast)
        return make_error_from_last(env);

    auto* resource = static_cast<ASTResource*>(
        enif_alloc_resource(AST_RESOURCE, sizeof(ASTResource)));
    resource->ast = ast;

    ERL_NIF_TERM ref = enif_make_resource(env, resource);
    enif_release_resource(resource);

    return make_ok(env, ref);
}

// eval_ast(ast_ref, context :: map) -> {:ok, result} | {:error, reason}
static ERL_NIF_TERM nif_eval_ast(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ASTResource* resource;
    if (!enif_get_resource(env, argv[0], AST_RESOURCE, reinterpret_cast<void**>(&resource)))
        return make_error(env, "invalid AST resource");

    if (!enif_is_map(env, argv[1]))
        return make_error(env, "context must be a map");

    dsqlex_context* ctx = build_context_from_map(env, argv[1]);
    dsqlex_result* result = dsqlex_eval(resource->ast, ctx);
    dsqlex_context_free(ctx);

    if (!result)
        return make_error_from_last(env);

    return make_ok(env, result_to_term(env, result));
}

// eval_string(expression :: binary, context :: map) -> {:ok, result} | {:error, reason}
static ERL_NIF_TERM nif_eval_string(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ErlNifBinary expr_bin;
    if (!enif_inspect_binary(env, argv[0], &expr_bin))
        return make_error(env, "expression must be a binary");

    if (!enif_is_map(env, argv[1]))
        return make_error(env, "context must be a map");

    std::string expr(reinterpret_cast<const char*>(expr_bin.data), expr_bin.size);

    dsqlex_context* ctx = build_context_from_map(env, argv[1]);
    dsqlex_result* result = dsqlex_eval_string(expr.c_str(), ctx);
    dsqlex_context_free(ctx);

    if (!result)
        return make_error_from_last(env);

    return make_ok(env, result_to_term(env, result));
}

// ---- NIF module setup -------------------------------------------------------

static ErlNifFunc nif_funcs[] = {
    {"parse",       1, nif_parse,      0},
    {"eval_ast",    2, nif_eval_ast,   0},
    {"eval_string", 2, nif_eval_string, 0},
};

static int on_load(ErlNifEnv* env, void**, ERL_NIF_TERM) {
    AST_RESOURCE = enif_open_resource_type(
        env, nullptr, "dsqlex_ast",
        ast_resource_dtor,
        ERL_NIF_RT_CREATE, nullptr);
    if (!AST_RESOURCE) return -1;
    return 0;
}

ERL_NIF_INIT(Elixir.DsqlexC.Nif, nif_funcs, on_load, nullptr, nullptr, nullptr)
