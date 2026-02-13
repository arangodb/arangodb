defmodule Toast.Deployment do
  @moduledoc "Start and stop ArangoDB deployments for testing."

  alias Toast.Config
  alias Toast.Deployment.Controller

  @type t :: %__MODULE__{
          id: String.t(),
          mode: :single_server,
          endpoint: String.t(),
          controller: pid()
        }

  @enforce_keys [:id, :mode, :endpoint, :controller]
  defstruct [:id, :mode, :endpoint, :controller]

  @spec start(atom(), keyword()) :: {:ok, t()} | {:error, term()}
  def start(mode \\ :single_server, opts \\ [])

  def start(:single_server, opts) do
    config = Config.load(opts)

    controller_opts = [config: config] ++ Keyword.take(opts, [:id])

    with {:ok, pid} <- Toast.Deployment.Supervisor.start_controller(controller_opts),
         :ok <- Controller.deploy(pid, config.startup_timeout) do
      info = Controller.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.id,
         mode: :single_server,
         endpoint: info.endpoint,
         controller: pid
       }}
    else
      {:error, reason} = error ->
        cleanup_failed_controller(reason, opts)
        error
    end
  end

  def start(mode, _opts) do
    {:error, {:unsupported_mode, mode}}
  end

  @spec stop(t(), keyword()) :: :ok | {:error, term()}
  def stop(%__MODULE__{controller: pid}, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, 30_000)

    with :ok <- Controller.shutdown(pid, timeout) do
      DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
      :ok
    end
  end

  defp cleanup_failed_controller(_reason, _opts) do
    # Controller may have already been cleaned up during deploy failure rollback.
    # If it's still alive under the supervisor, it will be cleaned up when the
    # supervisor shuts down (test suite end).
    :ok
  end
end
