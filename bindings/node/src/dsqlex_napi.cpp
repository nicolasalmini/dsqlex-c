/**
 * dsqlex_napi.cpp — Node.js N-API binding for libdsqlex.
 *
 * Exports:
 *   parse(expression: string) -> ASTHandle (external)
 *   evalAST(ast: ASTHandle, context: object) -> value
 *   evalString(expression: string, context: object) -> value
 */

#include <napi.h>
#include <cstring>
#include <string>

#include "dsqlex/dsqlex.h"

// ---- Helpers ----------------------------------------------------------------

// Build a dsqlex_context from a JS object
static dsqlex_context* build_context(Napi::Env env, Napi::Object obj) {
    dsqlex_context* ctx = dsqlex_context_new();

    Napi::Array keys = obj.GetPropertyNames();
    uint32_t len = keys.Length();

    for (uint32_t i = 0; i < len; i++) {
        Napi::Value keyVal = keys.Get(i);
        std::string key = keyVal.As<Napi::String>().Utf8Value();

        Napi::Value val = obj.Get(key);

        if (val.IsNull() || val.IsUndefined()) {
            dsqlex_context_set_null(ctx, key.c_str());
        } else if (val.IsBoolean()) {
            dsqlex_context_set_bool(ctx, key.c_str(), val.As<Napi::Boolean>().Value());
        } else if (val.IsNumber()) {
            double d = val.As<Napi::Number>().DoubleValue();
            std::string s = std::to_string(d);
            // Clean up trailing zeros for integers
            if (s.find('.') != std::string::npos) {
                size_t last = s.find_last_not_of('0');
                if (s[last] == '.') last--;
                s = s.substr(0, last + 1);
            }
            dsqlex_context_set_decimal(ctx, key.c_str(), s.c_str());
        } else if (val.IsString()) {
            std::string s = val.As<Napi::String>().Utf8Value();
            dsqlex_context_set_string(ctx, key.c_str(), s.c_str());
        } else if (val.IsObject() && !val.IsArray()) {
            dsqlex_context* nested = dsqlex_context_set_nested(ctx, key.c_str());
            // Recursively populate nested context
            Napi::Object nested_obj = val.As<Napi::Object>();
            Napi::Array nested_keys = nested_obj.GetPropertyNames();
            for (uint32_t j = 0; j < nested_keys.Length(); j++) {
                std::string nkey = nested_keys.Get(j).As<Napi::String>().Utf8Value();
                Napi::Value nval = nested_obj.Get(nkey);

                if (nval.IsNull() || nval.IsUndefined()) {
                    dsqlex_context_set_null(nested, nkey.c_str());
                } else if (nval.IsBoolean()) {
                    dsqlex_context_set_bool(nested, nkey.c_str(), nval.As<Napi::Boolean>().Value());
                } else if (nval.IsNumber()) {
                    double d = nval.As<Napi::Number>().DoubleValue();
                    dsqlex_context_set_decimal(nested, nkey.c_str(), std::to_string(d).c_str());
                } else if (nval.IsString()) {
                    dsqlex_context_set_string(nested, nkey.c_str(), nval.As<Napi::String>().Utf8Value().c_str());
                }
            }
        }
    }

    return ctx;
}

// Convert dsqlex_result to a JS value
static Napi::Value result_to_js(Napi::Env env, dsqlex_result* result) {
    dsqlex_type t = dsqlex_result_type(result);
    Napi::Value val;

    switch (t) {
        case DSQLEX_TYPE_DECIMAL: {
            const char* s = dsqlex_result_decimal(result);
            // Return as string to preserve precision (like Decimal.js)
            val = Napi::String::New(env, s);
            break;
        }
        case DSQLEX_TYPE_STRING: {
            const char* s = dsqlex_result_string(result);
            val = Napi::String::New(env, s);
            break;
        }
        case DSQLEX_TYPE_BOOL:
            val = Napi::Boolean::New(env, dsqlex_result_bool(result));
            break;
        case DSQLEX_TYPE_NULL:
        default:
            val = env.Null();
            break;
    }

    dsqlex_result_free(result);
    return val;
}

// ---- Custom destructor hint for the AST via pointers -----
static void free_ast_destructor(Napi::Env, dsqlex_ast* ast) {
    dsqlex_ast_free(ast);
}

// ---- N-API functions --------------------------------------------------------

// parse(expression: string) -> External<dsqlex_ast>
Napi::Value Parse(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "expression must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string expr = info[0].As<Napi::String>().Utf8Value();
    dsqlex_ast* ast = dsqlex_parse(expr.c_str());

    if (!ast) {
        const char* err = dsqlex_last_error();
        Napi::Error::New(env, err ? err : "Parse error").ThrowAsJavaScriptException();
        return env.Null();
    }

    return Napi::External<dsqlex_ast>::New(env, ast, free_ast_destructor);
}

// evalAST(ast: External, context: object) -> value
Napi::Value EvalAST(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsExternal() || !info[1].IsObject()) {
        Napi::TypeError::New(env, "Expected (ast, context)").ThrowAsJavaScriptException();
        return env.Null();
    }

    dsqlex_ast* ast = info[0].As<Napi::External<dsqlex_ast>>().Data();
    Napi::Object ctx_obj = info[1].As<Napi::Object>();

    dsqlex_context* ctx = build_context(env, ctx_obj);
    dsqlex_result* result = dsqlex_eval(ast, ctx);
    dsqlex_context_free(ctx);

    if (!result) {
        const char* err = dsqlex_last_error();
        Napi::Error::New(env, err ? err : "Eval error").ThrowAsJavaScriptException();
        return env.Null();
    }

    return result_to_js(env, result);
}

// evalString(expression: string, context: object) -> value
Napi::Value EvalString(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsObject()) {
        Napi::TypeError::New(env, "Expected (expression, context)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string expr = info[0].As<Napi::String>().Utf8Value();
    Napi::Object ctx_obj = info[1].As<Napi::Object>();

    dsqlex_context* ctx = build_context(env, ctx_obj);
    dsqlex_result* result = dsqlex_eval_string(expr.c_str(), ctx);
    dsqlex_context_free(ctx);

    if (!result) {
        const char* err = dsqlex_last_error();
        Napi::Error::New(env, err ? err : "Eval error").ThrowAsJavaScriptException();
        return env.Null();
    }

    return result_to_js(env, result);
}

// ---- Module init ------------------------------------------------------------

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("parse", Napi::Function::New(env, Parse));
    exports.Set("evalAST", Napi::Function::New(env, EvalAST));
    exports.Set("evalString", Napi::Function::New(env, EvalString));
    return exports;
}

NODE_API_MODULE(dsqlex_napi, Init)
