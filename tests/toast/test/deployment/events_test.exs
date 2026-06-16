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

defmodule Toast.Deployment.EventsTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.{Events, ServerInstance}

  defmodule CaptureListener do
    @behaviour Toast.Deployment.EventListener

    @impl true
    def on_event(event), do: send(self(), {:event, event})
  end

  defp emitted do
    assert_receive {:event, event}
    {ts, event} = Map.pop!(event, :timestamp)
    assert is_integer(ts)
    event
  end

  defp server_instance(overrides \\ []) do
    struct!(
      %ServerInstance{
        id: "dbserver-1",
        role: :dbserver,
        port: 8629,
        endpoint: "http://127.0.0.1:8629",
        log_file: "/data/dbserver-1/arangod.log",
        server_dir: "/data/dbserver-1",
        pid: 4242
      },
      overrides
    )
  end

  test "deployment_starting/5 projects servers to complete static birth records" do
    servers = %{"dbserver-1" => server_instance()}

    Events.deployment_starting(CaptureListener, "dep-1", :cluster, [:fake_stacktrace], servers)

    assert emitted() == %{
             event: :deployment_starting,
             deployment_id: "dep-1",
             mode: :cluster,
             stacktrace: [:fake_stacktrace],
             specs: [
               %{
                 id: "dbserver-1",
                 role: :dbserver,
                 port: 8629,
                 endpoint: "http://127.0.0.1:8629",
                 log_file: "/data/dbserver-1/arangod.log",
                 server_dir: "/data/dbserver-1"
               }
             ]
           }
  end

  test "deployment_started/2 is a pure status transition" do
    Events.deployment_started(CaptureListener, "dep-1")

    assert emitted() == %{event: :deployment_started, deployment_id: "dep-1"}
  end

  test "deployment_stopped/2" do
    Events.deployment_stopped(CaptureListener, "dep-1")

    assert emitted() == %{event: :deployment_stopped, deployment_id: "dep-1"}
  end

  test "server_identified/4" do
    Events.server_identified(CaptureListener, "dep-1", "dbserver-1", "PRMR-abc123")

    assert emitted() == %{
             event: :server_identified,
             deployment_id: "dep-1",
             server_id: "dbserver-1",
             arango_id: "PRMR-abc123"
           }
  end

  test "server_started/5 carries the os pid" do
    Events.server_started(CaptureListener, "dep-1", "dbserver-1", server_instance(), 4242)

    assert emitted() == %{
             event: :server_started,
             deployment_id: "dep-1",
             server_id: "dbserver-1",
             pid: 4242
           }
  end

  test "server_stopped/4" do
    Events.server_stopped(CaptureListener, "dep-1", "dbserver-1", 4242)

    assert emitted() == %{
             event: :server_stopped,
             deployment_id: "dep-1",
             server_id: "dbserver-1",
             pid: 4242,
             reason: nil
           }
  end

  test "server_killed/4" do
    Events.server_killed(CaptureListener, "dep-1", "dbserver-1", 4242)

    assert emitted() == %{
             event: :server_killed,
             deployment_id: "dep-1",
             server_id: "dbserver-1",
             pid: 4242
           }
  end

  test "server_paused/3" do
    Events.server_paused(CaptureListener, "dep-1", "dbserver-1")

    assert emitted() == %{
             event: :server_paused,
             deployment_id: "dep-1",
             server_id: "dbserver-1"
           }
  end

  test "server_resumed/3" do
    Events.server_resumed(CaptureListener, "dep-1", "dbserver-1")

    assert emitted() == %{
             event: :server_resumed,
             deployment_id: "dep-1",
             server_id: "dbserver-1"
           }
  end

  test "server_unhealthy/3" do
    Events.server_unhealthy(CaptureListener, "dep-1", "dbserver-1")

    assert emitted() == %{
             event: :server_unhealthy,
             deployment_id: "dep-1",
             server_id: "dbserver-1"
           }
  end

  test "server_crashed/5 carries crash_info and derives pid from it" do
    crash_info = %Toast.Process.CrashInfo{
      exit_status: 139,
      signal: 11,
      timestamp: 1_781_179_656_208_459,
      os_pid: 4242
    }

    Events.server_crashed(CaptureListener, "dep-1", "dbserver-1", crash_info, false)

    assert emitted() == %{
             event: :server_crashed,
             deployment_id: "dep-1",
             server_id: "dbserver-1",
             pid: 4242,
             crash_info: crash_info,
             expected: false
           }
  end

  test "timeout_kill/5 carries source, reason, and affected servers" do
    servers = [%{server_id: "dbserver-1", os_pid: 4242, log_file: "/data/dbserver-1/arangod.log"}]

    Events.timeout_kill(CaptureListener, "dep-1", :startup, "Startup timeout", servers)

    assert emitted() == %{
             event: :timeout_kill,
             deployment_id: "dep-1",
             source: :startup,
             reason: "Startup timeout",
             servers: servers
           }
  end
end
