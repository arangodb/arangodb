defmodule Mix.Tasks.Toast.Gen.Suite do
  @shortdoc "Generate a new Toast test suite"
  @moduledoc """
  Generates a new Toast test suite under `suites/`.

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

    mode = parse_mode(Keyword.get(opts, :mode))
    module_name = Macro.camelize(name)
    suite_dir = Path.join("suites", name)

    if File.exists?(suite_dir) do
      Mix.raise("Directory #{suite_dir} already exists")
    end

    create_file(Path.join(suite_dir, "suite.ex"), suite_template(module_name, mode))
    create_file(Path.join(suite_dir, "test_example.exs"), example_test_template(module_name))

    Mix.shell().info("""

    Suite "#{name}" created at #{suite_dir}/.

    Run its tests with:

        TOAST_BUILD_DIR=/path/to/build mix toast #{name}
    """)
  end

  defp parse_mode(nil), do: nil
  defp parse_mode("single_server"), do: :single_server
  defp parse_mode("cluster"), do: :cluster
  defp parse_mode(other), do: Mix.raise("Invalid mode: #{other}. Use 'single_server' or 'cluster'.")

  defp create_file(path, content) do
    File.mkdir_p!(Path.dirname(path))
    File.write!(path, content)
    Mix.shell().info("* creating #{path}")
  end

  defp suite_template(module_name, nil) do
    """
    defmodule #{module_name}.Suite do
      use ToastTest.Suite
    end
    """
  end

  defp suite_template(module_name, mode) do
    """
    defmodule #{module_name}.Suite do
      use ToastTest.Suite,
        mode: :#{mode}
    end
    """
  end

  defp example_test_template(module_name) do
    """
    defmodule #{module_name}.ExampleTest do
      use #{module_name}.Suite

      test "server is running", %{client: client} do
        assert {:ok, body} = Client.Admin.version(client)
        assert body["server"] == "arango"
      end
    end
    """
  end
end
