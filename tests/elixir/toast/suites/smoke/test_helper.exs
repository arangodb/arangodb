ExUnit.start()

case Toast.TestCase.setup_suite() do
  {:ok, _} -> :ok
  {:error, _} -> ExUnit.configure(exclude: [:toast_suite])
end
