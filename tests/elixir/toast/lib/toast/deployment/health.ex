defmodule Toast.Deployment.Health do
  @moduledoc "HTTP health check polling for ArangoDB servers."

  require Logger

  @type wait_opts :: [
          timeout: pos_integer(),
          poll_interval: pos_integer(),
          http_timeout: pos_integer(),
          process_check_fn: (-> boolean())
        ]

  @spec wait_for_agency_ready([String.t()], keyword()) :: :ok | {:error, term()}
  def wait_for_agency_ready(agent_endpoints, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, 30_000)
    poll_interval = Keyword.get(opts, :poll_interval, 200)
    deadline = System.monotonic_time(:millisecond) + timeout

    Logger.debug("Waiting for agency consensus across #{length(agent_endpoints)} agents")

    agency_poll_loop(agent_endpoints, opts, poll_interval, deadline)
  end

  @spec wait_until_ready(String.t(), wait_opts()) :: :ok | {:error, term()}
  def wait_until_ready(endpoint, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, 60_000)
    poll_interval = Keyword.get(opts, :poll_interval, 500)
    process_check_fn = Keyword.get(opts, :process_check_fn)
    deadline = System.monotonic_time(:millisecond) + timeout

    Logger.debug("Waiting for #{endpoint} to become ready")
    poll_loop(endpoint, opts, process_check_fn, poll_interval, deadline, _first_attempt = true)
  end

  @spec check_once(String.t(), keyword()) :: :ok | {:error, term()}
  def check_once(endpoint, opts \\ []) do
    http_timeout = Keyword.get(opts, :http_timeout, 2_000)
    url = endpoint <> "/_api/version"

    case Req.get(url, receive_timeout: http_timeout, pool_timeout: http_timeout, retry: false) do
      {:ok, %{status: status}} when status in 200..299 ->
        Logger.debug("#{endpoint} responded with status #{status}")
        :ok

      {:ok, %{status: status}} ->
        {:error, {:unexpected_status, status}}

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp agency_poll_loop(agent_endpoints, opts, poll_interval, deadline) do
    results = Enum.map(agent_endpoints, &check_agency_config(&1, opts))

    case analyze_agency_status(results) do
      :ready ->
        Logger.debug("Agency consensus reached")
        :ok

      {:not_ready, reason} ->
        now = System.monotonic_time(:millisecond)

        if now >= deadline do
          {:error, :timeout}
        else
          Logger.debug("Agency not ready: #{inspect(reason)}, retrying...")
          Process.sleep(poll_interval)
          agency_poll_loop(agent_endpoints, opts, poll_interval, deadline)
        end
    end
  end

  defp check_agency_config(endpoint, opts) do
    http_timeout = Keyword.get(opts, :http_timeout, 2_000)
    url = endpoint <> "/_api/agency/config"

    case Req.get(url, receive_timeout: http_timeout, pool_timeout: http_timeout, retry: false) do
      {:ok, %{status: status, body: body}} when status in 200..299 and is_map(body) ->
        {:ok, body}

      {:ok, %{status: status}} ->
        {:error, {:unexpected_status, status}}

      {:error, reason} ->
        {:error, reason}
    end
  end

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

        !has_last_acked ->
          {:not_ready, :no_last_acked}

        MapSet.size(MapSet.new(leader_ids)) != 1 ->
          {:not_ready, :leader_disagreement}

        true ->
          :ready
      end
    end
  end

  defp poll_loop(endpoint, opts, process_check_fn, poll_interval, deadline, first_attempt) do
    if process_check_fn && !process_check_fn.() do
      Logger.debug("#{endpoint}: OS process is no longer running")
      {:error, :process_died}
    else
      case check_once(endpoint, opts) do
        :ok ->
          Logger.debug("#{endpoint} is ready")
          :ok

        {:error, reason} ->
          handle_check_failure(
            endpoint,
            reason,
            opts,
            process_check_fn,
            poll_interval,
            deadline,
            first_attempt
          )
      end
    end
  end

  defp handle_check_failure(
         endpoint,
         reason,
         opts,
         process_check_fn,
         poll_interval,
         deadline,
         first_attempt
       ) do
    now = System.monotonic_time(:millisecond)
    remaining = max(0, deadline - now)

    if first_attempt do
      Logger.debug(
        "#{endpoint}: first check failed (#{inspect(reason)}), #{remaining}ms remaining"
      )
    end

    if now >= deadline do
      Logger.debug("#{endpoint}: timed out waiting for ready")
      {:error, :timeout}
    else
      Process.sleep(poll_interval)
      poll_loop(endpoint, opts, process_check_fn, poll_interval, deadline, false)
    end
  end
end
