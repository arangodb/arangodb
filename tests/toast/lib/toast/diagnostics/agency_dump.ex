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

defmodule Toast.Diagnostics.AgencyDump do
  @moduledoc """
  Capture agency state for cluster diagnostics.

  Queries agents directly via `/_api/agency/state`, trying each endpoint
  until one responds. The response is kept as raw JSON and written to disk.
  """

  require Logger

  defmodule InvalidDumpError do
    @moduledoc false
    defexception message: "agency dump log entry is missing a numeric epoch_millis"
  end

  @doc """
  Capture agency dump from an agent.

  Tries each endpoint in order until one succeeds. Returns the raw JSON
  response body or nil if all endpoints fail.

  ## Options

    * `:endpoints` - list of agent endpoint URLs
    * `:timeout` - request timeout in ms (default: 30_000)
    * `:auth` - authentication tuple, e.g. `{:jwt, token}` (default: nil)
    * `:client_opts` - extra options forwarded to `Toast.Client.new/2` (test use)

  """
  @spec capture(keyword()) :: binary() | nil
  def capture(opts) do
    endpoints = Keyword.get(opts, :endpoints, [])
    timeout = Keyword.get(opts, :timeout, 30_000)
    auth = Keyword.get(opts, :auth)
    client_opts = Keyword.get(opts, :client_opts, [])

    case endpoints do
      [] ->
        Logger.warning("AgencyDump: no agent endpoints available")
        nil

      _ ->
        try_endpoints(endpoints, timeout, auth, client_opts)
    end
  end

  @doc """
  Fetch agency state from the deployment and write it to `result_dir`.

  Returns `{:ok, path}` when a dump was written, or `:error` when the
  deployment is not a cluster, the agents are unreachable, or the write fails.
  """
  @spec try_collect(Toast.Deployment.t(), Path.t()) :: {:ok, Path.t()} | :error
  def try_collect(deployment, result_dir) do
    case Toast.Deployment.dump_agency(deployment) do
      {:ok, json} when json != nil ->
        case write(json, result_dir, deployment.id) do
          {:ok, path} ->
            Logger.info("Agency dump written to #{path}")
            {:ok, path}

          {:error, reason} ->
            Logger.warning("Failed to write agency dump: #{inspect(reason)}")
            :error
        end

      {:ok, nil} ->
        Logger.warning("Agency dump returned nil (no responsive agents?)")
        :error

      {:error, reason} ->
        Logger.debug("Agency dump skipped: #{inspect(reason)}")
        :error
    end
  rescue
    e ->
      Logger.warning("Agency dump failed: #{Exception.message(e)}")
      :error
  end

  @doc """
  Read an agency dump file and return its `"log"` entries.

  Each returned entry carries the original fields plus a `"time_us"` field
  (`epoch_millis` converted to microseconds), kept in the dump's own order
  (already ascending by time).
  """
  @spec extract_log_entries_from_file(Path.t()) :: {:ok, [map()]} | {:error, term()}
  def extract_log_entries_from_file(path) do
    with {:ok, json} <- File.read(path),
         {:ok, %{} = decoded} <- decode_json(json) do
      {:ok, Enum.map(Map.get(decoded, "log", []), &stamp_time_us/1)}
    else
      {:ok, _non_object} -> {:error, :not_an_agency_dump}
      {:error, _} = error -> error
    end
  rescue
    InvalidDumpError -> {:error, :invalid_log_entry}
  end

  defp decode_json(json) do
    {:ok, :json.decode(json)}
  rescue
    _ -> {:error, :invalid_json}
  end

  # A valid dump has a numeric `epoch_millis` on every entry; anything else is
  # a corrupt dump (rare), so we let the mismatch raise rather than paying a
  # validation pass on every normal run.
  defp stamp_time_us(%{"epoch_millis" => ms} = entry) when is_number(ms),
    do: Map.put(entry, "time_us", round(ms * 1_000))

  defp stamp_time_us(_entry), do: raise(InvalidDumpError)

  @doc """
  Write agency dump JSON to a file in the given directory.

  The file is named `agency-dump-<deployment_id>.json` and written
  uncompressed so post-execution can read it directly; CI artifact
  compression is handled separately by `ToastTest.ResultPackaging`.

  Returns `{:ok, path}` on success or `{:error, reason}` on failure.
  """
  @spec write(iodata(), Path.t(), String.t()) :: {:ok, Path.t()} | {:error, term()}
  def write(json, dir, deployment_id) do
    File.mkdir_p!(dir)
    json_path = Path.join(dir, "agency-dump-#{deployment_id}.json")
    File.write!(json_path, json)
    {:ok, json_path}
  rescue
    e -> {:error, Exception.message(e)}
  end

  # --- Capture internals ---

  defp try_endpoints([], _timeout, _auth, _client_opts) do
    Logger.warning("AgencyDump: no agent responded")
    nil
  end

  defp try_endpoints([endpoint | rest], timeout, auth, client_opts) do
    req_opts = [{:receive_timeout, timeout} | client_opts]
    req_opts = if auth, do: [{:auth, auth} | req_opts], else: req_opts
    client = Toast.Client.new(endpoint, req_opts)

    case Toast.Client.get(client, "/_api/agency/state", decode_body: false) do
      {:ok, %{status: 200, body: body}} when is_binary(body) ->
        body

      {:ok, %{status: 200, body: body}} ->
        IO.iodata_to_binary(:json.encode(body))

      {:ok, %{status: status}} ->
        Logger.warning("AgencyDump: #{endpoint} returned status #{status}")
        try_endpoints(rest, timeout, auth, client_opts)

      {:error, reason} ->
        Logger.warning("AgencyDump: #{endpoint} failed: #{inspect(reason)}")
        try_endpoints(rest, timeout, auth, client_opts)
    end
  rescue
    e ->
      Logger.warning("AgencyDump: #{endpoint} error: #{Exception.message(e)}")
      try_endpoints(rest, timeout, auth, client_opts)
  end
end
