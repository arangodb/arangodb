defmodule Toast.Analysis.SummaryTest do
  use ExUnit.Case, async: true

  alias Toast.Analysis.Summary

  defp sample_results do
    %{
      "test_run" => %{"duration_seconds" => 95.5},
      "summary" => %{
        "total" => 55,
        "passed" => 53,
        "failed" => 2,
        "skipped" => 0
      },
      "modules" => %{
        "Elixir.Smoke.VersionTest" => %{
          "duration_seconds" => 12.3,
          "tests" => [
            %{"outcome" => "passed"},
            %{"outcome" => "passed"},
            %{"outcome" => "passed"}
          ]
        },
        "Elixir.ShellServer.CrudTest" => %{
          "duration_seconds" => 83.2,
          "tests" => [
            %{"outcome" => "passed"},
            %{"outcome" => "failed"},
            %{"outcome" => "failed"}
          ]
        }
      }
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

  test "formats module breakdown" do
    output = Summary.format(sample_results())
    assert output =~ "Elixir.Smoke.VersionTest: 3 passed, 0 failed"
    assert output =~ "Elixir.ShellServer.CrudTest: 1 passed, 2 failed"
  end
end
