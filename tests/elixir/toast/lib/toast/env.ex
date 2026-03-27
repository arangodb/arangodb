defmodule Toast.Env do
  @moduledoc """
  Load Toast configuration from environment variables and `.toast.local.exs`,
  then populate `Application` env under the `:toast` app.

  Called automatically by `Toast.Application.start/2` (env vars + local file).
  Called again by `Mix.Tasks.Toast` with CLI opts taking precedence.

  Precedence (highest to lowest): opts > env vars > .toast.local.exs > defaults.
  """

  require Logger

  alias Toast.Diagnostics.Sanitizer

  @default_result_dir "toast-results"

  @spec load(keyword()) :: :ok
  def load(opts \\ []) do
    local = load_local_config(Keyword.get(opts, :local_config_dir))

    values =
      resolve_all(opts, local)
      |> resolve_sanitizers()
      |> apply_timeout_factor()
      |> ensure_base_dir()

    for {key, value} <- values, value != nil do
      Application.put_env(:toast, key, value)
    end

    Application.put_env(:toast, :__env_loaded__, true)
    log_config(values)
    :ok
  end

  @doc "Returns true if `load/1` has been called."
  @spec loaded?() :: boolean()
  def loaded?, do: Application.get_env(:toast, :__env_loaded__, false)

  @doc "Default result directory name."
  @spec default_result_dir() :: String.t()
  def default_result_dir, do: @default_result_dir

  defp resolve_all(opts, local) do
    %{
      # Paths
      build_dir: resolve(opts, local, :build_dir, &env/1, "TOAST_BUILD_DIR"),
      base_dir: resolve(opts, local, :base_dir, &env/1, "TOAST_BASE_DIR"),
      result_dir:
        resolve(opts, local, :result_dir, &env/1, "TOAST_RESULT_DIR") || @default_result_dir,
      coredump_dir: resolve(opts, local, :coredump_dir, &env/1, "TOAST_COREDUMP_DIR"),

      # Timeouts
      global_timeout:
        resolve(opts, local, :global_timeout, &read_pos_int/1, "TOAST_GLOBAL_TIMEOUT") ||
          3_600_000,
      test_timeout:
        resolve(opts, local, :test_timeout, &read_pos_int/1, "TOAST_TEST_TIMEOUT") || 300_000,
      startup_timeout:
        resolve(opts, local, :startup_timeout, &read_pos_int/1, "TOAST_STARTUP_TIMEOUT") ||
          60_000,
      shutdown_timeout:
        resolve(opts, local, :shutdown_timeout, &read_pos_int/1, "TOAST_SHUTDOWN_TIMEOUT") ||
          60_000,
      coredump_timeout:
        resolve(opts, local, :coredump_timeout, &read_pos_int/1, "TOAST_COREDUMP_TIMEOUT") ||
          180_000,
      timeout_factor:
        resolve(opts, local, :timeout_factor, &read_pos_int/1, "TOAST_TIMEOUT_FACTOR"),

      # Cluster topology
      cluster_agents:
        resolve(opts, local, :cluster_agents, &read_pos_int/1, "TOAST_CLUSTER_AGENTS") || 3,
      cluster_dbservers:
        resolve(opts, local, :cluster_dbservers, &read_pos_int/1, "TOAST_CLUSTER_DBSERVERS") || 3,
      cluster_coordinators:
        resolve(opts, local, :cluster_coordinators, &read_pos_int/1, "TOAST_CLUSTER_COORDINATORS") ||
          1,
      cluster_replication_factor:
        resolve(
          opts,
          local,
          :cluster_replication_factor,
          &read_pos_int/1,
          "TOAST_CLUSTER_REPLICATION_FACTOR"
        ) || 2,

      # Server args (no env var — only via opts or local config)
      server_args: Keyword.get(opts, :server_args, local[:server_args]) || %{},
      coordinator_args: Keyword.get(opts, :coordinator_args, local[:coordinator_args]) || %{},
      dbserver_args: Keyword.get(opts, :dbserver_args, local[:dbserver_args]) || %{},
      agent_args: Keyword.get(opts, :agent_args, local[:agent_args]) || %{},

      # Deployment
      show_server_logs:
        resolve(opts, local, :show_server_logs, &read_bool/1, "TOAST_SHOW_SERVER_LOGS") || false,
      deployment_mode:
        resolve(opts, local, :deployment_mode, &read_deployment_mode/1, "TOAST_DEPLOYMENT_MODE") ||
          :single_server,
      api_version: resolve(opts, local, :api_version, &read_api_version/1, "TOAST_API_VERSION"),
      sanitizer_override:
        resolve(opts, local, :sanitizer_override, &read_sanitizer/1, "TOAST_SANITIZER"),
      active_sanitizers: nil,

      # Test execution
      keep_data: resolve(opts, local, :keep_data, &read_bool/1, "TOAST_KEEP_DATA") || false,
      ci: resolve(opts, local, :ci, &read_bool/1, "TOAST_CI") || false,
      debugger: resolve(opts, local, :debugger, &read_debugger/1, "TOAST_DEBUGGER") || :auto,
      dump_agency_on_error:
        resolve(opts, local, :dump_agency_on_error, &read_opt_bool/1, "TOAST_DUMP_AGENCY")
    }
    |> then(fn values ->
      # dump_agency_on_error defaults to true if not explicitly set
      if values.dump_agency_on_error == nil,
        do: %{values | dump_agency_on_error: true},
        else: values
    end)
  end

  # Precedence: opts > env var > local file
  defp resolve(opts, local, key, env_reader, env_var) do
    case Keyword.fetch(opts, key) do
      {:ok, val} ->
        val

      :error ->
        case env_reader.(env_var) do
          nil -> local[key]
          val -> val
        end
    end
  end

  defp resolve_sanitizers(values) do
    sanitizer_override =
      values.sanitizer_override
      |> normalize_sanitizer()
      |> then(&(&1 || Sanitizer.detect_from_build_dir(values.build_dir)))

    active_sanitizers = Sanitizer.detect(sanitizer_override)

    %{values | sanitizer_override: sanitizer_override, active_sanitizers: active_sanitizers}
  end

  defp apply_timeout_factor(values) do
    factor =
      values.timeout_factor ||
        if Enum.any?(values.active_sanitizers), do: 3, else: 1

    %{
      values
      | timeout_factor: factor,
        global_timeout: round(values.global_timeout * factor),
        test_timeout: round(values.test_timeout * factor),
        startup_timeout: round(values.startup_timeout * factor),
        shutdown_timeout: round(values.shutdown_timeout * factor),
        coredump_timeout: round(values.coredump_timeout * factor)
    }
  end

  defp ensure_base_dir(%{base_dir: nil} = values) do
    %{values | base_dir: generate_base_dir()}
  end

  defp ensure_base_dir(values), do: values

  defp generate_base_dir do
    ts = DateTime.utc_now(:second) |> DateTime.to_iso8601() |> String.replace(":", "-")
    suffix = :crypto.strong_rand_bytes(2) |> Base.encode16()
    Path.join([System.tmp_dir!(), "toast", "#{ts}_#{suffix}"])
  end

  defp load_local_config(dir) do
    if System.get_env("TOAST_CI") == "true" do
      %{}
    else
      read_local_config_file(dir || File.cwd!())
    end
  rescue
    error ->
      Logger.error(
        "Failed to load .toast.local.exs: #{Exception.message(error)}\n#{Exception.format_stacktrace(__STACKTRACE__)}"
      )

      %{}
  end

  defp read_local_config_file(dir) do
    path = Path.join(dir, ".toast.local.exs")

    if File.exists?(path) do
      {config_map, _bindings} = Code.eval_file(path)
      if is_map(config_map), do: config_map, else: %{}
    else
      %{}
    end
  end

  defp env(name), do: System.get_env(name)

  defp read_bool(var) do
    case env(var) do
      nil -> nil
      "true" -> true
      _ -> false
    end
  end

  defp read_opt_bool(var) do
    case env(var) do
      "true" -> true
      "false" -> false
      _ -> nil
    end
  end

  defp read_pos_int(var) do
    case env(var) do
      nil ->
        nil

      val ->
        case Integer.parse(val) do
          {int, ""} when int > 0 -> int
          _ -> raise ArgumentError, "#{var} must be a positive integer, got: #{inspect(val)}"
        end
    end
  end

  defp read_deployment_mode(var) do
    case env(var) do
      "cluster" ->
        :cluster

      "single_server" ->
        :single_server

      nil ->
        nil

      other ->
        raise ArgumentError,
              "Invalid #{var}: #{inspect(other)} (expected \"cluster\" or \"single_server\")"
    end
  end

  defp normalize_sanitizer("tsan"), do: :tsan
  defp normalize_sanitizer("alubsan"), do: :alubsan
  defp normalize_sanitizer(atom) when atom in [nil, :tsan, :alubsan], do: atom

  defp normalize_sanitizer(other),
    do: raise(ArgumentError, "invalid sanitizer: #{inspect(other)}, expected :tsan or :alubsan")

  defp read_sanitizer(var) do
    case env(var) do
      "tsan" ->
        :tsan

      "alubsan" ->
        :alubsan

      nil ->
        nil

      other ->
        raise ArgumentError,
              "Invalid #{var}: #{inspect(other)} (expected \"tsan\" or \"alubsan\")"
    end
  end

  defp read_api_version(var) do
    case env(var) do
      nil ->
        nil

      val ->
        case Integer.parse(val) do
          {int, ""} -> int
          _ -> val
        end
    end
  end

  defp read_debugger(var) do
    case env(var) do
      "gdb" -> :gdb
      "lldb" -> :lldb
      "auto" -> :auto
      "none" -> :none
      _ -> nil
    end
  end

  defp log_config(values) do
    Logger.debug(fn ->
      fields = [
        build_dir: inspect(values.build_dir),
        base_dir: values.base_dir,
        result_dir: values.result_dir,
        deployment_mode: values.deployment_mode,
        show_server_logs: values.show_server_logs,
        timeout_factor: values.timeout_factor,
        global_timeout: "#{values.global_timeout}ms",
        test_timeout: "#{values.test_timeout}ms",
        startup_timeout: "#{values.startup_timeout}ms",
        shutdown_timeout: "#{values.shutdown_timeout}ms",
        active_sanitizers: inspect(MapSet.to_list(values.active_sanitizers))
      ]

      "Toast.Env: " <> Enum.map_join(fields, " ", fn {k, v} -> "#{k}=#{v}" end)
    end)
  end
end
