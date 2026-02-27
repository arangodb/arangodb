defmodule Toast.FormatterTestHelpers do
  def make_test(overrides \\ %{}) do
    defaults = %{
      name: :"test something",
      module: FakeTest,
      state: nil,
      time: 25_000,
      tags: %{file: "test/fake_test.exs", line: 5, test_type: :test}
    }

    fields = Map.merge(defaults, overrides)

    %ExUnit.Test{
      name: fields.name,
      module: fields.module,
      state: fields.state,
      time: fields.time,
      tags: fields.tags
    }
  end
end
