defmodule ToastTest.Formatting do
  @moduledoc false

  def formatter_cb(:diff_enabled?, _default), do: true
  def formatter_cb(:error_info, msg), do: colorize(msg, :red, true)
  def formatter_cb(:extra_info, msg), do: colorize(msg, :cyan, true)
  def formatter_cb(:location_info, msg), do: colorize(msg, [:bright, :default_color], true)
  def formatter_cb(:diff_delete, msg), do: colorize_diff(msg, :red)

  def formatter_cb(:diff_delete_whitespace, msg),
    do: colorize_diff(msg, IO.ANSI.color_background(1, 0, 0))

  def formatter_cb(:diff_insert, msg), do: colorize_diff(msg, :green)

  def formatter_cb(:diff_insert_whitespace, msg),
    do: colorize_diff(msg, IO.ANSI.color_background(0, 1, 0))

  def formatter_cb(:blame_diff, msg), do: colorize_diff(msg, [:red, :bright])
  def formatter_cb(_, msg), do: msg

  def colorize_diff(msg, color) when is_binary(msg) or is_list(msg) do
    colorize(msg, color, true)
  end

  def colorize_diff(msg, color) do
    Inspect.Algebra.concat([ansi_code(color), msg, IO.ANSI.reset()])
  end

  def ansi_code(color) when is_list(color),
    do: IO.iodata_to_binary(Enum.map(color, &ansi_code/1))

  def ansi_code(:bold), do: IO.ANSI.bright()
  def ansi_code(color) when is_atom(color), do: apply(IO.ANSI, color, [])
  def ansi_code(color) when is_binary(color), do: color

  def colorize(text, color, %{colors_enabled: enabled}), do: colorize(text, color, enabled)
  def colorize(text, _color, false), do: text

  def colorize(text, color, true) when is_binary(text) or is_list(text) do
    IO.iodata_to_binary([ansi_code(color), text, IO.ANSI.reset()])
  end

  def colorize(text, color, true) do
    Inspect.Algebra.concat([ansi_code(color), text, IO.ANSI.reset()])
  end
end
