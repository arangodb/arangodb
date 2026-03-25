defmodule Toast.Deployment.Config do
  @moduledoc """
  Configuration for a deployment — what the deployment infrastructure needs
  to build and manage ArangoDB server processes.

  Constructed via `new/1` which reads defaults from application env
  (populated by `Toast.Env.load/1`) and applies optional overrides.

  The `cluster` field determines the deployment mode:
  - `nil` → single server
  - `%ClusterOpts{}` → cluster with the given topology
  """

  alias Toast.Deployment.ClusterOpts

  @type server_args_map :: %{String.t() => String.t() | [String.t()]}

  @type t :: %__MODULE__{
          build_dir: Path.t() | nil,
          show_server_logs: boolean(),
          server_args: server_args_map(),
          active_sanitizers: MapSet.t(String.t()),
          sanitizer_override: atom() | nil,
          timeout_factor: number(),
          startup_timeout: pos_integer(),
          shutdown_timeout: pos_integer(),
          api_version: non_neg_integer() | String.t() | nil,
          cluster: ClusterOpts.t() | nil
        }

  defstruct build_dir: nil,
            show_server_logs: false,
            server_args: %{},
            active_sanitizers: MapSet.new(),
            sanitizer_override: nil,
            timeout_factor: 1,
            startup_timeout: 60_000,
            shutdown_timeout: 60_000,
            api_version: nil,
            cluster: nil

  @doc """
  Build a deployment config from application env with optional overrides.

  ## Cluster mode

  Pass `cluster: true` to build a cluster config with defaults from app env,
  or `cluster: [dbservers: 3, coordinators: 2]` to override specific topology.
  Omit or pass `cluster: nil` for single server mode.

  ## Examples

      # Single server, all defaults:
      Config.new()

      # Cluster with defaults:
      Config.new(cluster: true)

      # Cluster with custom topology:
      Config.new(cluster: [dbservers: 3, coordinators: 2])

      # Override server args:
      Config.new(server_args: %{"log.level" => "debug"})
  """
  @spec new(keyword()) :: t()
  def new(overrides \\ []) do
    {cluster_opt, overrides} = Keyword.pop(overrides, :cluster)

    # Read each field from app env, falling back to the struct default.
    # The :cluster field is special — built from ClusterOpts, not a direct env read.
    base =
      Enum.reduce(Map.from_struct(%__MODULE__{}), %__MODULE__{}, fn
        {:cluster, _default}, acc -> Map.put(acc, :cluster, build_cluster_opts(cluster_opt))
        {key, default}, acc -> Map.put(acc, key, Application.get_env(:toast, key, default))
      end)

    struct!(base, overrides)
  end

  @doc "Returns true if this is a cluster configuration."
  @spec cluster?(t()) :: boolean()
  def cluster?(%__MODULE__{cluster: nil}), do: false
  def cluster?(%__MODULE__{cluster: %ClusterOpts{}}), do: true

  @doc "Returns the deployment mode atom."
  @spec mode(t()) :: :single_server | :cluster
  def mode(%__MODULE__{cluster: nil}), do: :single_server
  def mode(%__MODULE__{cluster: %ClusterOpts{}}), do: :cluster

  defp build_cluster_opts(nil), do: nil
  defp build_cluster_opts(false), do: nil
  defp build_cluster_opts(true), do: ClusterOpts.new()
  defp build_cluster_opts(overrides) when is_list(overrides), do: ClusterOpts.new(overrides)
end
