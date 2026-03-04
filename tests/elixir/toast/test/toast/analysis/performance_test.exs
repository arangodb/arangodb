defmodule Toast.Analysis.PerformanceTest do
  use ExUnit.Case, async: true

  alias Toast.Analysis.Performance

  defp sample_results do
    %{
      "modules" => %{
        "A" => %{
          "tests" => [
            %{
              "module" => "A",
              "name" => "fast",
              "outcome" => "passed",
              "duration_seconds" => 0.1
            },
            %{
              "module" => "A",
              "name" => "medium",
              "outcome" => "passed",
              "duration_seconds" => 3.5
            }
          ]
        },
        "B" => %{
          "tests" => [
            %{
              "module" => "B",
              "name" => "slow",
              "outcome" => "passed",
              "duration_seconds" => 12.345
            },
            %{
              "module" => "B",
              "name" => "very_slow",
              "outcome" => "passed",
              "duration_seconds" => 45.0
            }
          ]
        }
      }
    }
  end

  test "shows slowest N tests in order" do
    output = Performance.format(sample_results(), 2)
    assert output =~ "1. B - very_slow (45.0s)"
    assert output =~ "2. B - slow (12.345s)"
    refute output =~ "fast"
  end

  test "shows duration distribution" do
    output = Performance.format(sample_results())
    assert output =~ "<1s: 1 tests"
    assert output =~ "1-5s: 1 tests"
    assert output =~ "5-30s: 1 tests"
    assert output =~ ">30s: 1 tests"
  end

  test "handles empty results" do
    output = Performance.format(%{"modules" => %{}})
    assert output =~ "No tests found"
  end
end
