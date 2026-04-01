defmodule ToastTest.Enrichment.SanitizerPrecisionTest do
  @moduledoc """
  Tests for sub-second mtime precision in sanitizer file reading and its
  effect on time-window attribution.

  The core bug: File.stat only gives second precision, so a sanitizer file
  written at 14:08:29.820 was attributed to a test ending at 14:08:29.001
  instead of one starting at 14:08:29.006. The fix uses stat(1) with
  sub-second output to get microsecond precision.
  """
  use ExUnit.Case, async: true

  alias ToastTest.Enrichment.Sanitizer
  alias ToastTest.Attribution.TimeWindows

  import ToastTest.TimeTestHelpers, only: [to_us: 1]

  @tmp_dir Path.join(
             System.tmp_dir!(),
             "toast_sanitizer_precision_test_#{System.unique_integer([:positive])}"
           )

  setup do
    File.mkdir_p!(@tmp_dir)
    on_exit(fn -> File.rm_rf!(@tmp_dir) end)
    {:ok, tmp_dir: @tmp_dir}
  end

  describe "read/1 microsecond precision" do
    test "timestamp preserves sub-second component from known mtime", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.1")
      File.write!(path, "sanitizer output")

      System.cmd("touch", ["-d", "2026-01-15T10:00:05.820000Z", path])

      assert {:ok, result} = Sanitizer.read(path)
      assert is_integer(result.timestamp)
      assert rem(result.timestamp, 1_000_000) == 820_000
    end

    test "timestamp seconds and microseconds are both correct", %{tmp_dir: dir} do
      path = Path.join(dir, "alubsan.log.2")
      File.write!(path, "content")

      System.cmd("touch", ["-d", "2026-01-15T10:00:05.123456Z", path])

      assert {:ok, result} = Sanitizer.read(path)
      expected = to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:05.123456]))
      assert result.timestamp == expected
    end
  end

  describe "sub-second attribution via TimeWindows" do
    # Reproduces the real bug scenario:
    # test_a: 10:00:28.000 - 10:00:29.001
    # test_b: 10:00:29.006 - 10:00:30.000
    # sanitizer file mtime: 10:00:29.820
    #
    # With second-precision (truncated to 10:00:29.000), the timestamp
    # falls within test_a's window [28.000, 29.001] because 29.000 <= 29.001.
    # With microsecond precision, 29.820 > 29.001, so it correctly falls
    # into test_b's window [29.006, 30.000].

    @test_a_start DateTime.to_unix(
                    DateTime.new!(~D[2026-01-15], ~T[10:00:28.000000]),
                    :microsecond
                  )
    @test_a_end DateTime.to_unix(DateTime.new!(~D[2026-01-15], ~T[10:00:29.001000]), :microsecond)
    @test_b_start DateTime.to_unix(
                    DateTime.new!(~D[2026-01-15], ~T[10:00:29.006000]),
                    :microsecond
                  )
    @test_b_end DateTime.to_unix(DateTime.new!(~D[2026-01-15], ~T[10:00:30.000000]), :microsecond)

    defp build_windows do
      %{
        suite: %{
          started_at: to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:00.000000])),
          finished_at: to_us(DateTime.new!(~D[2026-01-15], ~T[10:01:00.000000]))
        },
        modules: %{},
        tests: %{
          {TestMod, :test_a} => %{started_at: @test_a_start, finished_at: @test_a_end},
          {TestMod, :test_b} => %{started_at: @test_b_start, finished_at: @test_b_end}
        }
      }
    end

    test "microsecond timestamp attributes to correct test window" do
      # 10:00:29.820 should match test_b, not test_a
      timestamp = to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:29.820000]))
      windows = build_windows()

      {scope, confidence, _phase} = TimeWindows.attribute(timestamp, windows)

      assert scope == {:test, TestMod, :test_b}
      assert confidence == :high
    end

    test "second-truncated timestamp wrongly matches test_a (demonstrates the bug)" do
      # Truncating to seconds gives 10:00:29.000, which falls within
      # test_a's window [28.000, 29.001] — this is the misattribution
      truncated = to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:29.000000]))
      windows = build_windows()

      {scope, _confidence, _phase} = TimeWindows.attribute(truncated, windows)

      assert scope == {:test, TestMod, :test_a}
    end

    test "timestamp at exact boundary of test_a end is attributed to test_a" do
      windows = build_windows()

      {scope, confidence, _phase} = TimeWindows.attribute(@test_a_end, windows)

      assert scope == {:test, TestMod, :test_a}
      assert confidence == :high
    end

    test "timestamp in gap between tests gets low-confidence attribution" do
      # 10:00:29.003 is after test_a ends but before test_b starts
      gap_timestamp = to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:29.003000]))
      windows = build_windows()

      {scope, confidence, _phase} = TimeWindows.attribute(gap_timestamp, windows)

      assert scope == {:test, TestMod, :test_a}
      assert confidence == :low
    end
  end

  describe "end-to-end: read/1 + attribute" do
    test "file with sub-second mtime is attributed to correct test", %{tmp_dir: dir} do
      path = Path.join(dir, "alubsan.log.1")
      File.write!(path, "ERROR: sanitizer report")

      # Set mtime to 10:00:29.820 — should attribute to test_b
      System.cmd("touch", ["-d", "2026-01-15T10:00:29.820000Z", path])

      assert {:ok, result} = Sanitizer.read(path)

      windows = %{
        suite: %{
          started_at: to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:00.000000])),
          finished_at: to_us(DateTime.new!(~D[2026-01-15], ~T[10:01:00.000000]))
        },
        modules: %{},
        tests: %{
          {TestMod, :test_a} => %{
            started_at: to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:28.000000])),
            finished_at: to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:29.001000]))
          },
          {TestMod, :test_b} => %{
            started_at: to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:29.006000])),
            finished_at: to_us(DateTime.new!(~D[2026-01-15], ~T[10:00:30.000000]))
          }
        }
      }

      {scope, confidence, _phase} = TimeWindows.attribute(result.timestamp, windows)

      assert scope == {:test, TestMod, :test_b}
      assert confidence == :high
    end
  end
end
