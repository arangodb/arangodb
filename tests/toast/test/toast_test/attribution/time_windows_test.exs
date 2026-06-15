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

defmodule ToastTest.Attribution.TimeWindowsTest do
  use ExUnit.Case, async: true

  alias ToastTest.Attribution.TimeWindows

  import ToastTest.TimeTestHelpers, only: [to_us: 1]

  # Timeline:
  # ModA:  10:00:01 - 10:05:00
  #   test_one: 10:00:03 - 10:01:00   (setup ends at first test_started)
  #   test_two: 10:01:05 - 10:02:00   (teardown begins at last test_finished)
  # ModB:  10:05:01 - 10:09:00 (no tests — setup/teardown both nil)

  @mod_a_started ~U[2026-03-09 10:00:01Z]
  @mod_a_finished ~U[2026-03-09 10:05:00Z]

  @test1_started ~U[2026-03-09 10:00:03Z]
  @test1_finished ~U[2026-03-09 10:01:00Z]
  @test2_started ~U[2026-03-09 10:01:05Z]
  @test2_finished ~U[2026-03-09 10:02:00Z]

  @mod_b_started ~U[2026-03-09 10:05:01Z]
  @mod_b_finished ~U[2026-03-09 10:09:00Z]

  defp event(type, ts, fields) do
    Map.merge(%{event: type, timestamp: to_us(ts)}, fields)
  end

  defp build_events do
    [
      event(:module_started, @mod_a_started, %{module: ModA}),
      event(:test_started, @test1_started, %{module: ModA, name: :test_one}),
      event(:test_finished, @test1_finished, %{
        module: ModA,
        name: :test_one,
        outcome: :passed,
        duration_us: 57_000_000
      }),
      event(:test_started, @test2_started, %{module: ModA, name: :test_two}),
      event(:test_finished, @test2_finished, %{
        module: ModA,
        name: :test_two,
        outcome: :passed,
        duration_us: 55_000_000
      }),
      event(:module_finished, @mod_a_finished, %{module: ModA}),
      event(:module_started, @mod_b_started, %{module: ModB}),
      event(:module_finished, @mod_b_finished, %{module: ModB})
    ]
  end

  defp build_windows, do: TimeWindows.build(build_events())

  # --- build/1 ---

  describe "build/1" do
    test "builds module windows for each module" do
      windows = build_windows()

      assert map_size(windows.modules) == 2
      assert windows.modules[ModA].started_at == to_us(@mod_a_started)
      assert windows.modules[ModA].finished_at == to_us(@mod_a_finished)
    end

    test "module setup ends at the first test_started" do
      windows = build_windows()
      assert windows.modules[ModA].setup_finished_at == to_us(@test1_started)
    end

    test "module teardown begins at the last test_finished" do
      windows = build_windows()
      assert windows.modules[ModA].teardown_started_at == to_us(@test2_finished)
    end

    test "builds test windows keyed by {module, name}" do
      windows = build_windows()
      assert map_size(windows.tests) == 2
      assert windows.tests[{ModA, :test_one}].started_at == to_us(@test1_started)
      assert windows.tests[{ModA, :test_one}].finished_at == to_us(@test1_finished)
      assert windows.tests[{ModA, :test_two}].started_at == to_us(@test2_started)
      assert windows.tests[{ModA, :test_two}].finished_at == to_us(@test2_finished)
    end

    test "module with no tests produces no test windows and nil setup/teardown" do
      windows = build_windows()
      refute Map.has_key?(windows.tests, {ModB, :anything})
      assert windows.modules[ModB].setup_finished_at == nil
      assert windows.modules[ModB].teardown_started_at == nil
    end

    test "test window finished_at extends to between_tests_finished when present" do
      barrier_finished = ~U[2026-03-09 10:01:04Z]

      events =
        build_events() ++
          [event(:between_tests_finished, barrier_finished, %{module: ModA, name: :test_one})]

      windows = TimeWindows.build(events)
      assert windows.tests[{ModA, :test_one}].started_at == to_us(@test1_started)
      assert windows.tests[{ModA, :test_one}].finished_at == to_us(barrier_finished)
      # other tests unaffected
      assert windows.tests[{ModA, :test_two}].finished_at == to_us(@test2_finished)
    end

    test "between_tests_finished for an unknown test is ignored" do
      events =
        build_events() ++
          [event(:between_tests_finished, @test2_finished, %{module: Ghost, name: :nope})]

      windows = TimeWindows.build(events)
      refute Map.has_key?(windows.tests, {Ghost, :nope})
    end

    test "a test that never finished produces no window" do
      events = [
        event(:module_started, @mod_a_started, %{module: ModA}),
        event(:test_started, @test1_started, %{module: ModA, name: :aborted_test})
      ]

      windows = TimeWindows.build(events)
      refute Map.has_key?(windows.tests, {ModA, :aborted_test})
      # but it still marks the end of module setup
      assert windows.modules[ModA].setup_finished_at == to_us(@test1_started)
    end

    test "unrelated events are ignored" do
      events =
        build_events() ++
          [
            event(:server_started, @test1_started, %{server_id: "s1", pid: 1}),
            event(:netstat_snapshot, @test2_started, %{total: 100, label: nil}),
            event(:custom, @test2_finished, %{kind: :checkpoint, payload: %{}})
          ]

      assert TimeWindows.build(events) == build_windows()
    end
  end

  describe "attribute/3 — extended window via between_tests_finished" do
    test "timestamp between test end and barrier end returns :high" do
      # Simulates a crash whose :DOWN arrived post-coredump: the physical
      # timestamp is after the test finished, but within the between-tests
      # barrier window that we assert as still attributable to the test.
      barrier_finished = ~U[2026-03-09 10:01:04Z]

      events =
        build_events() ++
          [event(:between_tests_finished, barrier_finished, %{module: ModA, name: :test_one})]

      windows = TimeWindows.build(events)

      # test_one physically finished at 10:01:00; barrier returned at 10:01:04.
      # A timestamp at 10:01:03 used to be `:low` via tolerance; now it's `:high`.
      ts = to_us(~U[2026-03-09 10:01:03Z])
      assert {{:test, ModA, :test_one}, :high, nil} = TimeWindows.attribute(ts, windows)
    end
  end

  # --- attribute/3 ---

  describe "attribute/3 — test window (high confidence)" do
    test "timestamp inside test window returns {:test, mod, name} with :high" do
      windows = build_windows()
      # 10:00:30 is inside test_one (10:00:03 - 10:01:00)
      ts = to_us(~U[2026-03-09 10:00:30Z])

      assert {{:test, ModA, :test_one}, :high, nil} = TimeWindows.attribute(ts, windows)
    end

    test "timestamp exactly at test started_at returns :high" do
      windows = build_windows()

      assert {{:test, ModA, :test_one}, :high, nil} =
               TimeWindows.attribute(to_us(@test1_started), windows)
    end

    test "timestamp exactly at test finished_at returns :high" do
      windows = build_windows()

      assert {{:test, ModA, :test_one}, :high, nil} =
               TimeWindows.attribute(to_us(@test1_finished), windows)
    end

    test "timestamp inside second test returns that test" do
      windows = build_windows()
      ts = to_us(~U[2026-03-09 10:01:30Z])

      assert {{:test, ModA, :test_two}, :high, nil} = TimeWindows.attribute(ts, windows)
    end
  end

  describe "attribute/3 — tolerance window (low confidence)" do
    test "timestamp within 5 seconds after test end returns :low" do
      windows = build_windows()
      # test_one ends at 10:01:00, so 10:01:03 is 3s after
      ts = to_us(~U[2026-03-09 10:01:03Z])

      assert {{:test, ModA, :test_one}, :low, nil} = TimeWindows.attribute(ts, windows)
    end

    test "timestamp exactly 5 seconds after test end returns :low" do
      windows = build_windows()
      # test_one ends at 10:01:00, exactly 5s after
      ts = to_us(~U[2026-03-09 10:01:05Z])

      # 5s is within default tolerance (<=5s), but test_two starts at 10:01:05
      # so test_two's :high match should win over test_one's :low
      assert {{:test, ModA, :test_two}, :high, nil} = TimeWindows.attribute(ts, windows)
    end

    test "timestamp beyond tolerance lands in module teardown" do
      windows = build_windows()
      # test_two (the last test) ends at 10:02:00; 10:02:06 is 6s after —
      # beyond the 5s tolerance. Module teardown runs from the last
      # test_finished until module_finished, so it attributes to teardown.
      ts = to_us(~U[2026-03-09 10:02:06Z])

      assert {{:module, ModA}, nil, :teardown} = TimeWindows.attribute(ts, windows)
    end

    test "custom tolerance_us option" do
      windows = build_windows()
      # test_two ends at 10:02:00, 10:02:08 is 8s after — within 10s tolerance
      ts = to_us(~U[2026-03-09 10:02:08Z])

      assert {{:test, ModA, :test_two}, :low, nil} =
               TimeWindows.attribute(ts, windows, tolerance_us: 10_000_000)
    end

    test "zero tolerance means no low-confidence matches" do
      windows = build_windows()
      # 1 second after test_one ends — between tests, before teardown begins
      ts = to_us(~U[2026-03-09 10:01:01Z])

      assert {:suite, nil, nil} = TimeWindows.attribute(ts, windows, tolerance_us: 0)
    end
  end

  describe "attribute/3 — module setup window" do
    test "timestamp in setup window returns {:module, mod} with phase :setup" do
      windows = build_windows()
      # ModA setup: 10:00:01 - 10:00:03
      ts = to_us(~U[2026-03-09 10:00:02Z])

      assert {{:module, ModA}, nil, :setup} = TimeWindows.attribute(ts, windows)
    end

    test "timestamp at module started_at (setup start) returns module with phase :setup" do
      windows = build_windows()

      assert {{:module, ModA}, nil, :setup} =
               TimeWindows.attribute(to_us(@mod_a_started), windows)
    end
  end

  describe "attribute/3 — module teardown window" do
    test "timestamp in teardown window returns {:module, mod} with phase :teardown" do
      windows = build_windows()
      # ModA teardown: 10:02:00 (last test_finished) - 10:05:00
      ts = to_us(~U[2026-03-09 10:04:59Z])

      assert {{:module, ModA}, nil, :teardown} = TimeWindows.attribute(ts, windows)
    end

    test "timestamp at module finished_at returns module with phase :teardown" do
      windows = build_windows()

      assert {{:module, ModA}, nil, :teardown} =
               TimeWindows.attribute(to_us(@mod_a_finished), windows)
    end
  end

  describe "attribute/3 — suite fallback" do
    test "timestamp outside all module windows returns :suite with nil" do
      windows = build_windows()
      # Between ModA (ends 10:05:00) and ModB (starts 10:05:01)
      ts = to_us(~U[2026-03-09 10:05:00.500000Z])

      assert {:suite, nil, nil} = TimeWindows.attribute(ts, windows)
    end

    test "timestamp before first module returns :suite with phase :startup" do
      windows = build_windows()
      ts = to_us(~U[2026-03-09 09:59:00Z])

      assert {:suite, nil, :startup} = TimeWindows.attribute(ts, windows)
    end

    test "timestamp after last module returns :suite with phase :shutdown" do
      windows = build_windows()
      ts = to_us(~U[2026-03-09 10:15:00Z])

      assert {:suite, nil, :shutdown} = TimeWindows.attribute(ts, windows)
    end
  end

  describe "attribute/3 — module with no tests" do
    test "timestamp inside module with no tests returns :suite" do
      windows = build_windows()
      # ModB: 10:05:01 - 10:09:00 has no tests, so setup_finished_at and
      # teardown_started_at are nil — neither setup nor teardown matches.
      ts = to_us(~U[2026-03-09 10:06:00Z])

      assert {:suite, nil, nil} = TimeWindows.attribute(ts, windows)
    end
  end

  describe "attribute/3 — priority: high test match wins over low" do
    test "when timestamp is in one test and in tolerance of another, high wins" do
      windows = build_windows()
      # test_two starts at 10:01:05 — this is exactly at test_two start
      # and within 5s of test_one end (10:01:00)
      # :high for test_two should win over :low for test_one

      assert {{:test, ModA, :test_two}, :high, nil} =
               TimeWindows.attribute(to_us(@test2_started), windows)
    end
  end
end
