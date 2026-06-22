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

defmodule ToastTest.Formatting.AgencyLog do
  @moduledoc """
  Display formatting for agency-dump log entries.

  Each entry is a raft log entry from `/_api/agency/state`. The one-line
  summary carries the raft term, log index, and submitting client; the detail
  is the full `request` transaction rendered as JSON, truncated to a body
  limit (mirroring traffic body rendering).
  """

  alias ToastTest.Formatting.Utils

  @default_body_limit 500

  @doc """
  Return `{summary, detail | nil}` for an agency log entry.

  `opts` may carry `:limit` — the max characters of rendered request to show
  (a positive integer or `:unlimited`); defaults to #{@default_body_limit}.
  `detail` is `nil` when the entry has no request transaction.
  """
  @spec format_entry(map(), map()) :: {String.t(), String.t() | nil}
  def format_entry(entry, opts \\ %{}) do
    {format_summary(entry), format_request(entry, opts)}
  end

  defp format_summary(entry) do
    [term_part(entry["term"]), index_part(entry["_key"]), client_part(entry["clientId"])]
    |> Enum.reject(&is_nil/1)
    |> Enum.join(" ")
  end

  defp term_part(term) when is_integer(term), do: "term=#{term}"
  defp term_part(_), do: nil

  defp index_part(key) when is_binary(key) do
    case Integer.parse(key) do
      {index, _} -> "##{index}"
      :error -> nil
    end
  end

  defp index_part(_), do: nil

  defp client_part(client) when is_binary(client) and client != "",
    do: "client=#{String.slice(client, 0, 8)}"

  defp client_part(_), do: nil

  defp format_request(%{"request" => request}, opts)
       when is_map(request) and map_size(request) > 0 do
    limit = Map.get(opts, :limit, @default_body_limit)
    json = Jason.encode!(request, pretty: true)

    {text, truncated} = Utils.truncate(json, limit)
    suffix = if truncated, do: " …", else: ""
    Utils.indent(text <> suffix)
  end

  defp format_request(_entry, _opts), do: nil
end
