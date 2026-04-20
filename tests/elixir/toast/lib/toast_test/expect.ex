defmodule ToastTest.Expect do
  @moduledoc """
  Non-fatal expectations for Toast integration tests.

  Unlike `assert`, which stops test execution on failure, `expect` records the
  failure and continues — similar to Google Test's `EXPECT_*` macros. This
  allows a single test to report multiple independent failures.

  Failures are collected in the process dictionary and checked after the test
  function returns. If any expectations failed, the test is marked as failed
  with an `ExUnit.MultiError` containing all recorded failures.

  ## Usage

      test "multiple independent checks", %{client: client} do
        expect {:ok, _} = Client.Admin.version(client)
        expect {:ok, _} = Client.Admin.engine(client)
        expect {:ok, _} = Client.Admin.statistics(client)
      end

  ## Limitations

  - Variable bindings from pattern matches inside `expect` are unreliable —
    they are undefined when the expectation fails. Use `assert` when you need
    matched values for subsequent code.

  - Expectations must be called from the test process. Failures recorded in
    spawned tasks or child processes are silently lost because each process
    has its own dictionary. This also applies to `assert` in unlinked child
    processes, but `expect` has no crash-based fallback to surface the issue.
  """

  @pdict_key :toast_expect_failures

  @doc """
  Evaluates an assertion without stopping test execution on failure.

  Supports the same expression forms as `assert`: truthiness checks,
  pattern matches (`expect {:ok, _} = expr`), and comparisons
  (`expect a == b`).
  """
  defmacro expect(assertion) do
    quote do
      try do
        ExUnit.Assertions.assert(unquote(assertion))
      rescue
        error in [ExUnit.AssertionError] ->
          ToastTest.Expect.record_failure(error, __STACKTRACE__)
          nil
      end
    end
  end

  @doc false
  @spec record_failure(Exception.t(), Exception.stacktrace()) :: :ok
  def record_failure(error, stacktrace) do
    failures = Process.get(@pdict_key, [])
    Process.put(@pdict_key, [{:error, error, stacktrace} | failures])
    :ok
  end

  @doc false
  @spec collect_failures() :: [{:error, Exception.t(), Exception.stacktrace()}]
  def collect_failures do
    failures = Process.get(@pdict_key, [])
    Process.delete(@pdict_key)
    Enum.reverse(failures)
  end
end
