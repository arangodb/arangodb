defmodule Toast.LogFormatter do
  @moduledoc """
  Custom log formatter for Toast.

  Provides two format callbacks:

  - `format/4` — Elixir Logger console formatter.
    Produces: `[level] [ModuleName] message\\n`

  - `format/2` — Erlang `:logger` handler formatter (for file handler).
    Produces: `YYYY-MM-DD HH:MM:SS.mmm [level] [ModuleName] message\\n`
  """

  @doc "Elixir Logger console format callback."
  @spec format(Logger.level(), Logger.message(), Logger.Formatter.time(), keyword()) ::
          IO.chardata()
  def format(level, message, _timestamp, metadata) do
    module = format_module(metadata[:mfa])
    ["[", Atom.to_string(level), "] ", module, IO.iodata_to_binary(message), "\n"]
  rescue
    _ -> ["[", Atom.to_string(level), "] ", IO.iodata_to_binary(message), "\n"]
  end

  @doc """
  Erlang `:logger` handler format callback.

  Called by `:logger_std_h` (or any Erlang handler) with a log event map
  and a formatter config. We ignore the config.
  """
  @spec format(map(), term()) :: IO.chardata()
  def format(%{level: level, msg: msg, meta: meta}, _config) do
    module = format_module(meta[:mfa])
    message = format_msg(msg)
    timestamp = format_timestamp(meta)
    [timestamp, " [", Atom.to_string(level), "] ", module, message, "\n"]
  rescue
    _ -> ["[log format error]\n"]
  end

  # --- Private ---

  defp format_module({module, _function, _arity}) do
    name = module |> Atom.to_string() |> strip_elixir_prefix()
    ["[", name, "] "]
  end

  defp format_module(_), do: ""

  defp strip_elixir_prefix("Elixir." <> rest), do: rest
  defp strip_elixir_prefix(name), do: name

  defp format_msg({:string, msg}), do: IO.chardata_to_string(msg)
  defp format_msg({:report, report}), do: inspect(report)
  defp format_msg({format, args}), do: :io_lib.format(format, args)

  defp format_timestamp(%{time: usec}) when is_integer(usec) do
    # microseconds since epoch
    {date, {h, m, s}} = :calendar.system_time_to_universal_time(usec, :microsecond)
    ms = div(rem(usec, 1_000_000), 1_000)
    {year, month, day} = date

    :io_lib.format("~4..0B-~2..0B-~2..0B ~2..0B:~2..0B:~2..0B.~3..0B", [
      year,
      month,
      day,
      h,
      m,
      s,
      ms
    ])
  end

  defp format_timestamp(_), do: ""
end
