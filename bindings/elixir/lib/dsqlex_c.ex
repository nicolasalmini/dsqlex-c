defmodule DsqlexC do
  @moduledoc """
  DSQLEX expression evaluator backed by a native C library.

  Provides the same API as the pure-Elixir Dsqlex, but evaluation is
  performed by libdsqlex (C/C++ with mpdecimal).

  ## Usage

      # Parse once, evaluate many times
      {:ok, ast} = DsqlexC.parse("(amount_local / currency_rate)")
      {:ok, result} = DsqlexC.eval_ast(ast, %{"amount_local" => "500.00", "currency_rate" => "5.00"})

      # Or one-shot
      {:ok, result} = DsqlexC.eval("amount * 2", %{"amount" => "100"})
  """

  alias DsqlexC.Nif

  @doc """
  Parse a DSQLEX expression into a reusable AST reference.

  Returns `{:ok, ast}` or `{:error, reason}`.
  """
  def parse(expression) when is_binary(expression) do
    Nif.parse(expression)
  end

  @doc """
  Evaluate a pre-parsed AST against a context map.

  Context values can be:
  - Strings (binary)
  - Integers / floats (converted to decimal internally)
  - `{:decimal, "123.45"}` tuples for explicit decimal precision
  - Booleans (`true` / `false`)
  - `nil` (NULL)
  - Maps (for nested/dot-path access)

  Returns `{:ok, result}` or `{:error, reason}`.

  Results are:
  - `{:decimal, "123.45"}` for numeric results
  - Binary string for string results
  - `true` / `false` for boolean results
  - `nil` for NULL
  """
  def eval_ast(ast, context) when is_map(context) do
    ctx = normalize_context(context)
    Nif.eval_ast(ast, ctx)
  end

  @doc """
  Parse and evaluate in one call. Convenience for one-off evaluations.

  Returns `{:ok, result}` or `{:error, reason}`.
  """
  def eval(expression, context) when is_binary(expression) and is_map(context) do
    ctx = normalize_context(context)
    Nif.eval_string(expression, ctx)
  end

  # Normalize context: convert Decimal structs to {:decimal, string} tuples,
  # and atom keys to string keys.
  defp normalize_context(map) do
    Map.new(map, fn {k, v} ->
      key = if is_atom(k), do: Atom.to_string(k), else: k
      {key, normalize_value(v)}
    end)
  end

  defp normalize_value(v) when is_map(v), do: normalize_context(v)
  defp normalize_value(v), do: v
end
