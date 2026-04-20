defmodule Mix.Tasks.Toast.Analyze.WeightsTest do
  use ExUnit.Case, async: true

  alias Mix.Tasks.Toast.Analyze.Weights

  describe "suggest_weights/1" do
    test "empty input returns empty list" do
      assert Weights.suggest_weights(%{}) == []
    end

    test "returns nothing when all weights already match" do
      outcomes = %{
        "smoke" => [
          %{"module" => "Elixir.Smoke.AqlTest", "duration_us" => 10_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.DocTest", "duration_us" => 10_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.GraphTest", "duration_us" => 10_000, "weight" => 1}
        ]
      }

      assert Weights.suggest_weights(outcomes) == []
    end

    test "suggests weight change when current weight differs from calculated" do
      outcomes = %{
        "smoke" => [
          %{"module" => "Elixir.Smoke.FastA", "duration_us" => 1_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.FastB", "duration_us" => 1_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.FastC", "duration_us" => 1_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.HeavyTest", "duration_us" => 10_000, "weight" => 1}
        ]
      }

      result = Weights.suggest_weights(outcomes)
      heavy = Enum.find(result, &(&1.module == "Elixir.Smoke.HeavyTest"))

      assert heavy.suggested_weight == 10
      assert heavy.current_weight == 1
    end

    test "omits module whose current weight already matches suggestion" do
      outcomes = %{
        "smoke" => [
          %{"module" => "Elixir.Smoke.FastA", "duration_us" => 1_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.FastB", "duration_us" => 1_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.FastC", "duration_us" => 1_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.HeavyTest", "duration_us" => 10_000, "weight" => 10}
        ]
      }

      result = Weights.suggest_weights(outcomes)

      # Heavy already has correct weight, fast modules are weight 1 which matches
      assert result == []
    end

    test "modules across multiple suites" do
      outcomes = %{
        "smoke" => [
          %{"module" => "Elixir.Smoke.FastTest", "duration_us" => 1_000, "weight" => 1}
        ],
        "resilience" => [
          %{"module" => "Elixir.Resilience.SlowTest", "duration_us" => 5_000, "weight" => 1}
        ]
      }

      result = Weights.suggest_weights(outcomes)

      # Median of [1000, 5000] = 3000
      # Fast: max(1, round(1000/3000)) = 1 → matches current, omitted
      # Slow: max(1, round(5000/3000)) = 2 → differs from current 1
      assert length(result) == 1
      slow = hd(result)
      assert slow.module == "Elixir.Resilience.SlowTest"
      assert slow.suggested_weight == 2
      assert slow.current_weight == 1
    end

    test "results are sorted by duration descending" do
      outcomes = %{
        "smoke" => [
          %{"module" => "Elixir.Smoke.MediumTest", "duration_us" => 5_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.SlowTest", "duration_us" => 20_000, "weight" => 1}
        ]
      }

      result = Weights.suggest_weights(outcomes)
      durations = Enum.map(result, & &1.duration_us)

      assert durations == Enum.sort(durations, :desc)
    end

    test "multiple tests in same module are summed" do
      outcomes = %{
        "smoke" => [
          %{"module" => "Elixir.Smoke.BigTest", "duration_us" => 3_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.BigTest", "duration_us" => 7_000, "weight" => 1},
          %{"module" => "Elixir.Smoke.SmallTest", "duration_us" => 1_000, "weight" => 1}
        ]
      }

      result = Weights.suggest_weights(outcomes)
      big = Enum.find(result, &(&1.module == "Elixir.Smoke.BigTest"))

      assert big.duration_us == 10_000
    end

    test "defaults to weight 1 when weight field is missing" do
      outcomes = %{
        "smoke" => [
          %{"module" => "Elixir.Smoke.FastA", "duration_us" => 1_000},
          %{"module" => "Elixir.Smoke.FastB", "duration_us" => 1_000},
          %{"module" => "Elixir.Smoke.FastC", "duration_us" => 1_000},
          %{"module" => "Elixir.Smoke.SlowTest", "duration_us" => 10_000}
        ]
      }

      result = Weights.suggest_weights(outcomes)
      slow = Enum.find(result, &(&1.module == "Elixir.Smoke.SlowTest"))

      assert slow.current_weight == 1
      assert slow.suggested_weight == 10
    end
  end
end
