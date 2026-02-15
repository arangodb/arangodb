defmodule Mix.Tasks.Toast.Gen.Suite do
  @shortdoc "Generate a new Toast test suite app"
  @moduledoc """
  Generates a new umbrella app for a Toast test suite.

  ## Usage

      mix toast.gen.suite <name> [--mode single_server|cluster]

  ## Examples

      mix toast.gen.suite smoke_test
      mix toast.gen.suite cluster_tests --mode cluster
  """

  use Mix.Task

  @impl Mix.Task
  def run(args) do
    {opts, argv, _} = OptionParser.parse(args, strict: [mode: :string])

    name =
      case argv do
        [name | _] -> name
        [] -> Mix.raise("Usage: mix toast.gen.suite <name> [--mode single_server|cluster]")
      end

    mode = parse_mode(Keyword.get(opts, :mode, "single_server"))
    module_name = Macro.camelize(name)
    apps_dir = Path.join("apps", name)

    if File.exists?(apps_dir) do
      Mix.raise("Directory #{apps_dir} already exists")
    end

    create_file(Path.join(apps_dir, "mix.exs"), mix_exs_template(name, module_name))
    create_file(Path.join([apps_dir, "lib", "#{name}.ex"]), lib_template(name, module_name))
    create_file(Path.join([apps_dir, "test", "test_helper.exs"]), test_helper_template(mode))
    create_file(
      Path.join([apps_dir, "test", name, "example_test.exs"]),
      example_test_template(module_name)
    )

    Mix.shell().info("""

    Suite "#{name}" created at #{apps_dir}/.

    Run its tests with:

        TOAST_BIN_DIR=/path/to/bin mix test #{apps_dir}
    """)
  end

  defp parse_mode("single_server"), do: :single_server
  defp parse_mode("cluster"), do: :cluster
  defp parse_mode(other), do: Mix.raise("Invalid mode: #{other}. Use 'single_server' or 'cluster'.")

  defp create_file(path, content) do
    File.mkdir_p!(Path.dirname(path))
    File.write!(path, content)
    Mix.shell().info("* creating #{path}")
  end

  defp mix_exs_template(name, module_name) do
    """
    defmodule #{module_name}.MixProject do
      use Mix.Project

      def project do
        [
          app: :#{name},
          version: "0.1.0",
          build_path: "../../_build",
          config_path: "../../config/config.exs",
          deps_path: "../../deps",
          lockfile: "../../mix.lock",
          elixir: "~> 1.19",
          start_permanent: false,
          deps: deps()
        ]
      end

      def application do
        [extra_applications: [:logger]]
      end

      defp deps do
        [{:toast, in_umbrella: true}]
      end
    end
    """
  end

  defp lib_template(name, module_name) do
    """
    defmodule #{module_name} do
      @moduledoc "#{String.replace(name, "_", " ") |> String.capitalize()} test suite."
    end
    """
  end

  defp test_helper_template(mode) do
    """
    ExUnit.start()

    case Toast.TestCase.setup_suite(:#{mode}) do
      {:ok, _} -> :ok
      {:error, _} -> ExUnit.configure(exclude: [:toast_suite])
    end
    """
  end

  defp example_test_template(module_name) do
    """
    defmodule #{module_name}.ExampleTest do
      use Toast.TestCase

      test "server is running", %{client: client} do
        assert {:ok, body} = Client.version(client)
        assert body["server"] == "arango"
      end

      test "AQL query works", %{client: client} do
        assert {:ok, [1]} = Client.aql(client, "RETURN 1")
      end
    end
    """
  end
end
