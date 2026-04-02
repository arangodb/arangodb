defmodule ToastTest.Runner.TestFilter do
  @moduledoc """
  Pure filtering of ExUnit tests by tag include/exclude, test IDs, and name patterns.
  """

  @doc """
  Splits `tests` into `{to_run, to_skip}` based on the given filters.

  Filters map keys:
  - `:include` — ExUnit tag filters to include
  - `:exclude` — ExUnit tag filters to exclude
  - `:only_test_ids` — nil (all) or MapSet of `{module, test_name}` tuples
  - `:test_name_pattern` — nil (all) or case-insensitive substring to match
  """
  @spec filter(map(), [%ExUnit.Test{}]) :: {[%ExUnit.Test{}], [%ExUnit.Test{}]}
  def filter(filters, tests) do
    %{
      include: include,
      exclude: exclude,
      only_test_ids: test_ids,
      test_name_pattern: name_pattern
    } = filters

    name_pattern = name_pattern && String.downcase(name_pattern)

    {to_run, to_skip} =
      for test <- tests,
          include_test?(test_ids, test),
          match_test_name?(name_pattern, test),
          reduce: {[], []} do
        {to_run, to_skip} ->
          tags = Map.merge(test.tags, %{test: test.name, module: test.module})

          case ExUnit.Filters.eval(include, exclude, tags, tests) do
            :ok -> {[%{test | tags: tags} | to_run], to_skip}
            excluded_or_skipped -> {to_run, [%{test | state: excluded_or_skipped} | to_skip]}
          end
      end

    {Enum.reverse(to_run), Enum.reverse(to_skip)}
  end

  @spec include_test?(MapSet.t() | nil, %ExUnit.Test{}) :: boolean()
  def include_test?(nil, _test), do: true

  def include_test?(test_ids, test) do
    MapSet.member?(test_ids, {test.module, test.name})
  end

  @spec match_test_name?(String.t() | nil, %ExUnit.Test{}) :: boolean()
  def match_test_name?(nil, _test), do: true

  def match_test_name?(pattern, test) do
    test.name
    |> Atom.to_string()
    |> String.downcase()
    |> String.contains?(String.downcase(pattern))
  end
end
