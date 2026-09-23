# dsqlex-c

A C/C++ implementation of the DSQLEX expression evaluator, with language bindings for Elixir (NIF), Python (ctypes), and Node.js (N-API).

Follows the DuckDB model: one native core, thin bindings for each language.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    libdsqlex (C++)                       │
│                                                         │
│   String → Lexer → Parser → Evaluator → Result          │
│                                                         │
│   - mpdecimal for arbitrary-precision decimals          │
│   - Statically linked (no symbol conflicts with Python) │
│   - Thread-safe (distinct handles per thread)           │
│   - Hidden symbols (only C API exported)                │
└────────────────────────┬────────────────────────────────┘
                         │ extern "C" API
          ┌──────────────┼──────────────┐
          │              │              │
     ┌────▼────┐   ┌────▼────┐   ┌────▼────┐
     │  Elixir │   │  Python │   │  Node.js│
     │   NIF   │   │  ctypes │   │  N-API  │
     └─────────┘   └─────────┘   └─────────┘
```

## Building

### Prerequisites

- CMake 3.20+
- C++17 compiler (clang, gcc)
- mpdecimal (`brew install mpdecimal`)

### Build the core library

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run C++ tests

```bash
./build/dsqlex_tests
```

## Language Bindings

### Python

No compilation needed — uses ctypes to call the shared library directly.

```python
import sys; sys.path.insert(0, 'bindings/python')
from dsqlex_c import eval, parse, evaluate_ast
from decimal import Decimal

# One-shot
result = eval("(price * quantity) + tax", {
    "price": Decimal("100.00"),
    "quantity": Decimal("5"),
    "tax": Decimal("50.00"),
})
# result = Decimal("550.00")

# Parse once, evaluate many
ast = parse("amount * rate")
for record in records:
    result = evaluate_ast(ast, record)
```

```bash
python3 bindings/python/tests/test_dsqlex_c.py
```

### Elixir

```bash
cd bindings/elixir
mix deps.get
mix compile
mix test
```

```elixir
# One-shot
{:ok, {:decimal, "550.00"}} = DsqlexC.eval(
  "(price * quantity) + tax",
  %{"price" => {:decimal, "100.00"}, "quantity" => {:decimal, "5"}, "tax" => {:decimal, "50.00"}}
)

# Parse once, evaluate many
{:ok, ast} = DsqlexC.parse("amount * rate")
{:ok, result} = DsqlexC.eval_ast(ast, %{"amount" => {:decimal, "100"}, "rate" => {:decimal, "1.5"}})
```

### Node.js

```bash
cd bindings/node
npm install
npm test
```

```javascript
const { parse, evalAST, evalString } = require('dsqlex-c');

// One-shot (decimals returned as strings for precision)
const result = evalString('(price * quantity) + tax', {
  price: 100, quantity: 5, tax: 50
});
// result = "550"

// Parse once, evaluate many
const ast = parse('amount * rate');
const val = evalAST(ast, { amount: 100, rate: 1.5 });
```

## Supported Features

| Feature | Syntax |
|---------|--------|
| Arithmetic | `+`, `-`, `*`, `/` (decimal precision), unary `-` (`-x`, `-(a + b)`) |
| Comparison | `=`, `!=`, `<`, `>`, `<=`, `>=` |
| Logical | `AND`, `OR` (same-op chaining; mixing requires parens) |
| Conditionals | `CASE WHEN ... THEN ... ELSE ... END` |
| Functions | `ROUND()`, `COALESCE()`/`NVL()`, `UPPER()`, `LOWER()`, `ABS()`, `CONCAT()`, `LEAST()`, `GREATEST()`, `EVENT()` |
| Membership | `IN (...)`, `NOT IN (...)` |
| Pattern | `LIKE`, `NOT LIKE` (case-insensitive) |
| Null check | `IS NULL`, `IS NOT NULL`, `IS TRUE`, `IS FALSE` |
| Literals | Numbers, strings (`'...'`), `TRUE`, `FALSE`, `NULL` |
| Identifiers | `field`, `a.b`, single trailing `?` (`active?`, `user.admin?`) |
| Dot-paths | `config.pricing.margin_rate` (nested contexts and lists of contexts) |
| Comments | `--`, `#`, `/* ... */` |

NULL propagates through arithmetic and `ROUND`/`ABS`. `LEAST`/`GREATEST` require at least one argument, return NULL if any argument is NULL, compare decimals numerically, strings lexicographically, and same-kind temporal values (date/datetime/naive datetime/time) chronologically.

`EVENT(type, subtype)` takes literal identifier arguments and calls the configured event resolver `(type, subtype, ctx, visited)`; `EVENT(type, subtype, source)` resolves `source` against a named nested context or a named list of contexts (list results are summed as decimals; an empty list yields `0`).

## C API

The stable ABI is defined in `include/dsqlex/dsqlex.h`:

```c
// Parse once
dsqlex_ast* dsqlex_parse(const char* expression);
void dsqlex_ast_free(dsqlex_ast* ast);

// Build context
dsqlex_context* dsqlex_context_new(void);
void dsqlex_context_set_decimal(dsqlex_context*, const char* key, const char* val);
void dsqlex_context_set_string(dsqlex_context*, const char* key, const char* val);
void dsqlex_context_set_bool(dsqlex_context*, const char* key, bool val);
void dsqlex_context_set_null(dsqlex_context*, const char* key);
dsqlex_context* dsqlex_context_set_nested(dsqlex_context*, const char* key);
dsqlex_context* dsqlex_context_list_add(dsqlex_context*, const char* key);
void dsqlex_context_set_empty_list(dsqlex_context*, const char* key);
void dsqlex_context_set_date(dsqlex_context*, const char* key, int y, int m, int d);
void dsqlex_context_set_datetime(dsqlex_context*, const char* key, int y, int m, int d, int h, int mi, int s);
void dsqlex_context_set_naive_datetime(dsqlex_context*, const char* key, int y, int m, int d, int h, int mi, int s);
void dsqlex_context_set_time(dsqlex_context*, const char* key, int h, int mi, int s);
void dsqlex_context_free(dsqlex_context*);

// Evaluate many times
dsqlex_result* dsqlex_eval(const dsqlex_ast*, const dsqlex_context*);
dsqlex_type dsqlex_result_type(const dsqlex_result*);
const char* dsqlex_result_decimal(const dsqlex_result*);
const char* dsqlex_result_string(const dsqlex_result*);
bool dsqlex_result_bool(const dsqlex_result*);
void dsqlex_result_free(dsqlex_result*);

// Error handling
const char* dsqlex_last_error(void);
```

Result types reported by `dsqlex_result_type` are `DSQLEX_TYPE_DECIMAL`, `DSQLEX_TYPE_STRING`, `DSQLEX_TYPE_BOOL`, `DSQLEX_TYPE_NULL`, and (since the temporal/list parity additions) `DSQLEX_TYPE_DATE`, `DSQLEX_TYPE_DATETIME`, `DSQLEX_TYPE_NAIVE_DATETIME`, `DSQLEX_TYPE_TIME`, `DSQLEX_TYPE_LIST`, `DSQLEX_TYPE_MAP`. Temporal and list results expose a serialized form through `dsqlex_result_string`; for `DSQLEX_TYPE_MAP` results, `dsqlex_result_decimal` and `dsqlex_result_string` return NULL and there is no map introspection API.

The C ABI additions above (temporal/list input setters and the extended result types) are available to direct consumers of `libdsqlex`. The existing Python (ctypes), Elixir (NIF), and Node.js (N-API) wrappers already support nested map context inputs, but not the newly added temporal/list inputs or typed outputs — those need a follow-up binding update; the Python binding raises `DsqlexError` rather than silently returning `None` when it encounters an unsupported result type.

## Design Decisions

- **Explicit parentheses**: `a + b * c` is rejected. Use `(a + b) * c`. This matches the original DSQLEX semantics.
- **Decimal precision**: All arithmetic uses mpdecimal (same lib as Python's `decimal`). Statically linked to avoid symbol conflicts.
- **Hidden symbols**: Only the C API is exported from the shared library. Internal mpdecimal symbols are hidden.
- **Parse once, eval many**: The AST is immutable and can be shared across threads. Each thread needs its own context.
- **NULL = skip event**: Returning NULL signals "no event" in the ETL pipeline.

## Related

- [dsqlex-rs](https://github.com/nicolasalmini/dsqlex-rs) — Rust implementation (rust_decimal)
- [dsqlex-go](https://github.com/nicolasalmini/dsqlex-go) — Go implementation (govalues/decimal)
- [dsqlex-py](https://github.com/nicolasalmini/dsqlex-py) — Python implementation
- [dsqlex-ts](https://github.com/nicolasalmini/dsqlex-ts) — TypeScript implementation
- [dsqlex-bench](https://github.com/nicolasalmini/dsqlex-bench) — Cross-language benchmark suite
