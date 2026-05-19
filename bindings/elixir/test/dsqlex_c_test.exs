defmodule DsqlexCTest do
  use ExUnit.Case

  test "simple field lookup" do
    assert {:ok, {:decimal, "42"}} = DsqlexC.eval("field1", %{"field1" => {:decimal, "42"}})
  end

  test "arithmetic" do
    ctx = %{"a" => {:decimal, "10"}, "b" => {:decimal, "3"}}
    assert {:ok, {:decimal, "13"}} = DsqlexC.eval("a + b", ctx)
    assert {:ok, {:decimal, "7"}} = DsqlexC.eval("a - b", ctx)
    assert {:ok, {:decimal, "30"}} = DsqlexC.eval("a * b", ctx)
  end

  test "string literal" do
    assert {:ok, "hello"} = DsqlexC.eval("'hello'", %{})
  end

  test "boolean literals" do
    assert {:ok, true} = DsqlexC.eval("TRUE", %{})
    assert {:ok, false} = DsqlexC.eval("FALSE", %{})
  end

  test "null literal" do
    assert {:ok, nil} = DsqlexC.eval("NULL", %{})
  end

  test "comparison" do
    ctx = %{"a" => {:decimal, "10"}, "b" => {:decimal, "20"}}
    assert {:ok, true} = DsqlexC.eval("a < b", ctx)
    assert {:ok, false} = DsqlexC.eval("a > b", ctx)
  end

  test "CASE expression" do
    ctx = %{"status" => "active", "amount" => {:decimal, "100"}}
    assert {:ok, {:decimal, "100"}} =
      DsqlexC.eval("CASE WHEN status = 'active' THEN amount ELSE 0 END", ctx)
  end

  test "ROUND function" do
    assert {:ok, {:decimal, "3.14"}} = DsqlexC.eval("ROUND(3.14159, 2)", %{})
  end

  test "COALESCE function" do
    assert {:ok, {:decimal, "42"}} = DsqlexC.eval("COALESCE(NULL, NULL, 42)", %{})
    assert {:ok, "hello"} = DsqlexC.eval("COALESCE(NULL, 'hello')", %{})
    assert {:ok, nil} = DsqlexC.eval("COALESCE(NULL, NULL)", %{})
  end

  test "UPPER/LOWER functions" do
    assert {:ok, "HELLO"} = DsqlexC.eval("UPPER('hello')", %{})
    assert {:ok, "hello"} = DsqlexC.eval("LOWER('HELLO')", %{})
  end

  test "IN operator" do
    ctx = %{"status" => "active"}
    assert {:ok, true} = DsqlexC.eval("status IN ('active', 'pending')", ctx)
    assert {:ok, false} = DsqlexC.eval("status IN ('deleted')", ctx)
  end

  test "parse once, eval many" do
    {:ok, ast} = DsqlexC.parse("a * b")

    for i <- 1..100 do
      ctx = %{"a" => {:decimal, "10"}, "b" => {:decimal, to_string(i)}}
      expected = to_string(10 * i)
      assert {:ok, {:decimal, ^expected}} = DsqlexC.eval_ast(ast, ctx)
    end
  end

  test "error handling" do
    assert {:error, _reason} = DsqlexC.eval("'unterminated", %{})
  end

  test "real-world expression" do
    ctx = %{
      "currency" => "BRL",
      "amount_local" => {:decimal, "500.00"},
      "amount_usd" => {:decimal, "100.00"}
    }

    assert {:ok, {:decimal, "500.00"}} =
      DsqlexC.eval(
        "CASE WHEN currency = 'USD' THEN amount_usd " <>
        "WHEN currency = 'BRL' THEN amount_local " <>
        "ELSE NULL END",
        ctx
      )
  end
end
