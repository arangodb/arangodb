defmodule ToastTest.Enrichment.Coredump do
  @moduledoc """
  Enrichment wrapper around `Toast.Diagnostics.Coredump.analyze/3`.

  Transforms the debugger `Report` struct into the flat thread-list
  shape expected by `SuiteResult` issue details.
  """

  alias Toast.Deployment.ServerInstance
  alias Toast.Diagnostics.Coredump, as: DiagCoredump

  @type thread :: %{thread_id: String.t(), name: String.t() | nil, backtrace: String.t()}
  @type result :: %{threads: [thread()], signal: String.t() | nil}

  @doc """
  Analyze a coredump for the given server instance.

  Options:
    - `:analyzer` — override the analysis function (for testing);
      signature `(core_path, binary_path, opts) -> {:ok, Report.t()} | {:error, term()}`
    - All other options are forwarded to the analyzer.
  """
  @spec analyze(Path.t(), ServerInstance.t(), keyword()) :: {:ok, result()} | {:error, term()}
  def analyze(core_path, server, opts \\ []) do
    case extract_binary_path(server) do
      {:ok, binary_path} ->
        {analyzer, forward_opts} = Keyword.pop(opts, :analyzer, &DiagCoredump.analyze/3)
        run_analysis(analyzer, core_path, binary_path, forward_opts)

      :error ->
        {:error, :no_executable}
    end
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

    # Put the crash thread first so the summary prints the most relevant backtrace.
    threads =
      case report.crash_thread do
        nil ->
          threads

        crash_id ->
          crash_id_str = to_string(crash_id)
          {crash, rest} = Enum.split_with(threads, &(&1.thread_id == crash_id_str))
          crash ++ rest
      end

    %{threads: threads, signal: report.signal}
  end

  defp transform_thread(thread) do
    %{
      thread_id: to_string(thread.id),
      name: Map.get(thread, :name),
      backtrace: format_backtrace(thread.frames)
    }
  end

  defp format_backtrace(frames) do
    frames
    |> Enum.with_index()
    |> Enum.map_join("\n", fn {frame, idx} -> format_frame(frame, idx) end)
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
