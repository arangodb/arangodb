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

    # Put the crash thread first, then sort remaining threads by id.
    {crash, rest} =
      case crash_thread_id do
        nil -> {[], threads}
        id -> Enum.split_with(threads, &(&1.id == id))
      end

    threads = crash ++ Enum.sort_by(rest, &thread_sort_key/1)

    %{
      threads: threads,
      signal: report.signal,
      faulting_address: report.faulting_address,
      registers: report.registers,
      disassembly: report.disassembly,
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

  defp thread_sort_key(%{id: id}) do
    case Integer.parse(id) do
      {n, ""} -> {0, n}
      _ -> {1, id}
    end
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
