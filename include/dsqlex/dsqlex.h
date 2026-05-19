/**
 * dsqlex.h — Public C API for libdsqlex
 *
 * This is the stable ABI that language bindings (Elixir NIF, Python ctypes,
 * Node N-API) link against.  All functions are thread-safe for distinct
 * handles; a single dsqlex_ast* may be evaluated concurrently from multiple
 * threads provided each thread uses its own dsqlex_context*.
 */
#ifndef DSQLEX_H
#define DSQLEX_H

#include <stdbool.h>
#include <stddef.h>

/* Export macro — ensures C API symbols are visible even with hidden defaults */
#if defined(_WIN32) || defined(__CYGWIN__)
  #define DSQLEX_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
  #define DSQLEX_API __attribute__((visibility("default")))
#else
  #define DSQLEX_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Opaque handles ------------------------------------------------ */

typedef struct dsqlex_ast      dsqlex_ast;
typedef struct dsqlex_context  dsqlex_context;
typedef struct dsqlex_result   dsqlex_result;

/* ---------- Error handling ------------------------------------------------ */

/** Returns the last error message (thread-local). NULL if no error. */
DSQLEX_API const char* dsqlex_last_error(void);

/* ---------- Parsing ------------------------------------------------------- */

DSQLEX_API dsqlex_ast* dsqlex_parse(const char* expression);
DSQLEX_API void dsqlex_ast_free(dsqlex_ast* ast);

/* ---------- Context ------------------------------------------------------- */

DSQLEX_API dsqlex_context* dsqlex_context_new(void);
DSQLEX_API void dsqlex_context_set_decimal(dsqlex_context* ctx, const char* key, const char* value);
DSQLEX_API void dsqlex_context_set_string(dsqlex_context* ctx, const char* key, const char* value);
DSQLEX_API void dsqlex_context_set_bool(dsqlex_context* ctx, const char* key, bool value);
DSQLEX_API void dsqlex_context_set_null(dsqlex_context* ctx, const char* key);
DSQLEX_API dsqlex_context* dsqlex_context_set_nested(dsqlex_context* ctx, const char* key);
DSQLEX_API void dsqlex_context_free(dsqlex_context* ctx);

/* ---------- Evaluation ---------------------------------------------------- */

DSQLEX_API dsqlex_result* dsqlex_eval(const dsqlex_ast* ast, const dsqlex_context* ctx);

/* ---------- Evaluation with callbacks ------------------------------------- */

typedef dsqlex_result* (*dsqlex_resolver_fn)(const char* name, void* user_data);
typedef dsqlex_result* (*dsqlex_event_resolver_fn)(const char* type,
                                                    const char* subtype,
                                                    const dsqlex_context* ctx,
                                                    void* user_data);

typedef struct {
    dsqlex_resolver_fn       resolver;
    dsqlex_event_resolver_fn event_resolver;
    void*                    user_data;
} dsqlex_eval_options;

DSQLEX_API dsqlex_result* dsqlex_eval_with_options(const dsqlex_ast* ast,
                                                    const dsqlex_context* ctx,
                                                    const dsqlex_eval_options* opts);

/* ---------- Result inspection --------------------------------------------- */

typedef enum {
    DSQLEX_TYPE_DECIMAL = 0,
    DSQLEX_TYPE_STRING  = 1,
    DSQLEX_TYPE_BOOL    = 2,
    DSQLEX_TYPE_NULL    = 3,
} dsqlex_type;

DSQLEX_API dsqlex_type dsqlex_result_type(const dsqlex_result* r);
DSQLEX_API const char* dsqlex_result_decimal(const dsqlex_result* r);
DSQLEX_API const char* dsqlex_result_string(const dsqlex_result* r);
DSQLEX_API bool dsqlex_result_bool(const dsqlex_result* r);
DSQLEX_API void dsqlex_result_free(dsqlex_result* r);

/* ---------- Convenience --------------------------------------------------- */

DSQLEX_API dsqlex_result* dsqlex_eval_string(const char* expression,
                                              const dsqlex_context* ctx);

/* ---------- Result construction (for callbacks) --------------------------- */

DSQLEX_API dsqlex_result* dsqlex_result_new_decimal(const char* value);
DSQLEX_API dsqlex_result* dsqlex_result_new_string(const char* value);
DSQLEX_API dsqlex_result* dsqlex_result_new_bool(bool value);
DSQLEX_API dsqlex_result* dsqlex_result_new_null(void);

#ifdef __cplusplus
}
#endif

#endif /* DSQLEX_H */
