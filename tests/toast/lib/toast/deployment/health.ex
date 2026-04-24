defmodule Toast.Deployment.Health do
  @moduledoc """
  HTTP health check polling for ArangoDB servers.

  Health checks always use HTTP/1, regardless of the configured client protocol.
  They are simple liveness pings against `/_api/version` — they don't need
  protocol negotiation, and using HTTP/1 avoids introducing a dependency on
  the application protocol setting into the deployment infrastructure layer.
  """

  require Logger

  alias Toast.Utils.Polling

  @type auth_opt :: {:jwt, String.t()} | nil

  @type wait_opts :: [
          timeout: pos_integer(),
          poll_interval: pos_integer(),
          http_timeout: pos_integer(),
          process_check_fn: (-> boolean()),
          auth: auth_opt()
        ]

  @spec wait_for_agency_ready([String.t()], keyword()) :: :ok | {:error, term()}
  def wait_for_agency_ready(agent_endpoints, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, 30_000)
    poll_interval = Keyword.get(opts, :poll_interval, 200)
    deadline = System.monotonic_time(:millisecond) + timeout

    Logger.debug("Waiting for agency consensus across #{length(agent_endpoints)} agents")

    probe = fn -> agency_probe(agent_endpoints, opts) end

    with {:ok, :ready} <- Polling.poll_until(probe, deadline, poll_interval), do: :ok
  end

  @spec wait_until_ready(String.t(), wait_opts()) :: :ok | {:error, term()}
  def wait_until_ready(endpoint, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, 60_000)
    poll_interval = Keyword.get(opts, :poll_interval, 500)
    process_check_fn = Keyword.get(opts, :process_check_fn)
    deadline = System.monotonic_time(:millisecond) + timeout

    Logger.debug("Waiting for #{endpoint} to become ready")

    probe = fn -> readiness_probe(endpoint, opts, process_check_fn) end

    with {:ok, result} <- Polling.poll_until(probe, deadline, poll_interval), do: result
  end

  @spec check_once(String.t(), keyword()) :: :ok | {:error, term()}
  def check_once(endpoint, opts \\ []) do
    http_timeout = Keyword.get(opts, :http_timeout, 2_000)
    url = endpoint <> "/_api/version"

    req_opts =
      [receive_timeout: http_timeout, pool_timeout: http_timeout, retry: false]
      |> put_auth_header(opts)
      |> put_ssl_opts(endpoint)

    case Req.get(url, req_opts) do
      {:ok, %{status: status}} when status in 200..299 ->
        Logger.debug("#{endpoint} responded with status #{status}")
        :ok

      {:ok, %{status: status}} ->
        {:error, {:unexpected_status, status}}

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp agency_probe(agent_endpoints, opts) do
    results =
      agent_endpoints
      |> Task.async_stream(
        &check_agency_config(&1, opts),
        timeout: :timer.seconds(10),
        ordered: true
      )
      |> Enum.map(fn
        {:ok, result} -> result
        {:exit, _reason} -> {:error, :timeout}
      end)

    case analyze_agency_status(results) do
      :ready ->
        Logger.debug("Agency consensus reached")
        {:done, :ready}

      {:not_ready, reason} ->
        Logger.debug("Agency not ready: #{inspect(reason)}, retrying...")
        :not_ready
    end
  end

  defp check_agency_config(endpoint, opts) do
    http_timeout = Keyword.get(opts, :http_timeout, 2_000)
    url = endpoint <> "/_api/agency/config"

    req_opts =
      [receive_timeout: http_timeout, pool_timeout: http_timeout, retry: false]
      |> put_auth_header(opts)
      |> put_ssl_opts(endpoint)

    case Req.get(url, req_opts) do
      {:ok, %{status: status, body: body}} when status in 200..299 and is_map(body) ->
        {:ok, body}

      {:ok, %{status: status}} ->
        {:error, {:unexpected_status, status}}

      {:error, reason} ->
        {:error, reason}
    end
  end

  # Agents reject Basic auth, so a superuser JWT is the only option for
  # framework-internal polling under `authentication: true`.
  defp put_auth_header(req_opts, opts) do
    case Keyword.get(opts, :auth) do
      nil -> req_opts
      {:jwt, token} -> Keyword.put(req_opts, :headers, [{"authorization", "Bearer #{token}"}])
    end
  end

  # Self-signed test certs require disabling certificate verification.
  defp put_ssl_opts(req_opts, "https://" <> _) do
    Keyword.put(req_opts, :connect_options, transport_opts: [verify: :verify_none])
  end

  defp put_ssl_opts(req_opts, _endpoint), do: req_opts

  defp analyze_agency_status(results) do
    configs = for {:ok, body} <- results, do: body

    if length(configs) != length(results) do
      {:not_ready, :not_all_responding}
    else
      leader_ids =
        for config <- configs,
            id = config["leaderId"],
            is_binary(id) and id != "",
            do: id

      has_last_acked = Enum.any?(configs, &Map.has_key?(&1, "lastAcked"))

      cond do
        length(leader_ids) != length(configs) ->
          {:not_ready, :missing_leader_id}

        not has_last_acked ->
          {:not_ready, :no_last_acked}

        length(Enum.uniq(leader_ids)) != 1 ->
          {:not_ready, :leader_disagreement}

        true ->
          :ready
      end
    end
  end

  defp readiness_probe(endpoint, opts, process_check_fn) do
    if process_check_fn && not process_check_fn.() do
      Logger.debug("#{endpoint}: OS process is no longer running")
      {:done, {:error, :process_died}}
    else
      case check_once(endpoint, opts) do
        :ok ->
          Logger.debug("#{endpoint} is ready")
          {:done, :ok}

        {:error, _reason} ->
          :not_ready
      end
    end
  end
end
