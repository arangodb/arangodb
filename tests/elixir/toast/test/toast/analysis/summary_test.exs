defmodule Toast.Analysis.SummaryTest do
  use ExUnit.Case, async: true

  alias Toast.Analysis.Summary

  defp sample_results do
    %{
      "duration_seconds" => 95.5,
      "summary" => %{
        "total" => 55,
        "passed" => 53,
        "failed" => 2,
        "skipped" => 0
      },
      "suites" => [
        %{
          "name" => "smoke",
          "duration_seconds" => 12.3,
          "tests" => [
            %{"outcome" => "passed"},
            %{"outcome" => "passed"},
            %{"outcome" => "passed"}
          ]
        },
        %{
          "name" => "shell_server",
          "duration_seconds" => 83.2,
          "tests" => [
            %{"outcome" => "passed"},
            %{"outcome" => "failed"},
            %{"outcome" => "failed"}
          ]
        }
      ]
    }
  end

  test "formats totals line" do
    output = Summary.format(sample_results())
    assert output =~ "Total: 55 tests, 53 passed, 2 failed, 0 skipped"
  end

  test "formats duration" do
    output = Summary.format(sample_results())
    assert output =~ "Duration: 1m"
  end

  test "formats suite breakdown" do
    output = Summary.format(sample_results())
    assert output =~ "smoke: 3 passed, 0 failed"
    assert output =~ "shell_server: 1 passed, 2 failed"
  end
end
