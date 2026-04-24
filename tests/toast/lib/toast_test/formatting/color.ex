defmodule ToastTest.Formatting.Color do
  @moduledoc false

  @doc "256-color codes for section header backgrounds."
  def failure, do: 1
  def timeout, do: 1
  def crash, do: 160
  def sanitizer, do: 214
  def warning, do: 3
  def summary, do: 4
  def info, do: 195
end
