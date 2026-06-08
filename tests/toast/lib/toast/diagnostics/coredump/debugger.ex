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

defmodule Toast.Diagnostics.Coredump.Debugger do
  @moduledoc "Behaviour for coredump debugger backends."

  @type frame :: %{
          function: String.t(),
          file: String.t() | nil,
          line: integer() | nil
        }

  @type thread :: %{
          id: integer(),
          os_id: String.t() | nil,
          frames: [frame()]
        }

  @type result :: %{
          signal: String.t() | nil,
          faulting_address: String.t() | nil,
          registers: String.t() | nil,
          disassembly: String.t() | nil,
          threads: [thread()],
          crash_thread: integer() | nil
        }

  @doc "Return the debugger executable name."
  @callback executable() :: String.t()

  @doc "Build the command-line arguments for non-interactive stack trace extraction."
  @callback command(binary_path :: Path.t(), core_path :: Path.t()) :: [String.t()]

  @doc "Parse debugger output into a structured result."
  @callback parse_output(output :: String.t()) :: result()

  @internal_frames ~w(__libc_start_main __libc_start_call_main _start clone start_thread)
  @internal_prefixes ["__libc_", "__GI_"]

  @doc false
  @spec flush_current_thread(map()) :: map()
  def flush_current_thread(%{current: nil} = acc), do: acc

  def flush_current_thread(%{current: current} = acc) do
    thread = %{current | frames: Enum.reverse(current.frames)}
    %{acc | threads: [thread | acc.threads], current: nil}
  end

  @doc "Deduplicate threads with the same id, keeping the entry with more frames."
  @spec deduplicate_threads([thread()]) :: [thread()]
  def deduplicate_threads(threads) do
    threads
    |> Enum.group_by(& &1.id)
    |> Enum.map(fn {_id, entries} -> Enum.max_by(entries, &length(&1.frames)) end)
  end

  @doc false
  @spec filter_threads([thread()], integer() | nil) :: [thread()]
  def filter_threads(threads, crash_thread) do
    Enum.map(threads, fn
      %{id: ^crash_thread} = thread ->
        thread

      thread ->
        %{thread | frames: Enum.reject(thread.frames, &internal_frame?/1)}
    end)
  end

  defp internal_frame?(%{function: func}) do
    func in @internal_frames ||
      Enum.any?(@internal_prefixes, &String.starts_with?(func, &1))
  end
end
