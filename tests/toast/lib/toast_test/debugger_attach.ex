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

defmodule ToastTest.DebuggerAttach do
  @moduledoc false

  import ToastTest.Formatting

  alias Toast.Deployment
  alias Toast.Deployment.ServerInstance
  alias Toast.Diagnostics.Coredump

  @doc """
  Print server info with debugger attach commands, then wait for user to press ENTER.

  Resolves the debugger executable from the `:debugger` config (`:gdb`, `:lldb`,
  `:auto`, `:none`). If no debugger is found, prints server info without attach commands.
  """
  @spec prompt(Deployment.t(), atom()) :: :ok
  def prompt(deployment, debugger) do
    servers = fetch_servers(deployment)
    debugger_path = resolve_debugger_path(debugger)

    colors = IO.ANSI.enabled?()
    bar = String.duplicate("\u2500", 80)

    IO.puts("")
    IO.puts(colorize(bar, :yellow, colors))
    IO.puts(colorize(" ATTACH DEBUGGER", :yellow, colors))
    IO.puts(colorize(bar, :yellow, colors))
    IO.puts("")

    for line <- format_server_table(servers) do
      IO.puts(colorize(line, :cyan, colors))
    end

    if debugger_path do
      IO.puts("")

      for line <- format_attach_commands(servers, debugger_path) do
        IO.puts(colorize(line, :white, colors))
      end
    end

    IO.puts("")
    IO.puts(colorize("Press ENTER to continue with test execution...", :yellow, colors))
    IO.read(:stdio, :line)

    :ok
  end

  @spec format_server_table([ServerInstance.t()]) :: [String.t()]
  def format_server_table(servers) do
    id_width = servers |> Enum.map(&String.length(&1.id)) |> Enum.max(fn -> 0 end)

    Enum.map(servers, fn server ->
      id = String.pad_trailing(server.id, id_width)
      pid = if server.pid, do: "pid=#{server.pid}", else: "pid=?"
      "  #{id}  #{pid}  #{server.endpoint}"
    end)
  end

  @spec format_attach_commands([ServerInstance.t()], String.t()) :: [String.t()]
  def format_attach_commands(servers, debugger_path) do
    servers
    |> Enum.filter(& &1.pid)
    |> Enum.map(fn server ->
      "  #{debugger_path} -p #{server.pid}   # #{server.id}"
    end)
  end

  defp fetch_servers(deployment) do
    deployment
    |> Deployment.server_instances()
    |> Enum.sort_by(&{&1.role, &1.id})
  end

  defp resolve_debugger_path(debugger) do
    case Coredump.resolve_debugger(debugger) do
      {_mod, path} -> path
      nil -> nil
    end
  end
end
