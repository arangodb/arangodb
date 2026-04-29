################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule ToastTest.JS.ArangoshExecutor do
  @moduledoc """
  Executor that runs a JavaScript test file via arangosh.

  Launches arangosh with `--javascript.unit-tests` pointing to the JS file.
  The arangosh process uses `@arangodb/testrunner.runCommandLineTests()` to
  execute the tests and writes results to `testresult.json` in a temp directory.

  ## Required options

    * `:endpoint` — ArangoDB server endpoint (e.g., `"http://127.0.0.1:8529"`)
    * `:deployment` — `Toast.Deployment.t()` struct
    * `:arangosh` — absolute path to the arangosh binary
    * `:repo_root` — absolute path to the repository root

  ## Optional options

    * `:timeout` — execution timeout in milliseconds (default: 300_000)
    * `:extra_args` — map of extra arangosh CLI arguments
    * `:test_filter` — test name filter string

  """

  @behaviour ToastTest.JS.Executor

  require Logger

  @metadata_set MapSet.new(~w(
    duration status failed total totalSetUp totalTearDown
    suiteName message setUpAllDuration teardownAllDuration
  ))

  @test_metadata_set MapSet.new(~w(
    duration status failed total totalSetUp totalTearDown
    suiteName message setUpAllDuration teardownAllDuration
    setUpDuration tearDownDuration
  ))

  @needs_escape ~r/[^a-zA-Z0-9_.\/:-]/

  @impl true
  def run(js_file, opts) do
    deployment = Keyword.fetch!(opts, :deployment)
    endpoint = Keyword.fetch!(opts, :endpoint)
    arangosh = Keyword.fetch!(opts, :arangosh)
    repo_root = Keyword.fetch!(opts, :repo_root)
    timeout = Keyword.get(opts, :timeout, 300_000)
    extra_args = Keyword.get(opts, :extra_args, %{})
    test_filter = Keyword.get(opts, :test_filter)

    work_dir = create_work_dir()

    try do
      instance_info = build_instance_info(deployment, endpoint, work_dir)
      args = build_args(repo_root, endpoint, js_file, extra_args, test_filter, timeout)

      case execute(arangosh, args, instance_info, repo_root, timeout) do
        {:ok, _exit_code} ->
          read_results(work_dir, js_file)

        {:error, reason} ->
          {:error, reason}
      end
    after
      File.rm_rf(work_dir)
    end
  end

  defp build_args(repo_root, endpoint, js_file, extra_args, test_filter, timeout) do
    js_dir = Path.join(repo_root, "js")
    enterprise_js_dir = Path.join(repo_root, "enterprise/js")
    config_file = Path.join([repo_root, "etc", "testing", "arangosh.conf"])

    base = [
      "--configuration",
      config_file,
      "--javascript.startup-directory",
      js_dir,
      "--console.colors",
      "true",
      "--server.endpoint",
      url_to_endpoint(endpoint),
      "--javascript.unit-tests",
      js_file,
      "--log.level",
      "warning",
      "--javascript.execution-deadline",
      Integer.to_string(div(timeout, 1000)),
      "--server.authentication",
      "false"
    ]

    base =
      if File.dir?(enterprise_js_dir) do
        base ++ ["--javascript.module-directory", enterprise_js_dir]
      else
        base
      end

    base =
      if test_filter do
        base ++ ["--javascript.unit-test-filter", test_filter]
      else
        base
      end

    extra = Enum.flat_map(extra_args, fn {k, v} -> ["--#{k}", to_string(v)] end)

    base ++ extra
  end

  defp build_instance_info(deployment, endpoint, work_dir) do
    servers =
      deployment
      |> Toast.Deployment.servers()
      |> Enum.map(fn srv ->
        %{
          id: srv.id,
          instanceRole: role_to_string(srv.role),
          endpoint: srv.endpoint,
          url: endpoint_to_url(srv.endpoint),
          port: srv.port
        }
      end)

    %{
      protocol: endpoint_to_protocol(endpoint),
      rootDir: work_dir,
      endpoint: endpoint,
      url: endpoint_to_url(endpoint),
      arangods: servers
    }
  end

  defp execute(arangosh, args, instance_info, cwd, timeout) do
    env = [{"INSTANCEINFO", Jason.encode!(instance_info)}]

    command_str = Enum.join([shell_escape(arangosh) | Enum.map(args, &shell_escape/1)], " ")
    Logger.debug("Running arangosh: #{command_str}")

    script_bin = System.find_executable("script") || "script"

    port =
      Port.open({:spawn_executable, script_bin}, [
        :binary,
        :exit_status,
        :stderr_to_stdout,
        {:args, ["-qfec", command_str, "/dev/null"]},
        {:env, Enum.map(env, fn {k, v} -> {String.to_charlist(k), String.to_charlist(v)} end)},
        {:cd, String.to_charlist(cwd)}
      ])

    deadline = System.monotonic_time(:millisecond) + timeout
    await_exit(port, deadline, timeout)
  end

  defp shell_escape(arg) do
    if arg =~ @needs_escape do
      "'" <> String.replace(arg, "'", "'\\''") <> "'"
    else
      arg
    end
  end

  defp await_exit(port, deadline, timeout) do
    remaining = max(deadline - System.monotonic_time(:millisecond), 0)

    receive do
      {^port, {:data, data}} ->
        IO.write(data)
        await_exit(port, deadline, timeout)

      {^port, {:exit_status, status}} ->
        Logger.debug("arangosh exited with status #{status}")
        {:ok, status}
    after
      remaining ->
        Port.close(port)
        {:error, "arangosh timed out after #{timeout}ms"}
    end
  end

  defp read_results(work_dir, js_file) do
    result_file = Path.join(work_dir, "testresult.json")

    case File.read(result_file) do
      {:ok, content} ->
        parse_results(content, js_file)

      {:error, reason} ->
        {:error, "Failed to read testresult.json: #{inspect(reason)}"}
    end
  end

  defp parse_results(content, js_file) do
    case Jason.decode(content) do
      {:ok, data} when is_map(data) ->
        tests = extract_tests(data, js_file)
        {:ok, %{tests: tests}}

      {:ok, _other} ->
        {:error, "Unexpected testresult.json format"}

      {:error, reason} ->
        {:error, "Failed to parse testresult.json: #{inspect(reason)}"}
    end
  end

  defp extract_tests(data, js_file) do
    data
    |> Enum.reject(fn {key, _} -> MapSet.member?(@metadata_set, key) end)
    |> Enum.flat_map(fn
      {_file_key, file_result} when is_map(file_result) ->
        file_result
        |> Enum.reject(fn {key, _} -> MapSet.member?(@test_metadata_set, key) end)
        |> Enum.filter(fn {_key, val} -> is_map(val) end)
        |> Enum.map(fn {name, result} -> parse_test_result(name, result, js_file) end)

      {_key, _non_map} ->
        []
    end)
  end

  defp parse_test_result(name, result, js_file) when is_map(result) do
    %{
      name: name,
      status: map_status(result),
      duration_ms: result["duration"] || 0,
      message: result["message"],
      file: js_file,
      line: nil,
      started_at: parse_iso8601(result["startedAt"]),
      finished_at: parse_iso8601(result["finishedAt"])
    }
  end

  defp map_status(%{"status" => true}), do: :pass
  defp map_status(%{"status" => false, "message" => msg}) when is_binary(msg), do: :fail
  defp map_status(%{"status" => false}), do: :fail
  defp map_status(%{"skipped" => true}), do: :skip
  defp map_status(_), do: :error

  defp url_to_endpoint("http://" <> rest), do: "tcp://#{rest}"
  defp url_to_endpoint("https://" <> rest), do: "ssl://#{rest}"
  defp url_to_endpoint(other), do: other

  defp endpoint_to_url("tcp://" <> rest), do: "http://#{rest}"
  defp endpoint_to_url("ssl://" <> rest), do: "https://#{rest}"
  defp endpoint_to_url(other), do: other

  defp endpoint_to_protocol("tcp://" <> _), do: "tcp"
  defp endpoint_to_protocol("ssl://" <> _), do: "ssl"
  defp endpoint_to_protocol("http://" <> _), do: "tcp"
  defp endpoint_to_protocol("https://" <> _), do: "ssl"
  defp endpoint_to_protocol(_), do: "tcp"

  defp role_to_string(:single), do: "single"
  defp role_to_string(:coordinator), do: "coordinator"
  defp role_to_string(:dbserver), do: "dbserver"
  defp role_to_string(:agent), do: "agent"
  defp role_to_string(other), do: to_string(other)

  defp parse_iso8601(nil), do: nil

  defp parse_iso8601(str) when is_binary(str) do
    case DateTime.from_iso8601(str) do
      {:ok, dt, _offset} -> dt
      _ -> nil
    end
  end

  defp create_work_dir do
    dir = Path.join(System.tmp_dir!(), "toast-js-#{:erlang.unique_integer([:positive])}")
    File.mkdir_p!(dir)
    dir
  end
end
