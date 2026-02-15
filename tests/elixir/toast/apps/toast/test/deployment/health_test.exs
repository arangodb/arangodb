defmodule Toast.Deployment.HealthTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.Health

  describe "analyze_agency_status/1" do
    test "returns :ready when all agents agree on leader and have lastAcked" do
      results = [
        {:ok, %{"leaderId" => "agent-1", "lastAcked" => %{}}},
        {:ok, %{"leaderId" => "agent-1"}},
        {:ok, %{"leaderId" => "agent-1"}}
      ]

      assert Health.analyze_agency_status(results) == :ready
    end

    test "returns :not_ready when not all agents responded" do
      results = [
        {:ok, %{"leaderId" => "agent-1", "lastAcked" => %{}}},
        {:error, :econnrefused},
        {:ok, %{"leaderId" => "agent-1"}}
      ]

      assert {:not_ready, :not_all_responding} = Health.analyze_agency_status(results)
    end

    test "returns :not_ready when a leader id is missing" do
      results = [
        {:ok, %{"leaderId" => "agent-1", "lastAcked" => %{}}},
        {:ok, %{"leaderId" => ""}},
        {:ok, %{"leaderId" => "agent-1"}}
      ]

      assert {:not_ready, :missing_leader_id} = Health.analyze_agency_status(results)
    end

    test "returns :not_ready when leaderId key is absent" do
      results = [
        {:ok, %{"leaderId" => "agent-1", "lastAcked" => %{}}},
        {:ok, %{"someOtherKey" => "value"}},
        {:ok, %{"leaderId" => "agent-1"}}
      ]

      assert {:not_ready, :missing_leader_id} = Health.analyze_agency_status(results)
    end

    test "returns :not_ready when no agent has lastAcked" do
      results = [
        {:ok, %{"leaderId" => "agent-1"}},
        {:ok, %{"leaderId" => "agent-1"}},
        {:ok, %{"leaderId" => "agent-1"}}
      ]

      assert {:not_ready, :no_last_acked} = Health.analyze_agency_status(results)
    end

    test "returns :not_ready when agents disagree on leader" do
      results = [
        {:ok, %{"leaderId" => "agent-1", "lastAcked" => %{}}},
        {:ok, %{"leaderId" => "agent-2"}},
        {:ok, %{"leaderId" => "agent-1"}}
      ]

      assert {:not_ready, :leader_disagreement} = Health.analyze_agency_status(results)
    end

    test "returns :ready for single agent with lastAcked" do
      results = [
        {:ok, %{"leaderId" => "agent-1", "lastAcked" => %{}}}
      ]

      assert Health.analyze_agency_status(results) == :ready
    end

    test "returns :not_ready for empty results (no agents)" do
      # Empty list: 0 configs == 0 results (passes), but no lastAcked exists
      assert {:not_ready, :no_last_acked} = Health.analyze_agency_status([])
    end

    test "returns :not_ready when all agents fail" do
      results = [
        {:error, :econnrefused},
        {:error, :timeout}
      ]

      assert {:not_ready, :not_all_responding} = Health.analyze_agency_status(results)
    end
  end
end
