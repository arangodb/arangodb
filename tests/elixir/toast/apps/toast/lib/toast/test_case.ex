defmodule Toast.TestCase do
  @moduledoc """
  ExUnit.CaseTemplate for Toast test suites.

  Provides test context with `deployment`, `endpoint`, and `client` keys.
  Requires a deployment to be registered via `setup_suite/2` or `setup_suite!/2`
  in the suite's `test_helper.exs`.

  ## Usage

  In `test_helper.exs`:

      ExUnit.start()
      Toast.TestCase.setup_suite()

  In test modules:

      defmodule MyTest do
        use Toast.TestCase

        test "server version", %{client: client} do
          assert {:ok, %{"server" => "arango"}} = Client.version(client)
        end
      end
  """

  use ExUnit.CaseTemplate

  require Logger

  @env_key :__test_deployment__
  @unavailable_key :__test_deployment_unavailable__

  using do
    quote do
      alias Toast.Client
      @moduletag :toast_suite
    end
  end

  setup _context do
    deployment = get_deployment()
    client = Toast.Client.new(deployment.endpoint)
    %{deployment: deployment, endpoint: deployment.endpoint, client: client}
  end

  @doc """
  Start a deployment and register it for test modules. Raises on failure.
  Call from test_helper.exs after ExUnit.start().

  When called without arguments, reads the deployment mode from
  `TOAST_DEPLOYMENT_MODE` (via `Toast.Config`).
  """
  @spec setup_suite!(atom(), keyword()) :: Toast.Deployment.t()
  def setup_suite!(mode \\ nil, opts \\ []) do
    mode = mode || Toast.Config.load(opts).deployment_mode

    case Toast.Deployment.start(mode, opts) do
      {:ok, deployment} ->
        register_deployment(deployment)
        register_after_suite(deployment)
        deployment

      {:error, reason} ->
        raise "Failed to start #{mode} deployment: #{inspect(reason)}"
    end
  end

  @doc """
  Start a deployment and register it. On failure, marks deployment as
  unavailable so tests are skipped gracefully.
  Returns {:ok, deployment} or {:error, reason}.

  When called without arguments, reads the deployment mode from
  `TOAST_DEPLOYMENT_MODE` (via `Toast.Config`).
  """
  @spec setup_suite(atom(), keyword()) :: {:ok, Toast.Deployment.t()} | {:error, term()}
  def setup_suite(mode \\ nil, opts \\ []) do
    mode = mode || Toast.Config.load(opts).deployment_mode

    case Toast.Deployment.start(mode, opts) do
      {:ok, deployment} ->
        register_deployment(deployment)
        register_after_suite(deployment)
        {:ok, deployment}

      {:error, reason} ->
        Application.put_env(:toast, @unavailable_key, true)
        {:error, reason}
    end
  end

  @doc "Register a deployment for test modules to use."
  @spec register_deployment(Toast.Deployment.t()) :: :ok
  def register_deployment(deployment) do
    Application.put_env(:toast, @env_key, deployment)
    :ok
  end

  @doc "Retrieve the registered deployment."
  @spec get_deployment() :: Toast.Deployment.t()
  def get_deployment do
    Application.get_env(:toast, @env_key) ||
      raise """
      No deployment registered.
      Call Toast.TestCase.setup_suite/2 in your test_helper.exs after ExUnit.start().
      """
  end

  defp register_after_suite(deployment) do
    register_formatters()

    ExUnit.after_suite(fn stats ->
      diagnostics = Toast.Deployment.stop_and_collect(deployment)
      Application.put_env(:toast, :__test_diagnostics__, diagnostics)
      print_diagnostics_summary(diagnostics)
      Toast.ResultExporter.export()

      if stats.failures == 0 do
        File.rm_rf(deployment.work_dir)
      else
        Logger.warning("Test failures — preserving server data in #{deployment.work_dir}")
      end
    end)
  end

  defp register_formatters do
    current = Application.get_env(:ex_unit, :formatters, [ExUnit.CLIFormatter])

    # Replace ExUnit.CLIFormatter with Toast.CLIFormatter
    formatters = List.delete(current, ExUnit.CLIFormatter)

    formatters =
      if Toast.CLIFormatter in formatters,
        do: formatters,
        else: [Toast.CLIFormatter | formatters]

    formatters =
      if Toast.ResultFormatter in formatters,
        do: formatters,
        else: formatters ++ [Toast.ResultFormatter]

    ExUnit.configure(formatters: formatters)
  end

  defp print_diagnostics_summary(nil), do: :ok

  defp print_diagnostics_summary(diagnostics) do
    case Toast.Diagnostics.Summary.format_crashed_servers(diagnostics) do
      nil -> :ok
      text -> IO.puts(text)
    end
  end
end
