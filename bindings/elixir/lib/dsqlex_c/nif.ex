defmodule DsqlexC.Nif do
  @moduledoc false
  @on_load :load_nif

  def load_nif do
    path = :filename.join(:code.priv_dir(:dsqlex_c), ~c"dsqlex_nif")
    :erlang.load_nif(path, 0)
  end

  def parse(_expression), do: :erlang.nif_error(:not_loaded)
  def eval_ast(_ast, _context), do: :erlang.nif_error(:not_loaded)
  def eval_string(_expression, _context), do: :erlang.nif_error(:not_loaded)
end
