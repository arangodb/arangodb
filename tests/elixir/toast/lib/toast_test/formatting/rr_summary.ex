defmodule ToastTest.Formatting.RrSummary do
  @moduledoc false

  import ToastTest.Formatting

  @doc """
  Print rr recording summary if any rr traces were captured.

  Scans `base_dir` recursively for `rr-trace` directories and prints
  their paths with replay commands.
  """
  @spec print(Path.t() | nil) :: :ok
  def print(nil), do: :ok

  def print(base_dir) do
    traces = find_rr_traces(base_dir)

    if traces != [] do
      colors = IO.ANSI.enabled?()
      bar = String.duplicate("\u2500", 80)

      IO.puts("")
      IO.puts(colorize(bar, :blue, colors))
      IO.puts(colorize(" rr RECORDINGS", :blue, colors))
      IO.puts("")

      for {server_id, trace_dir} <- traces do
        IO.puts("  #{colorize(server_id, :cyan, colors)}")
        IO.puts("    Trace: #{trace_dir}")
        IO.puts("    Replay: rr replay #{trace_dir}")
        IO.puts("")
      end
    end

    :ok
  end

  defp find_rr_traces(base_dir) do
    # Match both single-server (base_dir/server/rr-trace) and
    # cluster (base_dir/deployment/server/rr-trace) layouts.
    base_dir
    |> Path.join("**/rr-trace")
    |> Path.wildcard()
    |> Enum.sort()
    |> Enum.map(fn trace_dir ->
      server_id = trace_dir |> Path.dirname() |> Path.basename()
      {server_id, trace_dir}
    end)
  end
end
