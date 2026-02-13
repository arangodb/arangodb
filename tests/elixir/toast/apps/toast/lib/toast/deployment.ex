defmodule Toast.Deployment do
  @moduledoc "Start and stop ArangoDB deployments for testing."

  alias Toast.Config
  alias Toast.Deployment.{Controller, ClusterController}

  @type server_info :: %{role: atom(), port: pos_integer(), endpoint: String.t()}

  @type t :: %__MODULE__{
          id: String.t(),
          mode: :single_server | :cluster,
          endpoint: String.t(),
          controller: pid(),
          servers: %{String.t() => server_info()} | nil
        }

  @enforce_keys [:id, :mode, :endpoint, :controller]
  defstruct [:id, :mode, :endpoint, :controller, :servers]

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
      {:error, _reason} = error ->
        error
    end
  end

  def start(:cluster, opts) do
    config = Config.load(opts)

    controller_opts = [config: config] ++ Keyword.take(opts, [:id])

    with {:ok, pid} <- Toast.Deployment.Supervisor.start_cluster_controller(controller_opts),
         :ok <- ClusterController.deploy(pid, config.startup_timeout) do
      info = ClusterController.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.id,
         mode: :cluster,
         endpoint: info.coordinator_endpoint,
         controller: pid,
         servers: info.servers
       }}
    else
      {:error, _reason} = error ->
        error
    end
  end

  def start(mode, _opts) do
    {:error, {:unsupported_mode, mode}}
  end

  @spec stop(t(), keyword()) :: :ok | {:error, term()}
  def stop(deployment, opts \\ [])

  def stop(%__MODULE__{mode: :single_server, controller: pid}, opts) do
    timeout = Keyword.get(opts, :timeout, 30_000)

    with :ok <- Controller.shutdown(pid, timeout) do
      DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
      :ok
    end
  end

  def stop(%__MODULE__{mode: :cluster, controller: pid}, opts) do
    timeout = Keyword.get(opts, :timeout, 60_000)

    with :ok <- ClusterController.shutdown(pid, timeout) do
      DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
      :ok
    end
  end
end
