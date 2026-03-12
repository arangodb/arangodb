defmodule Toast.Deployment.StopTest.MockController do
  @moduledoc false
  use GenServer

  def start_link(opts \\ []) do
    responses = Keyword.get(opts, :responses, %{})
    GenServer.start_link(__MODULE__, %{calls: [], responses: responses})
  end

  def calls(pid), do: GenServer.call(pid, :get_calls)

  @impl true
  def init(state), do: {:ok, state}

  @impl true
  def handle_call(:get_calls, _from, state) do
    {:reply, Enum.reverse(state.calls), state}
  end

  def handle_call({:shutdown, _timeout} = msg, _from, state) do
    response = Map.get(state.responses, :shutdown, :ok)
    {:reply, response, %{state | calls: [msg | state.calls]}}
  end

  def handle_call(:dump_agency = msg, _from, state) do
    response = Map.get(state.responses, :dump_agency, %{"agency_data" => "test"})
    {:reply, response, %{state | calls: [msg | state.calls]}}
  end

  def handle_call(:get_info = msg, _from, state) do
    response = Map.get(state.responses, :get_info, %{servers: %{}, error: nil})
    {:reply, response, %{state | calls: [msg | state.calls]}}
  end

  def handle_call(msg, _from, state) do
    {:reply, :ok, %{state | calls: [msg | state.calls]}}
  end
end

defmodule Toast.Deployment.StopTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment
  alias Toast.Deployment.{ServerInstance, StopTest.MockController}

  defp deployment(pid, mode \\ :single_server) do
    %Deployment{
      id: "test-deploy",
      mode: mode,
      config: %Toast.Config{},
      controller: pid,
      endpoint: "http://localhost:1"
    }
  end

  defp server_instance(id, role, overrides \\ []) do
    defaults = [id: id, role: role, operational_state: :stopped, pid: nil]
    struct!(ServerInstance, Keyword.merge(defaults, overrides))
  end

  # --- stop/2 return format ---

  describe "stop/2 clean shutdown" do
    test "returns {:ok, stop_info} with servers and nil error" do
      servers = %{
        "single" => server_instance("single", :single, operational_state: :stopped)
      }

      {:ok, pid} =
        MockController.start_link(
          responses: %{
            shutdown: :ok,
            get_info: %{servers: servers, error: nil}
          }
        )

      result = Deployment.stop(deployment(pid))

      assert {:ok, stop_info} = result
      assert stop_info.error == nil
      assert %{"single" => %ServerInstance{id: "single", role: :single}} = stop_info.servers
    end

    test "stop_info.servers contains all server instances" do
      servers = %{
        "agent-0" => server_instance("agent-0", :agent),
        "dbserver-0" => server_instance("dbserver-0", :dbserver),
        "coordinator-0" => server_instance("coordinator-0", :coordinator)
      }

      {:ok, pid} =
        MockController.start_link(
          responses: %{shutdown: :ok, get_info: %{servers: servers, error: nil}}
        )

      {:ok, stop_info} = Deployment.stop(deployment(pid, :cluster))

      assert map_size(stop_info.servers) == 3
      assert stop_info.servers["agent-0"].role == :agent
      assert stop_info.servers["dbserver-0"].role == :dbserver
      assert stop_info.servers["coordinator-0"].role == :coordinator
    end

    test "passes timeout option to controller shutdown" do
      {:ok, pid} =
        MockController.start_link(
          responses: %{shutdown: :ok, get_info: %{servers: %{}, error: nil}}
        )

      Deployment.stop(deployment(pid), timeout: 45_000)

      assert [{:shutdown, 45_000} | _] = MockController.calls(pid)
    end

    test "uses shutdown_timeout from config" do
      {:ok, default_pid} =
        MockController.start_link(
          responses: %{shutdown: :ok, get_info: %{servers: %{}, error: nil}}
        )

      {:ok, custom_pid} =
        MockController.start_link(
          responses: %{shutdown: :ok, get_info: %{servers: %{}, error: nil}}
        )

      Deployment.stop(deployment(default_pid))

      custom_config = %Toast.Config{shutdown_timeout: 15_000}
      custom_deployment = %{deployment(custom_pid) | config: custom_config}
      Deployment.stop(custom_deployment)

      assert [{:shutdown, 60_000} | _] = MockController.calls(default_pid)
      assert [{:shutdown, 15_000} | _] = MockController.calls(custom_pid)
    end
  end

  describe "stop/2 with recorded error" do
    test "returns {:ok, stop_info} when shutdown succeeds but controller has error" do
      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: ~U[2026-01-15 12:00:00Z]
      }

      error = {:server_crashed, "single", crash_info}

      servers = %{
        "single" => server_instance("single", :single, operational_state: :crashed)
      }

      {:ok, pid} =
        MockController.start_link(
          responses: %{shutdown: :ok, get_info: %{servers: servers, error: error}}
        )

      assert {:ok, stop_info} = Deployment.stop(deployment(pid))
      assert {:server_crashed, "single", ^crash_info} = stop_info.error
      assert stop_info.servers["single"].operational_state == :crashed
    end
  end

  describe "stop/2 with shutdown failure" do
    test "returns {:error, reason, stop_info} when shutdown fails" do
      servers = %{
        "single" => server_instance("single", :single, operational_state: :running)
      }

      {:ok, pid} =
        MockController.start_link(
          responses: %{
            shutdown: {:error, :timeout},
            get_info: %{servers: servers, error: nil}
          }
        )

      assert {:error, :timeout, stop_info} = Deployment.stop(deployment(pid))
      assert stop_info.servers["single"].id == "single"
    end
  end

  describe "stop/2 with dead controller" do
    test "returns error when controller is already dead" do
      dead = spawn(fn -> :ok end)
      Process.sleep(50)

      result = Deployment.stop(deployment(dead))

      assert {:error, :controller_dead, stop_info} = result
      assert stop_info.servers == %{}
    end
  end

  # --- dump_agency/2 ---

  describe "dump_agency/2 for single_server" do
    test "returns {:error, :not_cluster}" do
      {:ok, pid} = MockController.start_link()

      assert {:error, :not_cluster} = Deployment.dump_agency(deployment(pid, :single_server))

      # Should not have called the controller at all
      assert MockController.calls(pid) == []
    end
  end

  describe "dump_agency/2 for cluster" do
    test "delegates to controller and returns :ok" do
      {:ok, pid} =
        MockController.start_link(responses: %{dump_agency: %{"agency_data" => "test"}})

      assert :ok = Deployment.dump_agency(deployment(pid, :cluster))
      assert [:dump_agency] = MockController.calls(pid)
    end

    test "passes timeout option" do
      {:ok, pid} = MockController.start_link()

      Deployment.dump_agency(deployment(pid, :cluster), timeout: 90_000)

      # The timeout is passed to GenServer.call, not as part of the message,
      # so we just verify the call was made
      assert [:dump_agency] = MockController.calls(pid)
    end

    test "returns {:error, :controller_dead} when controller is dead" do
      dead = spawn(fn -> :ok end)
      Process.sleep(50)

      assert {:error, _reason} = Deployment.dump_agency(deployment(dead, :cluster))
    end
  end
end
