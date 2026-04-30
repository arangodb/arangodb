################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

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

  @spec format_duration_us(non_neg_integer()) :: String.t()
  def format_duration_us(us) when us >= 60_000_000 do
    minutes = div(us, 60_000_000)
    secs = Float.round(rem(us, 60_000_000) / 1_000_000, 1)
    "#{minutes}m#{:erlang.float_to_binary(secs, decimals: 1)}s"
  end

  def format_duration_us(us) when us >= 1_000_000 do
    secs = Float.round(us / 1_000_000, 1)
    "#{:erlang.float_to_binary(secs, decimals: 1)}s"
  end

  def format_duration_us(us) when us >= 1_000 do
    ms = Float.round(us / 1_000, 0) |> trunc()
    "#{ms}ms"
  end

  def format_duration_us(us), do: "#{us}µs"
end
