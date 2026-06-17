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

defmodule ToastTest.Formatting.Backtrace do
  @moduledoc """
  Renders a thread's coredump frames as a human-readable backtrace string.

  Pure display: operates on the frame maps produced by
  `ToastTest.PostExecution.Enrichment.Coredump.analyze/3`. Shared by the live result
  formatting (`Formatting.Issues`) and the offline `mix toast.analyze` detail
  view.
  """

  @doc "Format a thread's frames as a human-readable backtrace string."
  def format_backtrace(frames) do
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
