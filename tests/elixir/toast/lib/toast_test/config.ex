defmodule ToastTest.Config do
  @moduledoc """
  Configuration for test execution — timeouts, result directories,
  diagnostics, and CI settings.

  Constructed via `new/1` which reads defaults from application env
  (populated by `Toast.Env.load/1`) and applies optional overrides.
  """

  @type t :: %__MODULE__{
          base_dir: Path.t(),
          result_dir: Path.t(),
          deployment_mode: :single_server | :cluster,
          timeout_factor: number(),
          global_timeout: pos_integer(),
          test_timeout: pos_integer(),
          keep_data: boolean(),
          ci: boolean(),
          debugger: :gdb | :lldb | :auto | :none | nil,
          coredump_timeout: pos_integer(),
          coredump_dir: Path.t() | nil,
          dump_agency_on_error: boolean()
        }

  defstruct base_dir: nil,
            result_dir: Toast.Env.default_result_dir(),
            deployment_mode: :single_server,
            timeout_factor: 1,
            global_timeout: 3_600_000,
            test_timeout: 300_000,
            keep_data: false,
            ci: false,
            debugger: :auto,
            coredump_timeout: 180_000,
            coredump_dir: nil,
            dump_agency_on_error: true

  @doc """
  Build a test config from application env with optional overrides.
  """
  @spec new(keyword()) :: t()
  def new(overrides \\ []) do
    base =
      Enum.reduce(Map.from_struct(%__MODULE__{}), %__MODULE__{}, fn {key, default}, acc ->
        Map.put(acc, key, Application.get_env(:toast, key, default))
      end)

    struct!(base, overrides)
  end
end
