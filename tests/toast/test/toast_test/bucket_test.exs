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

defmodule ToastTest.BucketTest do
  use ExUnit.Case, async: true

  alias ToastTest.Bucket

  defp buckets_for_suite(result, suite_name) do
    result
    |> Enum.filter(fn {_, suite, _} -> suite == suite_name end)
    |> Enum.map(fn {b, _, _} -> b end)
    |> Enum.uniq()
  end

  describe "parse_spec/1" do
    test "parses valid spec with multiple buckets" do
      assert {:ok, {4, 1}} = Bucket.parse_spec("4/1")
    end

    test "parses single bucket" do
      assert {:ok, {1, 0}} = Bucket.parse_spec("1/0")
    end

    test "parses first index" do
      assert {:ok, {4, 0}} = Bucket.parse_spec("4/0")
    end

    test "parses high total and mid index" do
      assert {:ok, {10, 5}} = Bucket.parse_spec("10/5")
    end

    test "parses last bucket index" do
      assert {:ok, {4, 3}} = Bucket.parse_spec("4/3")
    end

    test "rejects total of zero" do
      assert {:error, _reason} = Bucket.parse_spec("0/0")
    end

    test "rejects index equal to total" do
      assert {:error, _reason} = Bucket.parse_spec("4/4")
    end

    test "rejects index greater than total" do
      assert {:error, _reason} = Bucket.parse_spec("4/5")
    end

    test "rejects negative index" do
      assert {:error, _reason} = Bucket.parse_spec("4/-1")
    end

    test "rejects non-numeric input" do
      assert {:error, _reason} = Bucket.parse_spec("abc")
    end

    test "rejects missing index" do
      assert {:error, _reason} = Bucket.parse_spec("4/")
    end

    test "rejects missing total" do
      assert {:error, _reason} = Bucket.parse_spec("/1")
    end

    test "rejects empty string" do
      assert {:error, _reason} = Bucket.parse_spec("")
    end
  end

  describe "assign/3" do
    test "single module in single bucket goes to bucket 0" do
      suite_data = [{"smoke", ModA}]
      weights_fn = fn _mod -> 1 end

      result = Bucket.assign(suite_data, 1, weights_fn)

      assert result == [{0, "smoke", ModA}]
    end

    test "everything in single bucket" do
      suite_data = [{"smoke", ModA}, {"smoke", ModB}, {"resilience", ModC}]
      weights_fn = fn _mod -> 1 end

      result = Bucket.assign(suite_data, 1, weights_fn)

      assert Enum.all?(result, fn {bucket, _, _} -> bucket == 0 end)
      assert length(result) == 3
    end

    test "three equal-weight modules across two buckets are balanced" do
      suite_data = [{"s", ModA}, {"s", ModB}, {"s", ModC}]
      weights_fn = fn _mod -> 1 end

      result = Bucket.assign(suite_data, 2, weights_fn)

      bucket_0 = Enum.filter(result, fn {b, _, _} -> b == 0 end)
      bucket_1 = Enum.filter(result, fn {b, _, _} -> b == 1 end)

      # 3 modules across 2 buckets: one gets 2, the other gets 1
      assert Enum.sort([length(bucket_0), length(bucket_1)]) == [1, 2]
    end

    test "weighted modules are balanced by weight across two buckets" do
      weights = %{ModA => 10, ModB => 5, ModC => 3, ModD => 2, ModE => 1}
      suite_data = Enum.map(weights, fn {mod, _w} -> {"s", mod} end)
      weights_fn = fn mod -> Map.fetch!(weights, mod) end

      result = Bucket.assign(suite_data, 2, weights_fn)

      bucket_weight = fn bucket_idx ->
        result
        |> Enum.filter(fn {b, _, _} -> b == bucket_idx end)
        |> Enum.map(fn {_, _, mod} -> Map.fetch!(weights, mod) end)
        |> Enum.sum()
      end

      w0 = bucket_weight.(0)
      w1 = bucket_weight.(1)

      # Total weight is 21, so optimal split is 11/10 or similar.
      assert w0 + w1 == 21
      assert abs(w0 - w1) <= 3
    end

    test "small suite stays in one bucket" do
      # Light suite (weight 2) is below target (26/3 ≈ 8.7), so K=1.
      # It must land in exactly one bucket.
      weights = %{ModA => 10, ModB => 8, ModC => 6, ModD => 1, ModE => 1}

      suite_data = [
        {"heavy", ModA},
        {"heavy", ModB},
        {"heavy", ModC},
        {"light", ModD},
        {"light", ModE}
      ]

      weights_fn = fn mod -> Map.fetch!(weights, mod) end

      result = Bucket.assign(suite_data, 3, weights_fn)

      assert length(buckets_for_suite(result, "light")) == 1
    end

    test "large suite splits across minimum necessary buckets" do
      # Suite alpha (weight 20) with target 25/3 ≈ 8.3, K = ceil(20/8.3) = 3.
      # Suite beta (weight 5) with K = ceil(5/8.3) = 1.
      weights = %{ModA => 10, ModB => 10, ModC => 3, ModD => 2}

      suite_data = [
        {"alpha", ModA},
        {"alpha", ModB},
        {"beta", ModC},
        {"beta", ModD}
      ]

      weights_fn = fn mod -> Map.fetch!(weights, mod) end

      result = Bucket.assign(suite_data, 3, weights_fn)

      assert length(buckets_for_suite(result, "beta")) == 1

      # Alpha has only 2 modules, so it can span at most 2 buckets
      assert length(buckets_for_suite(result, "alpha")) <= 2
    end

    test "small suite stays in one bucket even when greedy per-module would scatter" do
      # Per-module greedy would scatter suite B:
      # After A's three 10s fill buckets to [10, 10, 10], B's 3s would go
      # to B0 and B1 (tie-breaking by lowest index). But B's total weight (6)
      # is below target (36/3=12), so it should stay in 1 bucket.
      weights = %{ModA1 => 10, ModA2 => 10, ModA3 => 10, ModB1 => 3, ModB2 => 3}

      suite_data = [
        {"alpha", ModA1},
        {"alpha", ModA2},
        {"alpha", ModA3},
        {"beta", ModB1},
        {"beta", ModB2}
      ]

      weights_fn = fn mod -> Map.fetch!(weights, mod) end

      result = Bucket.assign(suite_data, 3, weights_fn)

      assert length(buckets_for_suite(result, "beta")) == 1
    end

    test "more buckets than modules leaves some buckets empty" do
      suite_data = [{"s", ModA}, {"s", ModB}]
      weights_fn = fn _mod -> 1 end

      result = Bucket.assign(suite_data, 5, weights_fn)

      assigned_buckets = result |> Enum.map(fn {b, _, _} -> b end) |> Enum.uniq() |> Enum.sort()

      assert length(result) == 2
      # Only 2 of 5 buckets should have modules
      assert length(assigned_buckets) == 2
    end

    test "ties in bucket weight are broken by lowest index" do
      # Two buckets, both start at 0 weight. First module should go to bucket 0.
      suite_data = [{"s", ModA}]
      weights_fn = fn _mod -> 5 end

      result = Bucket.assign(suite_data, 2, weights_fn)

      assert result == [{0, "s", ModA}]
    end
  end

  describe "select/4" do
    test "returns only modules for the given bucket" do
      weights = %{ModA => 10, ModB => 1, ModC => 1}

      suite_data = [{"s", ModA}, {"s", ModB}, {"s", ModC}]
      weights_fn = fn mod -> Map.fetch!(weights, mod) end

      # With 2 buckets, greedy assigns:
      #   ModA(10)->B0, ModB(1)->B1, ModC(1)->B1
      selected = Bucket.select(suite_data, 0, 2, weights_fn)

      assert selected == [{"s", ModA}]
    end

    test "returns empty list for bucket with no modules" do
      suite_data = [{"s", ModA}]
      weights_fn = fn _mod -> 1 end

      # 3 buckets, 1 module — buckets 1 and 2 are empty
      assert Bucket.select(suite_data, 2, 3, weights_fn) == []
    end

    test "returns all modules when single bucket is selected" do
      suite_data = [{"s", ModA}, {"s", ModB}, {"t", ModC}]
      weights_fn = fn _mod -> 1 end

      selected = Bucket.select(suite_data, 0, 1, weights_fn)

      assert length(selected) == 3
      assert {"s", ModA} in selected
      assert {"s", ModB} in selected
      assert {"t", ModC} in selected
    end
  end
end
