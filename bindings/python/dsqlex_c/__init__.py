"""
dsqlex_c — Python bindings for the DSQLEX C library.

Uses ctypes to call into libdsqlex.dylib (the shared C library).
Provides the same API as the pure-Python dsqlex package:
  - parse(expression) -> AST handle
  - eval(expression, context) -> value
  - evaluate_ast(ast, context) -> value
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import pathlib
from decimal import Decimal
from typing import Any, Dict, Optional

# ---------- Load the shared library -------------------------------------------

def _find_lib() -> str:
    """Find libdsqlex.dylib."""
    env = os.environ.get("DSQLEX_LIB_PATH")
    if env and os.path.isfile(env):
        return env

    here = pathlib.Path(__file__).parent
    candidates = [
        here / ".." / ".." / ".." / "build" / "libdsqlex.dylib",
        here / ".." / ".." / ".." / "build" / "libdsqlex.so",
        here / ".." / ".." / ".." / "build" / "libdsqlex.dll",
    ]
    for c in candidates:
        c = c.resolve()
        if c.is_file():
            return str(c)

    found = ctypes.util.find_library("dsqlex")
    if found:
        return found

    raise OSError(
        "Cannot find libdsqlex. Set DSQLEX_LIB_PATH or build the C library first."
    )

_lib = ctypes.CDLL(_find_lib())

# ---------- Function signatures (all opaque pointers as c_void_p) -------------

# Enum values
DSQLEX_TYPE_DECIMAL = 0
DSQLEX_TYPE_STRING = 1
DSQLEX_TYPE_BOOL = 2
DSQLEX_TYPE_NULL = 3

_lib.dsqlex_last_error.restype = ctypes.c_char_p
_lib.dsqlex_last_error.argtypes = []

_lib.dsqlex_parse.restype = ctypes.c_void_p
_lib.dsqlex_parse.argtypes = [ctypes.c_char_p]

_lib.dsqlex_ast_free.restype = None
_lib.dsqlex_ast_free.argtypes = [ctypes.c_void_p]

_lib.dsqlex_context_new.restype = ctypes.c_void_p
_lib.dsqlex_context_new.argtypes = []

_lib.dsqlex_context_set_decimal.restype = None
_lib.dsqlex_context_set_decimal.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.dsqlex_context_set_string.restype = None
_lib.dsqlex_context_set_string.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.dsqlex_context_set_bool.restype = None
_lib.dsqlex_context_set_bool.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_bool]

_lib.dsqlex_context_set_null.restype = None
_lib.dsqlex_context_set_null.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.dsqlex_context_set_nested.restype = ctypes.c_void_p
_lib.dsqlex_context_set_nested.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.dsqlex_context_free.restype = None
_lib.dsqlex_context_free.argtypes = [ctypes.c_void_p]

_lib.dsqlex_eval.restype = ctypes.c_void_p
_lib.dsqlex_eval.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

_lib.dsqlex_eval_string.restype = ctypes.c_void_p
_lib.dsqlex_eval_string.argtypes = [ctypes.c_char_p, ctypes.c_void_p]

_lib.dsqlex_result_type.restype = ctypes.c_int
_lib.dsqlex_result_type.argtypes = [ctypes.c_void_p]

_lib.dsqlex_result_decimal.restype = ctypes.c_char_p
_lib.dsqlex_result_decimal.argtypes = [ctypes.c_void_p]

_lib.dsqlex_result_string.restype = ctypes.c_char_p
_lib.dsqlex_result_string.argtypes = [ctypes.c_void_p]

_lib.dsqlex_result_bool.restype = ctypes.c_bool
_lib.dsqlex_result_bool.argtypes = [ctypes.c_void_p]

_lib.dsqlex_result_free.restype = None
_lib.dsqlex_result_free.argtypes = [ctypes.c_void_p]

# ---------- Error helper ------------------------------------------------------

class DsqlexError(Exception):
    """Error from the DSQLEX C library."""
    pass

def _check_error(ptr, context: str = ""):
    if not ptr:
        err = _lib.dsqlex_last_error()
        msg = err.decode("utf-8") if err else "Unknown error"
        raise DsqlexError(f"{context}: {msg}" if context else msg)

# ---------- Result extraction -------------------------------------------------

def _extract_result(result_p) -> Any:
    """Extract a Python value from a dsqlex_result pointer, then free it."""
    try:
        t = _lib.dsqlex_result_type(result_p)
        if t == DSQLEX_TYPE_DECIMAL:
            s = _lib.dsqlex_result_decimal(result_p)
            return Decimal(s.decode("utf-8"))
        elif t == DSQLEX_TYPE_STRING:
            s = _lib.dsqlex_result_string(result_p)
            return s.decode("utf-8")
        elif t == DSQLEX_TYPE_BOOL:
            return _lib.dsqlex_result_bool(result_p)
        elif t == DSQLEX_TYPE_NULL:
            return None
        else:
            raise DsqlexError(f"Unsupported result type: {t}")
    finally:
        _lib.dsqlex_result_free(result_p)

# ---------- Context builder ---------------------------------------------------

def _build_context(data: Dict[str, Any]):
    """Build a dsqlex_context from a Python dict."""
    ctx = _lib.dsqlex_context_new()
    _populate_context(ctx, data)
    return ctx

def _populate_context(ctx, data: Dict[str, Any]) -> None:
    """Populate a context (top-level or nested)."""
    for key, value in data.items():
        k = key.encode("utf-8")
        if value is None:
            _lib.dsqlex_context_set_null(ctx, k)
        elif isinstance(value, bool):
            _lib.dsqlex_context_set_bool(ctx, k, value)
        elif isinstance(value, (int, float, Decimal)):
            _lib.dsqlex_context_set_decimal(ctx, k, str(value).encode("utf-8"))
        elif isinstance(value, str):
            _lib.dsqlex_context_set_string(ctx, k, value.encode("utf-8"))
        elif isinstance(value, dict):
            nested = _lib.dsqlex_context_set_nested(ctx, k)
            _populate_context(nested, value)

# ---------- AST handle (opaque) -----------------------------------------------

class AST:
    """Opaque handle to a parsed DSQLEX expression. Parse once, evaluate many."""

    def __init__(self, ptr):
        self._ptr = ptr

    def __del__(self):
        if self._ptr:
            _lib.dsqlex_ast_free(self._ptr)
            self._ptr = None

# ---------- Public API --------------------------------------------------------

def parse(expression: str) -> AST:
    """Parse a DSQLEX expression into a reusable AST handle."""
    if not isinstance(expression, str):
        raise TypeError("expression must be a string")
    ptr = _lib.dsqlex_parse(expression.encode("utf-8"))
    _check_error(ptr)
    return AST(ptr)


def evaluate_ast(ast: AST, context: dict, **opts) -> Any:
    """Evaluate a pre-parsed AST against a context dict."""
    if not isinstance(context, dict):
        raise TypeError("context must be a dict")
    ctx = _build_context(context)
    try:
        result = _lib.dsqlex_eval(ast._ptr, ctx)
        _check_error(result)
        return _extract_result(result)
    finally:
        _lib.dsqlex_context_free(ctx)


def eval(expression: str, context: dict, **opts) -> Any:
    """Parse and evaluate a DSQLEX expression in one call."""
    if not isinstance(expression, str):
        raise TypeError("expression must be a string")
    if not isinstance(context, dict):
        raise TypeError("context must be a dict")
    ctx = _build_context(context)
    try:
        result = _lib.dsqlex_eval_string(expression.encode("utf-8"), ctx)
        _check_error(result)
        return _extract_result(result)
    finally:
        _lib.dsqlex_context_free(ctx)
