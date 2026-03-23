defmodule ToastTest.Enrichment.Coredump do
  @moduledoc """
  Enrichment wrapper around `Toast.Diagnostics.Coredump.analyze/3`.

  Transforms the debugger `Report` struct into the structured shape
  stored in `SuiteResult.coredumps`.
  """

  alias Toast.Deployment.ServerInstance
  alias Toast.Diagnostics.Coredump, as: DiagCoredump

  @doc """
  Analyze a coredump for the given server instance.

  Options:
    - `:analyzer` — override the analysis function (for testing);
      signature `(core_path, binary_path, opts) -> {:ok, Report.t()} | {:error, term()}`
    - All other options are forwarded to the analyzer.
  """
  def analyze(core_path, server, opts \\ []) do
    case extract_binary_path(server) do
      {:ok, binary_path} ->
        {analyzer, forward_opts} = Keyword.pop(opts, :analyzer, &DiagCoredump.analyze/3)
        run_analysis(analyzer, core_path, binary_path, forward_opts)

      :error ->
        {:error, :no_executable}
    end
  end

  @doc "Format a thread's frames as a human-readable backtrace string."
  def format_backtrace(frames) do
    frames
    |> Enum.with_index()
    |> Enum.map_join("\n", fn {frame, idx} -> format_frame(frame, idx) end)
  end

  defp extract_binary_path(%ServerInstance{launch_spec: nil}), do: :error
  defp extract_binary_path(%ServerInstance{launch_spec: spec}), do: {:ok, spec.executable}

  defp run_analysis(analyzer, core_path, binary_path, opts) do
    case analyzer.(core_path, binary_path, opts) do
      {:ok, report} -> {:ok, transform_report(report)}
      {:error, _} = err -> err
    end
  end

  defp transform_report(report) do
    threads = Enum.map(report.threads, &transform_thread/1)
    crash_thread_id = if report.crash_thread, do: to_string(report.crash_thread)

    # Put the crash thread first so the summary prints the most relevant backtrace.
    threads =
      case crash_thread_id do
        nil ->
          threads

        id ->
          {crash, rest} = Enum.split_with(threads, &(&1.id == id))
          crash ++ rest
      end

    %{
      threads: threads,
      signal: report.signal,
      faulting_address: report.faulting_address,
      crash_thread: crash_thread_id,
      debugger: report.debugger
    }
  end

  defp transform_thread(thread) do
    %{
      id: to_string(thread.id),
      os_id: Map.get(thread, :os_id),
      frames: thread.frames
    }
  end

  defp format_frame(frame, idx) do
    location =
      case {frame[:file], frame[:line]} do
        {nil, _} -> ""
        {file, nil} -> " at #{file}"
        {file, line} -> " at #{file}:#{line}"
      end

    "##{idx} #{frame.function}#{location}"
  end
end
