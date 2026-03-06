defmodule ToastTest.TestResult do
  @enforce_keys [:module, :name, :outcome]

  @type t :: %__MODULE__{
          module: module(),
          name: String.t(),
          outcome: :passed | :failed | :skipped | :excluded | :invalid,
          duration_us: non_neg_integer(),
          failure: term(),
          started_at: DateTime.t() | nil,
          finished_at: DateTime.t() | nil,
          tags: %{file: String.t() | nil, line: non_neg_integer() | nil}
        }

  defstruct [
    :module,
    :name,
    :outcome,
    :failure,
    :started_at,
    :finished_at,
    duration_us: 0,
    tags: %{file: nil, line: nil}
  ]
end
