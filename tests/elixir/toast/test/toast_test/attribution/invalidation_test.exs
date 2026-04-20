defmodule ToastTest.Attribution.InvalidationTest do
  use ExUnit.Case, async: true

  import ToastTest.TimeTestHelpers, only: [to_us: 1]

  alias Toast.Process.CrashInfo
  alias ToastTest.Attribution.Invalidation
  alias ToastTest.CrashEvent

  # Suite timeline:
  #   test_one: 10:00:03 - 10:01:00     (fails)
  #   test_two: 10:01:05 - 10:02:00     (fails)
  #   test_three: 10:02:05 - 10:03:00   (fails)

  @t1_start ~U[2026-03-09 10:00:03Z]
  @t1_finish ~U[2026-03-09 10:01:00Z]
  @t2_start ~U[2026-03-09 10:01:05Z]
  @t2_finish ~U[2026-03-09 10:02:00Z]
  @t3_start ~U[2026-03-09 10:02:05Z]
  @t3_finish ~U[2026-03-09 10:03:00Z]

  defp make_test_result(name, started_at, finished_at, outcome) do
    %{
      name: name,
      outcome: outcome,
      duration_us: DateTime.diff(finished_at, started_at, :microsecond),
      started_at: started_at,
      finished_at: finished_at,
      tags: %{file: "test.exs", line: 1}
    }
  end

  defp make_exunit_test(module, name) do
    %ExUnit.Test{
      name: name,
      module: module,
      state: {:failed, [{:error, %{message: "boom"}, []}]},
      tags: %{file: "test.exs", line: 1}
    }
  end

  defp build_test_data(test_specs, failures) do
    tests =
      Enum.map(test_specs, fn {name, start, finish, outcome} ->
        make_test_result(name, start, finish, outcome)
      end)

    %{
      modules: %{
        ModA => %{
          started_at: @t1_start,
          finished_at: @t3_finish,
          setup_finished_at: @t1_start,
          teardown_started_at: @t3_finish,
          tests: tests
        }
      },
      failures: failures
    }
  end

  defp crash_event(timestamp) do
    %CrashEvent{
      server_id: "single1",
      crash_info: %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: to_us(timestamp)
      }
    }
  end

  defp fetch_test(test_data, name) do
    test_data.modules[ModA].tests |> Enum.find(&(&1.name == name))
  end

  describe "apply/2 — no crash" do
    test "returns test_data unchanged when no crash events" do
      failure = make_exunit_test(ModA, :test_one)

      test_data =
        build_test_data(
          [{:test_one, @t1_start, @t1_finish, :failed}],
          [failure]
        )

      assert Invalidation.invalidate(test_data, []) == test_data
    end
  end

  describe "apply/2 — test started after crash" do
    test "rewrites outcome :failed -> :invalidated" do
      failure = make_exunit_test(ModA, :test_two)

      test_data =
        build_test_data(
          [
            {:test_one, @t1_start, @t1_finish, :passed},
            {:test_two, @t2_start, @t2_finish, :failed}
          ],
          [failure]
        )

      # Crash happens between test_one finish and test_two start.
      crashes = [crash_event(~U[2026-03-09 10:01:02Z])]

      result = Invalidation.invalidate(test_data, crashes)

      assert fetch_test(result, :test_one).outcome == :passed
      assert fetch_test(result, :test_two).outcome == :invalidated
    end

    test "prunes invalidated tests from failures list" do
      f1 = make_exunit_test(ModA, :test_one)
      f2 = make_exunit_test(ModA, :test_two)

      test_data =
        build_test_data(
          [
            {:test_one, @t1_start, @t1_finish, :failed},
            {:test_two, @t2_start, @t2_finish, :failed}
          ],
          [f1, f2]
        )

      # Crash happens during test_one, before test_two starts.
      crashes = [crash_event(~U[2026-03-09 10:00:30Z])]

      result = Invalidation.invalidate(test_data, crashes)

      # test_one is the "trigger" (started before crash) and stays failed.
      # test_two started after crash and is invalidated.
      assert fetch_test(result, :test_one).outcome == :failed
      assert fetch_test(result, :test_two).outcome == :invalidated

      assert result.failures == [f1]
    end

    test "trigger test (started before crash, finished after) is NOT invalidated" do
      failure = make_exunit_test(ModA, :test_one)

      test_data =
        build_test_data(
          [{:test_one, @t1_start, @t1_finish, :failed}],
          [failure]
        )

      # Crash happens while test_one is still running (between its start and finish).
      crashes = [crash_event(~U[2026-03-09 10:00:30Z])]

      result = Invalidation.invalidate(test_data, crashes)

      assert fetch_test(result, :test_one).outcome == :failed
      assert result.failures == [failure]
    end
  end

  describe "apply/2 — multiple crashes" do
    test "uses the earliest crash as the cutoff" do
      f2 = make_exunit_test(ModA, :test_two)
      f3 = make_exunit_test(ModA, :test_three)

      test_data =
        build_test_data(
          [
            {:test_one, @t1_start, @t1_finish, :passed},
            {:test_two, @t2_start, @t2_finish, :failed},
            {:test_three, @t3_start, @t3_finish, :failed}
          ],
          [f2, f3]
        )

      # Earliest crash at 10:01:02 (before test_two starts).
      # A later crash event at 10:02:30 shouldn't raise the cutoff.
      crashes = [
        crash_event(~U[2026-03-09 10:02:30Z]),
        crash_event(~U[2026-03-09 10:01:02Z])
      ]

      result = Invalidation.invalidate(test_data, crashes)

      assert fetch_test(result, :test_two).outcome == :invalidated
      assert fetch_test(result, :test_three).outcome == :invalidated
      assert result.failures == []
    end
  end

  describe "apply/2 — edge cases" do
    test "non-failed outcomes are not rewritten even if after crash" do
      test_data =
        build_test_data(
          [
            {:test_one, @t1_start, @t1_finish, :passed},
            {:test_two, @t2_start, @t2_finish, :skipped}
          ],
          []
        )

      crashes = [crash_event(~U[2026-03-09 10:00:30Z])]

      result = Invalidation.invalidate(test_data, crashes)

      assert fetch_test(result, :test_one).outcome == :passed
      assert fetch_test(result, :test_two).outcome == :skipped
    end

    test "failed test with missing started_at is not invalidated" do
      failure = make_exunit_test(ModA, :test_one)

      test_data = %{
        modules: %{
          ModA => %{
            started_at: @t1_start,
            finished_at: @t1_finish,
            setup_finished_at: @t1_start,
            teardown_started_at: @t1_finish,
            tests: [
              %{
                name: :test_one,
                outcome: :failed,
                duration_us: 0,
                started_at: nil,
                finished_at: nil,
                tags: %{}
              }
            ]
          }
        },
        failures: [failure]
      }

      crashes = [crash_event(~U[2026-03-09 09:00:00Z])]

      result = Invalidation.invalidate(test_data, crashes)

      assert fetch_test(result, :test_one).outcome == :failed
      assert result.failures == [failure]
    end
  end
end
