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

defmodule Mix.Tasks.Toast.Analyze.WeightsTest do
  use ExUnit.Case, async: true

  alias Mix.Tasks.Toast.Analyze.Weights

  # A fake module with a custom weight for testing
  defmodule HeavyModule do
    def __toast_weight__, do: 10
  end

  defp make_result(suite, modules_map) do
    %ToastTest.SuiteResult{
      suite: suite,
      started_at: DateTime.utc_now(),
      finished_at: DateTime.utc_now(),
      times_us: %{async: nil, load: nil, run: 0},
      modules: modules_map
    }
  end

  defp make_tests(durations) do
    durations
    |> Enum.with_index()
    |> Enum.map(fn {dur, i} ->
      %{
        name: :"test_#{i}",
        outcome: :passed,
        duration_us: dur,
        started_at: nil,
        finished_at: nil,
        tags: %{}
      }
    end)
  end

  describe "suggest_weights/1" do
    test "empty input returns empty list" do
      assert Weights.suggest_weights([]) == []
    end

    test "returns nothing when all weights already match" do
      # All modules unknown (weight defaults to 1) with equal durations
      result =
        make_result("smoke", %{
          :"Elixir.Smoke.AqlTest" => %{tests: make_tests([10_000])},
          :"Elixir.Smoke.DocTest" => %{tests: make_tests([10_000])},
          :"Elixir.Smoke.GraphTest" => %{tests: make_tests([10_000])}
        })

      assert Weights.suggest_weights([result]) == []
    end

    test "suggests weight change when current weight differs from calculated" do
      result =
        make_result("smoke", %{
          :"Elixir.Smoke.FastA" => %{tests: make_tests([1_000])},
          :"Elixir.Smoke.FastB" => %{tests: make_tests([1_000])},
          :"Elixir.Smoke.FastC" => %{tests: make_tests([1_000])},
          :"Elixir.Smoke.HeavyTest" => %{tests: make_tests([10_000])}
        })

      result = Weights.suggest_weights([result])
      heavy = Enum.find(result, &(&1.module == "Elixir.Smoke.HeavyTest"))

      assert heavy.suggested_weight == 10
      assert heavy.current_weight == 1
    end

    test "omits module whose current weight already matches suggestion" do
      # HeavyModule has __toast_weight__ returning 10
      heavy_mod = __MODULE__.HeavyModule

      result =
        make_result("smoke", %{
          :"Elixir.Smoke.FastA" => %{tests: make_tests([1_000])},
          :"Elixir.Smoke.FastB" => %{tests: make_tests([1_000])},
          :"Elixir.Smoke.FastC" => %{tests: make_tests([1_000])},
          heavy_mod => %{tests: make_tests([10_000])}
        })

      result = Weights.suggest_weights([result])

      # HeavyModule already has weight 10 which matches calculated, fast modules are weight 1
      assert result == []
    end

    test "modules across multiple suites" do
      results = [
        make_result("smoke", %{
          :"Elixir.Smoke.FastTest" => %{tests: make_tests([1_000])}
        }),
        make_result("resilience", %{
          :"Elixir.Resilience.SlowTest" => %{tests: make_tests([5_000])}
        })
      ]

      result = Weights.suggest_weights(results)

      # Median of [1000, 5000] = 3000
      # Fast: max(1, round(1000/3000)) = 1 -> matches current, omitted
      # Slow: max(1, round(5000/3000)) = 2 -> differs from current 1
      assert length(result) == 1
      slow = hd(result)
      assert slow.module == "Elixir.Resilience.SlowTest"
      assert slow.suggested_weight == 2
      assert slow.current_weight == 1
    end

    test "results are sorted by duration descending" do
      result =
        make_result("smoke", %{
          :"Elixir.Smoke.MediumTest" => %{tests: make_tests([5_000])},
          :"Elixir.Smoke.SlowTest" => %{tests: make_tests([20_000])}
        })

      result = Weights.suggest_weights([result])
      durations = Enum.map(result, & &1.duration_us)

      assert durations == Enum.sort(durations, :desc)
    end

    test "multiple tests in same module are summed" do
      result =
        make_result("smoke", %{
          :"Elixir.Smoke.BigTest" => %{tests: make_tests([3_000, 7_000])},
          :"Elixir.Smoke.SmallTest" => %{tests: make_tests([1_000])}
        })

      result = Weights.suggest_weights([result])
      big = Enum.find(result, &(&1.module == "Elixir.Smoke.BigTest"))

      assert big.duration_us == 10_000
    end

    test "defaults to weight 1 for modules without __toast_weight__" do
      # All fake module atoms - none define __toast_weight__, so all default to 1
      result =
        make_result("smoke", %{
          :"Elixir.Smoke.FastA" => %{tests: make_tests([1_000])},
          :"Elixir.Smoke.FastB" => %{tests: make_tests([1_000])},
          :"Elixir.Smoke.FastC" => %{tests: make_tests([1_000])},
          :"Elixir.Smoke.SlowTest" => %{tests: make_tests([10_000])}
        })

      result = Weights.suggest_weights([result])
      slow = Enum.find(result, &(&1.module == "Elixir.Smoke.SlowTest"))

      assert slow.current_weight == 1
      assert slow.suggested_weight == 10
    end
  end
end
