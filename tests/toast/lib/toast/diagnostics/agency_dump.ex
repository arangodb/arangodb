defmodule Toast.Diagnostics.AgencyDump do
  @moduledoc """
  Capture agency state for cluster diagnostics.

  Queries agents directly via `/_api/agency/state`, trying each endpoint
  until one responds. The response is kept as raw JSON and written to disk.
  """

  require Logger

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

  # 1 MB — below this we keep raw JSON for easy inspection
  @compress_threshold 1_024 * 1_024

  @doc """
  Write agency dump JSON to a file in the given directory.

  The file is named `agency-dump-<deployment_id>.json`. If the content exceeds
  #{@compress_threshold} bytes, it is compressed (zstd or gzip) and the
  compressed path is returned.

  Returns `{:ok, path}` on success or `{:error, reason}` on failure.
  """
  @spec write(iodata(), Path.t(), String.t()) :: {:ok, Path.t()} | {:error, term()}
  def write(json, dir, deployment_id) do
    File.mkdir_p!(dir)
    json_path = Path.join(dir, "agency-dump-#{deployment_id}.json")

    File.write!(json_path, json)

    if IO.iodata_length(json) > @compress_threshold do
      compress_and_remove(json_path)
    else
      {:ok, json_path}
    end
  rescue
    e -> {:error, Exception.message(e)}
  end

  defp compress_and_remove(json_path) do
    ext = if Toast.Utils.Compression.zstd_available?(), do: ".zst", else: ".gz"

    case Toast.Utils.Compression.compress_file(json_path, json_path <> ext) do
      {:ok, compressed} ->
        File.rm(json_path)
        {:ok, compressed}

      {:error, _} ->
        {:ok, json_path}
    end
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
