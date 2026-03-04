defmodule Toast.Analysis.FailuresTest do
  use ExUnit.Case, async: true

  alias Toast.Analysis.Failures

  test "no failures returns message" do
    results = %{
      "modules" => %{
        "Elixir.Smoke.VersionTest" => %{"tests" => [%{"outcome" => "passed"}]}
      }
    }

    assert Failures.format(results) == "No failures."
  end

  test "formats failure details" do
    results = %{
      "modules" => %{
        "Elixir.Smoke.VersionTest" => %{
          "tests" => [
            %{
              "module" => "Elixir.Smoke.VersionTest",
              "name" => "fails on purpose",
              "outcome" => "failed",
              "duration_seconds" => 0.123,
              "failure" => %{"message" => "Expected true, got false"}
            }
          ]
        }
      }
    }

    output = Failures.format(results)
    assert output =~ "1 failure(s)"
    assert output =~ "Smoke.VersionTest"
    assert output =~ "fails on purpose"
    assert output =~ "Expected true, got false"
  end

  test "formats multiple failures with index" do
    results = %{
      "modules" => %{
        "A" => %{
          "tests" => [
            %{
              "module" => "A",
              "name" => "t1",
              "outcome" => "failed",
              "failure" => %{"message" => "err1"}
            }
          ]
        },
        "B" => %{
          "tests" => [
            %{
              "module" => "B",
              "name" => "t2",
              "outcome" => "failed",
              "failure" => %{"message" => "err2"}
            }
          ]
        }
      }
    }

    output = Failures.format(results)
    assert output =~ "1) A - t1"
    assert output =~ "2) B - t2"
  end
end
