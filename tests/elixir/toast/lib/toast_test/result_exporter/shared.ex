defmodule ToastTest.ResultExporter.Shared do
  @moduledoc false

  alias Toast.Diagnostics.Matcher

  @doc """
  Group matched entries by test, sorted by module+name.

  Returns a list of `{header, details}` tuples where `header` includes the
  test identity and confidence label, and `details` is a list of formatted
  strings produced by `detail_fn` for each matched item.
  """
  @spec format_grouped_matches([map()], atom(), (map() -> String.t())) ::
          [{String.t(), [String.t()]}]
  def format_grouped_matches(matched, item_key, detail_fn) do
    matched
    |> Enum.group_by(fn e -> {e.module, e.test} end)
    |> Enum.sort_by(fn {{mod, name}, _} -> {inspect(mod), name} end)
    |> Enum.map(&format_match_group(&1, item_key, detail_fn))
  end

  defp format_match_group({{module, test_name}, entries}, item_key, detail_fn) do
    confidence_label =
      entries |> Enum.map(& &1.confidence) |> Matcher.confidence_label()

    header = "#{inspect(module)} - #{test_name} (#{confidence_label})"
    details = Enum.map(entries, fn e -> detail_fn.(Map.fetch!(e, item_key)) end)
    {header, details}
  end
end
