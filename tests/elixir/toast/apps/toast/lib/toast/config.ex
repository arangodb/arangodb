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
          global_timeout: pos_integer(),
          test_timeout: pos_integer(),
          startup_timeout: pos_integer(),
          shutdown_timeout: pos_integer(),
          timeout_factor: pos_integer(),
          cluster_agents: pos_integer(),
          cluster_dbservers: pos_integer(),
          cluster_coordinators: pos_integer(),
          cluster_replication_factor: pos_integer(),
          keep_work_dir: boolean(),
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
    global_timeout: 3_600_000,
    test_timeout: 300_000,
    startup_timeout: 60_000,
    shutdown_timeout: 60_000,
    timeout_factor: 1,
    cluster_agents: 3,
    cluster_dbservers: 3,
    cluster_coordinators: 1,
    cluster_replication_factor: 2,
    keep_work_dir: false,
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

    sanitizer = opt_or(opts, :sanitizer, Toast.Diagnostics.Sanitizer.detect(explicit_sanitizer))
    factor = opt_or(opts, :timeout_factor, read_timeout_factor(sanitizer))

    config = %__MODULE__{
      build_dir: build_dir,
      work_dir: opt_or(opts, :work_dir, env("TOAST_WORK_DIR")) || default_work_dir(),
      result_dir: opt_or(opts, :result_dir, env("TOAST_RESULT_DIR")) || @default_result_dir,
      deployment_mode: opt_or(opts, :deployment_mode, read_deployment_mode()),
      show_server_logs: opt_or(opts, :show_server_logs, read_show_server_logs()),
      server_args: Keyword.get(opts, :server_args, %{}),
      global_timeout: opt_or(opts, :global_timeout, read_timeout("TOAST_GLOBAL_TIMEOUT", 3_600_000)) * factor,
      test_timeout: opt_or(opts, :test_timeout, read_timeout("TOAST_TEST_TIMEOUT", 300_000)) * factor,
      startup_timeout: opt_or(opts, :startup_timeout, read_timeout("TOAST_STARTUP_TIMEOUT", 60_000)) * factor,
      shutdown_timeout: opt_or(opts, :shutdown_timeout, read_timeout("TOAST_SHUTDOWN_TIMEOUT", 60_000)) * factor,
      timeout_factor: factor,
      cluster_agents: opt_or(opts, :cluster_agents, read_pos_int("TOAST_CLUSTER_AGENTS", 3)),
      cluster_dbservers: opt_or(opts, :cluster_dbservers, read_pos_int("TOAST_CLUSTER_DBSERVERS", 3)),
      cluster_coordinators: opt_or(opts, :cluster_coordinators, read_pos_int("TOAST_CLUSTER_COORDINATORS", 1)),
      cluster_replication_factor: opt_or(opts, :cluster_replication_factor, read_pos_int("TOAST_CLUSTER_REPLICATION_FACTOR", 2)),
      keep_work_dir: opt_or(opts, :keep_work_dir, read_bool("TOAST_KEEP_WORK_DIR")),
      explicit_sanitizer: explicit_sanitizer,
      sanitizer: sanitizer
    }

    Logger.debug(fn ->
      fields = [
        build_dir: inspect(config.build_dir),
        work_dir: config.work_dir,
        result_dir: config.result_dir,
        deployment_mode: config.deployment_mode,
        show_server_logs: config.show_server_logs,
        timeout_factor: config.timeout_factor,
        global_timeout: "#{config.global_timeout}ms",
        test_timeout: "#{config.test_timeout}ms",
        startup_timeout: "#{config.startup_timeout}ms",
        shutdown_timeout: "#{config.shutdown_timeout}ms",
        sanitizer: inspect(MapSet.to_list(config.sanitizer))
      ]

      fields =
        if config.deployment_mode == :cluster do
          fields ++
            [
              cluster_agents: config.cluster_agents,
              cluster_dbservers: config.cluster_dbservers,
              cluster_coordinators: config.cluster_coordinators,
              cluster_replication_factor: config.cluster_replication_factor
            ]
        else
          fields
        end

      "Config: " <> Enum.map_join(fields, " ", fn {k, v} -> "#{k}=#{v}" end)
    end)

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
    read_bool("TOAST_SHOW_SERVER_LOGS")
  end

  defp read_bool(var) do
    env(var) == "true"
  end

  defp read_timeout_factor(sanitizer) do
    case env("TOAST_TIMEOUT_FACTOR") do
      nil -> if MapSet.size(sanitizer) > 0, do: 3, else: 1
      val -> read_pos_int_value("TOAST_TIMEOUT_FACTOR", val)
    end
  end

  defp read_timeout(var, default), do: read_pos_int(var, default)

  defp read_pos_int(var, default) do
    case env(var) do
      nil -> default
      val -> read_pos_int_value(var, val)
    end
  end

  defp read_pos_int_value(var, val) do
    case Integer.parse(val) do
      {int, ""} when int > 0 ->
        int

      _ ->
        raise ArgumentError, "#{var} must be a positive integer, got: #{inspect(val)}"
    end
  end

  defp default_work_dir do
    Path.join([System.tmp_dir!(), "toast", "run_#{System.unique_integer([:positive])}"])
  end

  defp env(name), do: System.get_env(name)
end
