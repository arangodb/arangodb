defmodule ToastTest.Bucket do
  @moduledoc """
  Partitions test modules into buckets for parallel CI execution.

  Instead of simple round-robin, we use a weighted bucketing approach to keep
  related tests together while balancing load.
  Each suite's modules are assigned to the minimum number of buckets needed
  (based on its weight relative to the target bucket weight), keeping suite
  deployments concentrated while maintaining overall weight balance.
  """

  @type spec :: {pos_integer(), non_neg_integer()}

  @doc """
  Parses a bucket spec string like `"4/0"` into `{total, index}`.

  Index is 0-based. Returns `{:error, reason}` for invalid input.
  """
  @spec parse_spec(String.t()) :: {:ok, spec()} | {:error, String.t()}
  def parse_spec(spec) when is_binary(spec) do
    with [total_str, index_str] <- String.split(spec, "/", parts: 2),
         {total, ""} when total >= 1 <- Integer.parse(total_str),
         {index, ""} when index >= 0 <- Integer.parse(index_str),
         true <- index < total do
      {:ok, {total, index}}
    else
      _ -> {:error, "invalid bucket spec #{inspect(spec)}, expected TOTAL/INDEX (e.g. 4/0)"}
    end
  end

  @doc """
  Assigns each `{suite_name, module}` to a bucket (0-based index).

  Returns `[{bucket_index, suite_name, module}]`.

  Algorithm:
  1. Group by suite, sort suites by total weight descending
  2. Within each suite, sort modules by weight descending
  3. For each suite, pick the K lightest buckets where
     K = ceil(suite_weight / target_weight), and distribute
     the suite's modules across those K buckets via greedy assignment
  """
  @spec assign([{String.t(), module()}], pos_integer(), (module() -> number())) ::
          [{non_neg_integer(), String.t(), module()}]
  def assign(suite_data, total, weights_fn) do
    sorted_suites = build_sorted_suites(suite_data, weights_fn)
    total_weight = Enum.reduce(sorted_suites, 0, fn {_, _, w}, acc -> acc + w end)
    target = if total_weight > 0, do: total_weight / total, else: 1
    initial_buckets = for i <- 0..(total - 1), do: {i, 0}

    {assignments, _buckets} =
      Enum.flat_map_reduce(sorted_suites, initial_buckets, fn {suite, weighted_modules,
                                                               suite_weight},
                                                              buckets ->
        assign_suite(suite, weighted_modules, suite_weight, buckets, target, total)
      end)

    assignments
  end

  @doc """
  Returns modules assigned to `bucket_index` as `[{suite_name, module}]`.
  """
  @spec select([{String.t(), module()}], non_neg_integer(), pos_integer(), (module() -> number())) ::
          [{String.t(), module()}]
  def select(suite_data, bucket_index, total, weights_fn) do
    suite_data
    |> assign(total, weights_fn)
    |> Enum.filter(fn {b, _, _} -> b == bucket_index end)
    |> Enum.map(fn {_, suite, mod} -> {suite, mod} end)
  end

  # Returns [{suite_name, [{module, weight}], suite_weight}] sorted by suite weight desc
  defp build_sorted_suites(suite_data, weights_fn) do
    suite_data
    |> Enum.group_by(fn {suite, _mod} -> suite end, fn {_suite, mod} -> mod end)
    |> Enum.map(fn {suite, modules} ->
      weighted = Enum.map(modules, fn mod -> {mod, weights_fn.(mod)} end)
      sorted = Enum.sort_by(weighted, fn {_mod, w} -> w end, :desc)
      suite_weight = weighted |> Enum.map(fn {_, w} -> w end) |> Enum.sum()
      {suite, sorted, suite_weight}
    end)
    |> Enum.sort_by(fn {_, _, weight} -> weight end, :desc)
  end

  defp assign_suite(suite, weighted_modules, suite_weight, buckets, target, total) do
    k = bucket_count(suite_weight, target, total)

    eligible =
      buckets
      |> Enum.sort_by(fn {_idx, w} -> w end)
      |> Enum.take(k)
      |> MapSet.new(fn {idx, _} -> idx end)

    {assignments, updated_buckets} =
      Enum.map_reduce(weighted_modules, buckets, fn {mod, weight}, bkts ->
        {min_idx, _} =
          bkts
          |> Enum.filter(fn {idx, _} -> MapSet.member?(eligible, idx) end)
          |> Enum.min_by(fn {_idx, w} -> w end)

        updated =
          Enum.map(bkts, fn
            {^min_idx, w} -> {min_idx, w + weight}
            other -> other
          end)

        {{min_idx, suite, mod}, updated}
      end)

    {assignments, updated_buckets}
  end

  defp bucket_count(suite_weight, target, total) do
    ceil(suite_weight / target) |> min(total) |> max(1)
  end
end
