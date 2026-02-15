ExUnit.start()

case Toast.TestCase.setup_suite(:single_server) do
  {:ok, _} -> :ok
  {:error, _} -> ExUnit.configure(exclude: [:toast_suite])
end
