defmodule Toast.Deployment.Health do
  @moduledoc "HTTP health check polling for ArangoDB servers."

  require Logger

  @type wait_opts :: [
          timeout: pos_integer(),
          poll_interval: pos_integer(),
          http_timeout: pos_integer(),
          process_check_fn: (-> boolean())
        ]

  @spec wait_until_ready(String.t(), wait_opts()) :: :ok | {:error, term()}
  def wait_until_ready(endpoint, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, 60_000)
    poll_interval = Keyword.get(opts, :poll_interval, 500)
    deadline = System.monotonic_time(:millisecond) + timeout

    Logger.debug("[Toast.Health] Waiting for #{endpoint} to become ready")
    poll_loop(endpoint, opts, poll_interval, deadline, _first_attempt = true)
  end

  @spec check_once(String.t(), keyword()) :: :ok | {:error, term()}
  def check_once(endpoint, opts \\ []) do
    http_timeout = Keyword.get(opts, :http_timeout, 2_000)
    url = endpoint <> "/_api/version"

    case Req.get(url, receive_timeout: http_timeout, pool_timeout: http_timeout, retry: false) do
      {:ok, %{status: status}} when status in 200..299 -> :ok
      {:ok, %{status: status}} -> {:error, {:unexpected_status, status}}
      {:error, reason} -> {:error, reason}
    end
  end

  defp poll_loop(endpoint, opts, poll_interval, deadline, first_attempt) do
    process_check_fn = Keyword.get(opts, :process_check_fn)

    if process_check_fn && !process_check_fn.() do
      {:error, :process_died}
    else
      case check_once(endpoint, opts) do
        :ok ->
          Logger.debug("[Toast.Health] #{endpoint} is ready")
          :ok

        {:error, reason} ->
          if first_attempt do
            Logger.debug("[Toast.Health] First check for #{endpoint} failed: #{inspect(reason)}")
          end

          now = System.monotonic_time(:millisecond)

          if now >= deadline do
            {:error, :timeout}
          else
            Process.sleep(poll_interval)
            poll_loop(endpoint, opts, poll_interval, deadline, false)
          end
      end
    end
  end
end
