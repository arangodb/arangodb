defmodule ToastTest.Formatting do
  @moduledoc false

  def formatter_cb(:diff_enabled?, _default), do: IO.ANSI.enabled?()
  def formatter_cb(:error_info, msg), do: colorize(msg, :red, IO.ANSI.enabled?())
  def formatter_cb(:extra_info, msg), do: colorize(msg, :cyan, IO.ANSI.enabled?())

  def formatter_cb(:location_info, msg),
    do: colorize(msg, [:bright, :default_color], IO.ANSI.enabled?())

  def formatter_cb(:diff_delete, msg), do: colorize(msg, :red, IO.ANSI.enabled?())

  def formatter_cb(:diff_delete_whitespace, msg),
    do: colorize(msg, IO.ANSI.color_background(1, 0, 0), IO.ANSI.enabled?())

  def formatter_cb(:diff_insert, msg), do: colorize(msg, :green, IO.ANSI.enabled?())

  def formatter_cb(:diff_insert_whitespace, msg),
    do: colorize(msg, IO.ANSI.color_background(0, 1, 0), IO.ANSI.enabled?())

  def formatter_cb(:blame_diff, msg), do: colorize(msg, [:red, :bright], IO.ANSI.enabled?())
  def formatter_cb(_, msg), do: msg

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

  @spec test_outcome(ExUnit.Test.t()) ::
          :passed | :failed | :skipped | :excluded | :invalid | :unknown
  def test_outcome(%{state: nil}), do: :passed
  def test_outcome(%{state: {:failed, _}}), do: :failed
  def test_outcome(%{state: {:skipped, _}}), do: :skipped
  def test_outcome(%{state: {:excluded, _}}), do: :excluded
  def test_outcome(%{state: {:invalid, _}}), do: :invalid
  def test_outcome(%{state: _}), do: :unknown

  @spec display_test_name(atom() | String.t()) :: String.t()
  def display_test_name(name), do: name |> to_string() |> String.replace_prefix("test ", "")
end
