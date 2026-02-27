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
          sanitizer: MapSet.t(String.t()),
          api_version: non_neg_integer() | String.t() | nil,
          debugger: :gdb | :lldb | :auto | :none | nil,
          dump_agency_on_error: boolean(),
          coredump_timeout: pos_integer(),
          ci: boolean()
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
    sanitizer: MapSet.new(),
    api_version: nil,
    debugger: :auto,
    dump_agency_on_error: true,
    coredump_timeout: 120_000,
    ci: false
  ]

  @spec load() :: t()
  def load, do: load([])

  @spec load(keyword()) :: t()
  def load(opts) do
    local = load_local_config()
    {build_dir, explicit_sanitizer, sanitizer, factor} = resolve_sanitizer(opts, local)

    build_config(opts, local, build_dir, explicit_sanitizer, sanitizer, factor)
    |> apply_timeout_factor(factor)
    |> log_config()
  end

  defp resolve_sanitizer(opts, local) do
    build_dir = opt_or(opts, :build_dir, env("TOAST_BUILD_DIR"), local[:build_dir])

    explicit_sanitizer =
      opt_or(opts, :explicit_sanitizer, env("TOAST_SANITIZER"), local[:explicit_sanitizer]) ||
        Toast.Diagnostics.Sanitizer.detect_from_build_dir(build_dir)

    sanitizer = opt_or(opts, :sanitizer, Toast.Diagnostics.Sanitizer.detect(explicit_sanitizer))
    factor = opt_or(opts, :timeout_factor, read_timeout_factor(sanitizer), local[:timeout_factor])

    {build_dir, explicit_sanitizer, sanitizer, factor}
  end

  defp build_config(opts, local, build_dir, explicit_sanitizer, sanitizer, factor) do
    %__MODULE__{
      build_dir: build_dir,
      work_dir: opt_or(opts, :work_dir, env("TOAST_WORK_DIR"), local[:work_dir]) || default_work_dir(),
      result_dir: opt_or(opts, :result_dir, env("TOAST_RESULT_DIR"), local[:result_dir]) || @default_result_dir,
      deployment_mode: opt_or(opts, :deployment_mode, read_deployment_mode(), local[:deployment_mode]),
      show_server_logs: opt_or(opts, :show_server_logs, read_show_server_logs(), local[:show_server_logs]),
      server_args: Keyword.get(opts, :server_args, local[:server_args] || %{}),
      global_timeout: opt_or(opts, :global_timeout, read_timeout("TOAST_GLOBAL_TIMEOUT", 3_600_000), local[:global_timeout]),
      test_timeout: opt_or(opts, :test_timeout, read_timeout("TOAST_TEST_TIMEOUT", 300_000), local[:test_timeout]),
      startup_timeout: opt_or(opts, :startup_timeout, read_timeout("TOAST_STARTUP_TIMEOUT", 60_000), local[:startup_timeout]),
      shutdown_timeout: opt_or(opts, :shutdown_timeout, read_timeout("TOAST_SHUTDOWN_TIMEOUT", 60_000), local[:shutdown_timeout]),
      timeout_factor: factor,
      cluster_agents: opt_or(opts, :cluster_agents, read_pos_int("TOAST_CLUSTER_AGENTS", 3), local[:cluster_agents]),
      cluster_dbservers: opt_or(opts, :cluster_dbservers, read_pos_int("TOAST_CLUSTER_DBSERVERS", 3), local[:cluster_dbservers]),
      cluster_coordinators: opt_or(opts, :cluster_coordinators, read_pos_int("TOAST_CLUSTER_COORDINATORS", 1), local[:cluster_coordinators]),
      cluster_replication_factor: opt_or(opts, :cluster_replication_factor, read_pos_int("TOAST_CLUSTER_REPLICATION_FACTOR", 2), local[:cluster_replication_factor]),
      keep_work_dir: opt_or(opts, :keep_work_dir, read_bool("TOAST_KEEP_WORK_DIR"), local[:keep_work_dir]),
      explicit_sanitizer: explicit_sanitizer,
      sanitizer: sanitizer,
      api_version: opt_or(opts, :api_version, read_api_version(), local[:api_version]),
      debugger: opt_or(opts, :debugger, read_debugger(), local[:debugger]) || :auto,
      dump_agency_on_error: opt_or(opts, :dump_agency_on_error, read_opt_bool("TOAST_DUMP_AGENCY"), local[:dump_agency_on_error]) |> default_true(),
      coredump_timeout: opt_or(opts, :coredump_timeout, read_pos_int("TOAST_COREDUMP_TIMEOUT", nil), local[:coredump_timeout]) || 120_000,
      ci: opt_or(opts, :ci, read_bool("TOAST_CI"), local[:ci])
    }
  end

  defp apply_timeout_factor(config, factor) do
    %{
      config
      | global_timeout: config.global_timeout * factor,
        test_timeout: config.test_timeout * factor,
        startup_timeout: config.startup_timeout * factor,
        shutdown_timeout: config.shutdown_timeout * factor,
        coredump_timeout: config.coredump_timeout * factor
    }
  end

  defp log_config(config) do
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

  # Precedence: keyword opts > env vars > default (no local config tier)
  defp opt_or(opts, key, env_fallback) do
    if Keyword.has_key?(opts, key),
      do: Keyword.fetch!(opts, key),
      else: env_fallback
  end

  # Precedence: keyword opts > env vars > .toast.local.exs > nil
  defp opt_or(opts, key, env_fallback, local_fallback) do
    if Keyword.has_key?(opts, key) do
      Keyword.fetch!(opts, key)
    else
      if env_fallback != nil, do: env_fallback, else: local_fallback
    end
  end

  defp load_local_config do
    if System.get_env("TOAST_CI") == "true" do
      %{}
    else
      path = Path.join(File.cwd!(), ".toast.local.exs")

      if File.exists?(path) do
        {config_map, _bindings} = Code.eval_file(path)
        if is_map(config_map), do: config_map, else: %{}
      else
        %{}
      end
    end
  rescue
    error ->
      Logger.warning("Failed to load .toast.local.exs: #{Exception.message(error)}")
      %{}
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

  defp read_opt_bool(var) do
    case env(var) do
      "true" -> true
      "false" -> false
      _ -> nil
    end
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

  defp read_api_version do
    case env("TOAST_API_VERSION") do
      nil ->
        nil

      val ->
        case Integer.parse(val) do
          {int, ""} -> int
          _ -> val
        end
    end
  end

  defp read_debugger do
    case env("TOAST_DEBUGGER") do
      "gdb" -> :gdb
      "lldb" -> :lldb
      "auto" -> :auto
      "none" -> :none
      _ -> nil
    end
  end

  defp default_work_dir do
    Path.join([System.tmp_dir!(), "toast", "run_#{System.unique_integer([:positive])}"])
  end

  defp default_true(nil), do: true
  defp default_true(val), do: val

  defp env(name), do: System.get_env(name)
end
