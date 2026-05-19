"""Tests for dsqlex_c Python bindings."""
import sys
import os
from decimal import Decimal
from pathlib import Path

# Add the bindings directory to the path
sys.path.insert(0, str(Path(__file__).parent.parent))

from dsqlex_c import eval, parse, evaluate_ast, DsqlexError


def test_simple_field():
    result = eval("field1", {"field1": Decimal("42")})
    assert result == Decimal("42")


def test_arithmetic():
    ctx = {"a": Decimal("10"), "b": Decimal("3")}
    assert eval("a + b", ctx) == Decimal("13")
    assert eval("a - b", ctx) == Decimal("7")
    assert eval("a * b", ctx) == Decimal("30")


def test_string_literal():
    result = eval("'hello'", {})
    assert result == "hello"


def test_bool_literals():
    assert eval("TRUE", {}) is True
    assert eval("FALSE", {}) is False


def test_null_literal():
    assert eval("NULL", {}) is None


def test_comparison():
    ctx = {"a": Decimal("10"), "b": Decimal("20")}
    assert eval("a < b", ctx) is True
    assert eval("a > b", ctx) is False
    assert eval("a = a", ctx) is True


def test_case_expression():
    ctx = {"status": "active", "amount": Decimal("100")}
    result = eval(
        "CASE WHEN status = 'active' THEN amount ELSE 0 END",
        ctx,
    )
    assert result == Decimal("100")


def test_round():
    assert eval("ROUND(3.14159, 2)", {}) == Decimal("3.14")
    assert eval("ROUND(2.555, 2)", {}) == Decimal("2.56")


def test_coalesce():
    assert eval("COALESCE(NULL, NULL, 42)", {}) == Decimal("42")
    assert eval("COALESCE(NULL, 'hello')", {}) == "hello"
    assert eval("COALESCE(NULL, NULL)", {}) is None


def test_upper_lower():
    assert eval("UPPER('hello')", {}) == "HELLO"
    assert eval("LOWER('HELLO')", {}) == "hello"


def test_abs():
    ctx = {"x": Decimal("-42.5")}
    assert eval("ABS(x)", ctx) == Decimal("42.5")


def test_concat():
    ctx = {"first": "Hello", "last": "World"}
    assert eval("CONCAT(first, ' ', last)", ctx) == "Hello World"


def test_in_operator():
    ctx = {"status": "active"}
    assert eval("status IN ('active', 'pending')", ctx) is True
    assert eval("status IN ('deleted')", ctx) is False


def test_like_operator():
    ctx = {"name": "Hello World"}
    assert eval("name LIKE '%world%'", ctx) is True
    assert eval("name LIKE '%xyz%'", ctx) is False


def test_parse_once_eval_many():
    ast = parse("(price * quantity)")
    for i in range(100):
        result = evaluate_ast(ast, {"price": Decimal("10"), "quantity": Decimal(str(i))})
        assert result == Decimal("10") * Decimal(str(i))


def test_nested_context():
    ctx = {"config": {"rate": Decimal("5.00")}}
    result = eval("config.rate", ctx)
    assert result == Decimal("5.00")


def test_error_handling():
    try:
        eval("'unterminated", {})
        assert False, "Should have raised DsqlexError"
    except DsqlexError:
        pass


def test_real_world_expression():
    ctx = {
        "currency": "BRL",
        "amount_local": Decimal("500.00"),
        "amount_usd": Decimal("100.00"),
    }
    result = eval(
        "CASE WHEN currency = 'USD' THEN amount_usd "
        "WHEN currency = 'BRL' THEN amount_local "
        "ELSE NULL END",
        ctx,
    )
    assert result == Decimal("500.00")


if __name__ == "__main__":
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    passed = failed = 0
    for t in tests:
        try:
            t()
            print(f"  {t.__name__}... OK")
            passed += 1
        except Exception as e:
            print(f"  {t.__name__}... FAIL: {e}")
            failed += 1
    print(f"\n=== Python: {passed}/{passed + failed} passed ===")
    sys.exit(1 if failed else 0)
