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
| Arithmetic | `+`, `-`, `*`, `/` (decimal precision) |
| Comparison | `=`, `!=`, `<`, `>`, `<=`, `>=` |
| Logical | `AND`, `OR` (same-op chaining; mixing requires parens) |
| Conditionals | `CASE WHEN ... THEN ... ELSE ... END` |
| Functions | `ROUND()`, `COALESCE()`/`NVL()`, `UPPER()`, `LOWER()`, `ABS()`, `CONCAT()`, `EVENT()` |
| Membership | `IN (...)`, `NOT IN (...)` |
| Pattern | `LIKE`, `NOT LIKE` (case-insensitive) |
| Null check | `IS NULL`, `IS NOT NULL`, `IS TRUE`, `IS FALSE` |
| Literals | Numbers, strings (`'...'`), `TRUE`, `FALSE`, `NULL` |
| Dot-paths | `config.pricing.margin_rate` (nested context access) |
| Comments | `--`, `#`, `/* ... */` |

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

## Design Decisions

- **Explicit parentheses**: `a + b * c` is rejected. Use `(a + b) * c`. This matches the original DSQLEX semantics.
- **Decimal precision**: All arithmetic uses mpdecimal (same lib as Python's `decimal`). Statically linked to avoid symbol conflicts.
- **Hidden symbols**: Only the C API is exported from the shared library. Internal mpdecimal symbols are hidden.
- **Parse once, eval many**: The AST is immutable and can be shared across threads. Each thread needs its own context.
- **NULL = skip event**: Returning NULL signals "no event" in the ETL pipeline.
