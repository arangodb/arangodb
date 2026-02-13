defmodule Toast.Config do
  @moduledoc "Framework configuration from TOAST_* environment variables."

  @type t :: %__MODULE__{
          bin_dir: Path.t() | nil,
          work_dir: Path.t(),
          deployment_mode: :single_server | :cluster,
          show_server_logs: boolean(),
          server_args: %{String.t() => String.t() | [String.t()]},
          startup_timeout: pos_integer(),
          shutdown_timeout: pos_integer()
        }

  defstruct [
    :bin_dir,
    :work_dir,
    deployment_mode: :single_server,
    show_server_logs: false,
    server_args: %{},
    startup_timeout: 60_000,
    shutdown_timeout: 30_000
  ]

  @spec load() :: t()
  def load, do: load([])

  @spec load(keyword()) :: t()
  def load(opts) do
    %__MODULE__{
      bin_dir: opt_or(opts, :bin_dir, env("TOAST_BIN_DIR")),
      work_dir: opt_or(opts, :work_dir, env("TOAST_WORK_DIR")) || default_work_dir(),
      deployment_mode: opt_or(opts, :deployment_mode, read_deployment_mode()),
      show_server_logs: opt_or(opts, :show_server_logs, read_show_server_logs()),
      server_args: Keyword.get(opts, :server_args, %{}),
      startup_timeout: opt_or(opts, :startup_timeout, read_timeout("TOAST_STARTUP_TIMEOUT", 60_000)),
      shutdown_timeout: opt_or(opts, :shutdown_timeout, read_timeout("TOAST_SHUTDOWN_TIMEOUT", 30_000))
    }
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

  defp read_timeout(var, default) do
    case env(var) do
      nil -> default
      val -> String.to_integer(val)
    end
  end

  defp default_work_dir do
    Path.join(System.tmp_dir!(), "toast_#{System.unique_integer([:positive])}")
  end

  defp env(name), do: System.get_env(name)
end
