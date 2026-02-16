defmodule Toast.Config do
  @moduledoc "Framework configuration from TOAST_* environment variables."

  require Logger

  @type t :: %__MODULE__{
          build_dir: Path.t() | nil,
          work_dir: Path.t(),
          result_dir: Path.t(),
          deployment_mode: :single_server | :cluster,
          show_server_logs: boolean(),
          server_args: %{String.t() => String.t() | [String.t()]},
          startup_timeout: pos_integer(),
          shutdown_timeout: pos_integer(),
          cluster_agents: pos_integer(),
          cluster_dbservers: pos_integer(),
          cluster_coordinators: pos_integer(),
          cluster_replication_factor: pos_integer(),
          explicit_sanitizer: String.t() | nil,
          sanitizer: MapSet.t(String.t())
        }

  @default_result_dir "toast-results"

  defstruct [
    :build_dir,
    :work_dir,
    result_dir: @default_result_dir,
    deployment_mode: :single_server,
    show_server_logs: false,
    server_args: %{},
    startup_timeout: 60_000,
    shutdown_timeout: 30_000,
    cluster_agents: 3,
    cluster_dbservers: 3,
    cluster_coordinators: 1,
    cluster_replication_factor: 2,
    explicit_sanitizer: nil,
    sanitizer: MapSet.new()
  ]

  @spec load() :: t()
  def load, do: load([])

  @spec load(keyword()) :: t()
  def load(opts) do
    build_dir = opt_or(opts, :build_dir, env("TOAST_BUILD_DIR"))

    explicit_sanitizer =
      opt_or(opts, :explicit_sanitizer, env("TOAST_SANITIZER")) ||
        Toast.Diagnostics.Sanitizer.detect_from_build_dir(build_dir)

    config = %__MODULE__{
      build_dir: build_dir,
      work_dir: opt_or(opts, :work_dir, env("TOAST_WORK_DIR")) || default_work_dir(),
      result_dir: opt_or(opts, :result_dir, env("TOAST_RESULT_DIR")) || @default_result_dir,
      deployment_mode: opt_or(opts, :deployment_mode, read_deployment_mode()),
      show_server_logs: opt_or(opts, :show_server_logs, read_show_server_logs()),
      server_args: Keyword.get(opts, :server_args, %{}),
      startup_timeout: opt_or(opts, :startup_timeout, read_timeout("TOAST_STARTUP_TIMEOUT", 60_000)),
      shutdown_timeout: opt_or(opts, :shutdown_timeout, read_timeout("TOAST_SHUTDOWN_TIMEOUT", 30_000)),
      cluster_agents: opt_or(opts, :cluster_agents, read_pos_int("TOAST_CLUSTER_AGENTS", 3)),
      cluster_dbservers: opt_or(opts, :cluster_dbservers, read_pos_int("TOAST_CLUSTER_DBSERVERS", 3)),
      cluster_coordinators: opt_or(opts, :cluster_coordinators, read_pos_int("TOAST_CLUSTER_COORDINATORS", 1)),
      cluster_replication_factor: opt_or(opts, :cluster_replication_factor, read_pos_int("TOAST_CLUSTER_REPLICATION_FACTOR", 2)),
      explicit_sanitizer: explicit_sanitizer,
      sanitizer: opt_or(opts, :sanitizer, Toast.Diagnostics.Sanitizer.detect(explicit_sanitizer))
    }

    Logger.debug(
      "[Toast.Config] build_dir=#{inspect(config.build_dir)} work_dir=#{config.work_dir} " <>
        "mode=#{config.deployment_mode} sanitizer=#{inspect(MapSet.to_list(config.sanitizer))}"
    )

    config
  end

  defp opt_or(opts, key, env_fallback) do
    if Keyword.has_key?(opts, key),
      do: Keyword.fetch!(opts, key),
      else: env_fallback
  end

  defp read_deployment_mode do
    case env("TOAST_DEPLOYMENT_MODE") do
      "cluster" -> :cluster
      _ -> :single_server
    end
  end

  defp read_show_server_logs do
    env("TOAST_SHOW_SERVER_LOGS") == "true"
  end

  defp read_timeout(var, default), do: read_pos_int(var, default)

  defp read_pos_int(var, default) do
    case env(var) do
      nil ->
        default

      val ->
        case Integer.parse(val) do
          {int, ""} when int > 0 ->
            int

          _ ->
            raise ArgumentError, "#{var} must be a positive integer, got: #{inspect(val)}"
        end
    end
  end

  defp default_work_dir do
    Path.join([System.tmp_dir!(), "toast", "run_#{System.unique_integer([:positive])}"])
  end

  defp env(name), do: System.get_env(name)
end
