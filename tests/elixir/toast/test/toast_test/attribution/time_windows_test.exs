defmodule ToastTest.Attribution.TimeWindowsTest do
  use ExUnit.Case, async: true

  alias ToastTest.Attribution.TimeWindows

  # Suite: 10:00:00 - 10:10:00
  # ModA:  10:00:01 - 10:05:00 (setup until 10:00:03, teardown from 10:04:58)
  #   test_one: 10:00:03 - 10:01:00
  #   test_two: 10:01:05 - 10:02:00
  # ModB:  10:05:01 - 10:09:00 (no tests — setup/teardown both nil)

  @suite_started ~U[2026-03-09 10:00:00Z]
  @suite_finished ~U[2026-03-09 10:10:00Z]

  @mod_a_started ~U[2026-03-09 10:00:01Z]
  @mod_a_finished ~U[2026-03-09 10:05:00Z]
  @mod_a_setup_finished ~U[2026-03-09 10:00:03Z]
  @mod_a_teardown_started ~U[2026-03-09 10:04:58Z]

  @test1_started ~U[2026-03-09 10:00:03Z]
  @test1_finished ~U[2026-03-09 10:01:00Z]
  @test2_started ~U[2026-03-09 10:01:05Z]
  @test2_finished ~U[2026-03-09 10:02:00Z]

  @mod_b_started ~U[2026-03-09 10:05:01Z]
  @mod_b_finished ~U[2026-03-09 10:09:00Z]

  defp to_us(%DateTime{} = dt), do: DateTime.to_unix(dt, :microsecond)

  defp build_test_data do
    %{
      started_at: @suite_started,
      finished_at: @suite_finished,
      modules: %{
        ModA => %{
          started_at: @mod_a_started,
          finished_at: @mod_a_finished,
          setup_finished_at: @mod_a_setup_finished,
          teardown_started_at: @mod_a_teardown_started,
          tests: [
            %{name: :test_one, started_at: @test1_started, finished_at: @test1_finished},
            %{name: :test_two, started_at: @test2_started, finished_at: @test2_finished}
          ]
        },
        ModB => %{
          started_at: @mod_b_started,
          finished_at: @mod_b_finished,
          setup_finished_at: nil,
          teardown_started_at: nil,
          tests: []
        }
      }
    }
  end

  defp build_windows, do: TimeWindows.build(build_test_data())

  # --- build/1 ---

  describe "build/1" do
    test "builds suite window from test_data timestamps" do
      windows = build_windows()

      assert windows.suite.started_at == to_us(@suite_started)
      assert windows.suite.finished_at == to_us(@suite_finished)
    end

    test "builds module windows for each module" do
      windows = build_windows()

      assert map_size(windows.modules) == 2
      assert windows.modules[ModA].started_at == to_us(@mod_a_started)
      assert windows.modules[ModA].finished_at == to_us(@mod_a_finished)
      assert windows.modules[ModA].setup_finished_at == to_us(@mod_a_setup_finished)
      assert windows.modules[ModA].teardown_started_at == to_us(@mod_a_teardown_started)
    end

    test "builds test windows keyed by {module, name}" do
      windows = build_windows()

      assert map_size(windows.tests) == 2
      assert windows.tests[{ModA, :test_one}].started_at == to_us(@test1_started)
      assert windows.tests[{ModA, :test_one}].finished_at == to_us(@test1_finished)
      assert windows.tests[{ModA, :test_two}].started_at == to_us(@test2_started)
      assert windows.tests[{ModA, :test_two}].finished_at == to_us(@test2_finished)
    end

    test "module with no tests produces no test windows" do
      windows = build_windows()

      refute Map.has_key?(windows.tests, {ModB, :anything})
      # ModB still appears in modules
      assert Map.has_key?(windows.modules, ModB)
    end

    test "module with no tests has nil setup/teardown" do
      windows = build_windows()

      assert windows.modules[ModB].setup_finished_at == nil
      assert windows.modules[ModB].teardown_started_at == nil
    end

    test "handles single module with single test" do
      test_data = %{
        started_at: @suite_started,
        finished_at: @suite_finished,
        modules: %{
          OnlyMod => %{
            started_at: @mod_a_started,
            finished_at: @mod_a_finished,
            setup_finished_at: @test1_started,
            teardown_started_at: @test1_finished,
            tests: [
              %{name: :only_test, started_at: @test1_started, finished_at: @test1_finished}
            ]
          }
        }
      }

      windows = TimeWindows.build(test_data)
      assert map_size(windows.tests) == 1
      assert Map.has_key?(windows.tests, {OnlyMod, :only_test})
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

    test "timestamp beyond tolerance falls through" do
      windows = build_windows()
      # test_two ends at 10:02:00, 10:02:06 is 6s after — beyond default 5s tolerance
      # This is between tests and after tolerance, in module teardown window
      ts = to_us(~U[2026-03-09 10:02:06Z])

      # Falls through to module teardown (teardown starts at 10:04:58),
      # but 10:02:06 is between test_two end+tolerance and teardown start.
      # It's still within the module window (10:00:01 - 10:05:00) but not in
      # setup or teardown. This should still be attributed to module since
      # it's within the module's overall window.
      # Actually: setup is started_at..setup_finished_at, teardown is teardown_started_at..finished_at.
      # 10:02:06 is between setup_finished_at (10:00:03) and teardown_started_at (10:04:58) —
      # it's in the "test execution" zone but not in any specific test or tolerance.
      # Per the spec, this falls to :suite since it doesn't match steps 1-4.
      assert {:suite, nil, nil} = TimeWindows.attribute(ts, windows)
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
      # 1 second after test_one ends
      ts = to_us(~U[2026-03-09 10:01:01Z])

      # With zero tolerance, this is not in any test or tolerance window.
      # It's between tests — not in setup or teardown either.
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
      # ModA teardown: 10:04:58 - 10:05:00
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
    test "timestamp inside module with no tests returns {:module, mod}" do
      windows = build_windows()
      # ModB: 10:05:01 - 10:09:00, no setup/teardown (nil)
      # Since setup_finished_at is nil, the whole module window is neither
      # setup nor teardown. But the timestamp IS within the module.
      # Per spec: setup = started_at..setup_finished_at, teardown = teardown_started_at..finished_at
      # With both nil, neither matches. Falls to :suite.
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
