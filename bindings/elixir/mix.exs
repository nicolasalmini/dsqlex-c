defmodule DsqlexC.MixProject do
  use Mix.Project

  @version "0.1.0"

  def project do
    [
      app: :dsqlex_c,
      version: @version,
      elixir: "~> 1.14",
      start_permanent: Mix.env() == :prod,
      compilers: [:elixir_make | Mix.compilers()],
      make_makefile: "Makefile",
      deps: deps(),
      aliases: aliases()
    ]
  end

  def application do
    [extra_applications: [:logger]]
  end

  defp deps do
    [
      {:elixir_make, "~> 0.8", runtime: false}
    ]
  end

  defp aliases do
    []
  end
end
