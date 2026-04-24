defmodule Toast.Utils.PollingTest do
  use ExUnit.Case, async: true

  alias Toast.Utils.Polling

  defp now_ms, do: System.monotonic_time(:millisecond)

  describe "poll_until/3" do
    test "returns {:ok, result} immediately when the first probe succeeds" do
      probe = fn -> {:done, :first_try} end
      deadline = now_ms() + 1_000

      assert Polling.poll_until(probe, deadline, 100) == {:ok, :first_try}
    end

    test "keeps polling until probe reports :done" do
      {:ok, counter} = Agent.start_link(fn -> 0 end)

      probe = fn ->
        n = Agent.get_and_update(counter, fn n -> {n + 1, n + 1} end)
        if n >= 3, do: {:done, n}, else: :not_ready
      end

      deadline = now_ms() + 1_000

      assert Polling.poll_until(probe, deadline, 10) == {:ok, 3}
    end

    test "returns :timeout when deadline passes without :done" do
      probe = fn -> :not_ready end
      deadline = now_ms() + 100

      assert Polling.poll_until(probe, deadline, 20) == {:error, :timeout}
    end

    test "sleeps at most until the deadline, never past it" do
      probe = fn -> :not_ready end
      deadline = now_ms() + 50

      started = now_ms()
      assert Polling.poll_until(probe, deadline, 10_000) == {:error, :timeout}
      elapsed = now_ms() - started

      # The sleep should clamp to the remaining budget (~50ms), not the full 10s.
      assert elapsed < 500
    end

    test "evaluates probe before sleeping (probe-first)" do
      {:ok, tracker} = Agent.start_link(fn -> [] end)

      probe = fn ->
        Agent.update(tracker, fn events -> [{:probe, now_ms()} | events] end)
        {:done, :ok}
      end

      deadline = now_ms() + 1_000
      Polling.poll_until(probe, deadline, 1_000)

      events = Agent.get(tracker, & &1)
      # Exactly one probe, no sleep before it.
      assert length(events) == 1
    end
  end
end
