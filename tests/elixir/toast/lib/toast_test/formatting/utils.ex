defmodule ToastTest.Formatting.Utils do
  @moduledoc false

  def print_header(label, true, color) do
    IO.ANSI.format([
      IO.ANSI.color_background(color),
      IO.ANSI.bright(),
      "\n  ",
      label,
      :reset,
      "\n"
    ])
    |> IO.puts()
  end

  def print_header(label, false, _color) do
    bar = String.duplicate("\u2550", 80)
    IO.puts("\n#{bar}")
    IO.puts(label)
    IO.puts(bar)
  end
end
