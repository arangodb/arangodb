defmodule Toast.Deployment.Config do
  @moduledoc """
  Configuration for a deployment — what the deployment infrastructure (Toast)
  needs to build and manage ArangoDB server processes.

  This is intentionally separate from `ToastTest.Config`, which holds
  test-execution concerns (timeouts, diagnostics, CI settings). The split
  reflects the layering: Toast is reusable infrastructure; ToastTest is the
  test runner built on top of it. In an interactive session, only this config
  is needed — ToastTest is never involved.

  Constructed via `new/1` which reads defaults from application env
  (populated by `Toast.Env.load/1`) and applies optional overrides.
  This means env vars and CLI flags set the defaults, but callers can
  override any field when starting a deployment explicitly.

  The `cluster` field determines the deployment mode:
  - `nil` → single server
  - `%ClusterOpts{}` → cluster with the given topology
  """

  alias Toast.Deployment.ClusterOpts

  @type server_args_map :: %{String.t() => String.t() | [String.t()]}

  @type jwt_algorithm :: :hmac | :ecdsa

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
          cluster: ClusterOpts.t() | nil,
          memory_budget: pos_integer() | nil,
          rr: MapSet.t(atom()) | nil,
          rr_path: Path.t() | nil,
          protocol: :http1 | :http2,
          authentication: boolean(),
          jwt_algorithm: jwt_algorithm(),
          ssl: boolean()
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
            cluster: nil,
            memory_budget: nil,
            rr: nil,
            rr_path: nil,
            protocol: :http1,
            authentication: false,
            jwt_algorithm: :hmac,
            ssl: false

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

    base = Toast.Env.struct_from_env(%__MODULE__{})
    config = struct!(%{base | cluster: build_cluster_opts(cluster_opt)}, overrides)
    validate_auth!(config)
    config
  end

  # `jwt_algorithm` is only meaningful when `authentication: true`. An algorithm
  # set without auth enabled would silently go unused — unlike other bad values
  # on this struct, which crash loudly at the point of use. Reject here.
  defp validate_auth!(%__MODULE__{authentication: false, jwt_algorithm: alg}) when alg != :hmac do
    raise ArgumentError, "jwt_algorithm: #{inspect(alg)} requires authentication: true"
  end

  defp validate_auth!(%__MODULE__{}), do: :ok

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
