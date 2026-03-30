defmodule ToastTest.CLIFormatter do
  @moduledoc """
  ExUnit formatter that produces Google Test-style output with timestamps.

  Replaces ExUnit.CLIFormatter when tests are run through ToastTest.Case.
  Produces output like:

      Running test/smoke_test/version_test.exs
      ─────────────────────────────────────────
      2026-02-15T14:30:45.123Z [ RUN        ] server version
      2026-02-15T14:30:45.200Z [     PASSED ] server version (77ms)
      2026-02-15T14:30:45.300Z [------------] 1 test from SmokeTest.VersionTest (200ms total)

  Modules whose tests are entirely skipped due to a suite abort are shown as
  a single summary line instead of per-test output:

      2026-02-15T14:30:45.300Z [    SKIPPED ] 5 tests from SmokeTest.OtherTest
  """

  use GenServer

  import ToastTest.Formatting

  defmodule State do
    @moduledoc false
    @enforce_keys [:config, :colors_enabled]
    defstruct [
      :config,
      :colors_enabled,
      # Module tracking — header is deferred until the first real test starts
      pending_module: nil,
      module_header_printed: false,
      module_start_time: nil,
      module_test_count: 0,
      module_skipped_count: 0,
      # Suite-level stats
      failure_counter: 0,
      counters: %{passed: 0, failed: 0, skipped: 0, excluded: 0, invalid: 0, total: 0},
      suite_start_time: nil
    ]
  end

  @impl true
  def init(opts) do
    state = %State{
      config: opts,
      colors_enabled: colors_enabled?(opts),
      suite_start_time: System.monotonic_time(:millisecond)
    }

    {:ok, state}
  end

  # --- Event handling ---

  @impl true
  def handle_cast({:suite_started, _opts}, state) do
    {:noreply, %{state | suite_start_time: System.monotonic_time(:millisecond)}}
  end

  def handle_cast({:module_started, %ExUnit.TestModule{} = mod}, state) do
    {:noreply,
     %{
       state
       | pending_module: mod,
         module_header_printed: false,
         module_start_time: System.monotonic_time(:millisecond),
         module_test_count: 0,
         module_skipped_count: 0
     }}
  end

  # Don't print RUN for excluded/skipped tests (filter-excluded or abort-skipped)
  def handle_cast({:test_started, %ExUnit.Test{state: {:excluded, _}}}, state) do
    {:noreply, state}
  end

  def handle_cast({:test_started, %ExUnit.Test{state: {:skipped, _}}}, state) do
    {:noreply, state}
  end

  def handle_cast({:test_started, %ExUnit.Test{state: {:invalid, _}}}, state) do
    {:noreply, state}
  end

  def handle_cast({:test_started, %ExUnit.Test{} = test}, state) do
    state = ensure_module_header(state)
    name = display_name(test)
    IO.puts("#{timestamp()} #{colorize("[ RUN        ]", :yellow, state)} #{name}")
    {:noreply, state}
  end

  def handle_cast({:test_finished, %ExUnit.Test{} = test}, state) do
    state = update_counters(state, test)
    state = track_abort_skipped(state, test)
    print_test_result(test, state)
    state = count_failure(state, test)
    {:noreply, state}
  end

  def handle_cast({:module_finished, %ExUnit.TestModule{} = mod}, state) do
    state = maybe_print_module_failure(mod, state)

    if state.module_header_printed do
      print_module_summary(state)
    else
      print_skipped_module_summary(state)
    end

    {:noreply, state}
  end

  def handle_cast({:suite_finished, _times_us}, state) do
    print_session_summary(state)
    {:noreply, state}
  end

  def handle_cast({:sigquit, running_tests}, state) do
    IO.puts("\n#{colorize("Showing running tests:", :yellow, state)}")

    for %ExUnit.Test{} = test <- running_tests do
      IO.puts("  #{inspect(test.module)} - #{display_name(test)}")
    end

    {:noreply, state}
  end

  def handle_cast({:max_failures_reached, _}, state) do
    IO.puts("\n#{colorize("--max-failures reached, aborting test suite", :red, state)}")
    {:noreply, state}
  end

  def handle_cast(_msg, state) do
    {:noreply, state}
  end

  defp maybe_print_module_failure(
         %ExUnit.TestModule{state: {:failed, failures}} = mod,
         state
       ) do
    state = ensure_module_header(state)

    IO.puts(
      "#{timestamp()} #{colorize("[     FAILED ]", :red, state)} " <>
        "setup_all for #{colorize(inspect(mod.name), :bold, state)}"
    )

    Enum.each(failures, fn {kind, error, stack} ->
      IO.puts("\n#{Exception.format(kind, error, stack)}")
    end)

    IO.puts("")
    state
  end

  defp maybe_print_module_failure(_mod, state), do: state

  defp print_module_summary(state) do
    elapsed = elapsed_ms(state.module_start_time)
    mod_name = inspect(state.pending_module.name)
    count = state.module_test_count
    test_word = if count == 1, do: "test", else: "tests"

    skip_part =
      if state.module_skipped_count > 0,
        do: ", #{state.module_skipped_count} skipped",
        else: ""

    IO.puts(
      "#{timestamp()} #{colorize("[------------]", :cyan, state)} " <>
        "#{count} #{test_word} from #{colorize(mod_name, :bold, state)} (#{elapsed}ms total#{skip_part})"
    )
  end

  defp print_skipped_module_summary(state) do
    # Module header was never printed — all tests were excluded or skipped
    if state.module_skipped_count > 0 do
      mod_name = inspect(state.pending_module.name)
      count = state.module_skipped_count
      test_word = if count == 1, do: "test", else: "tests"

      IO.puts(
        "#{timestamp()} #{colorize("[    SKIPPED ]", :yellow, state)} " <>
          "#{count} #{test_word} from #{colorize(mod_name, :bold, state)}"
      )
    end
  end

  # --- Module header (deferred) ---

  defp ensure_module_header(%{module_header_printed: true} = state), do: state

  defp ensure_module_header(%{pending_module: mod} = state) do
    file = module_file(mod)
    print_module_header(file, state.colors_enabled)
    %{state | module_header_printed: true}
  end

  defp print_module_header(file, colors_enabled) do
    colored =
      "#{colorize("Running", :cyan, colors_enabled)} #{colorize(file, :bold, colors_enabled)}"

    underline = colorize(String.duplicate("─", 80), :cyan, colors_enabled)

    IO.puts("\n#{colored}\n#{underline}")
  end

  # --- Test result output ---

  defp print_test_result(%ExUnit.Test{state: nil} = test, state) do
    elapsed = div(test.time, 1000)
    name = display_name(test)
    IO.puts("#{timestamp()} #{colorize("[     PASSED ]", :green, state)} #{name} (#{elapsed}ms)")
  end

  defp print_test_result(%ExUnit.Test{state: {:failed, failures}} = test, state) do
    elapsed = div(test.time, 1000)
    name = display_name(test)
    IO.puts("#{timestamp()} #{colorize("[     FAILED ]", :red, state)} #{name} (#{elapsed}ms)")
    print_failure_details(test, failures, state)
  end

  defp print_test_result(%ExUnit.Test{state: {:skipped, _}} = test, state) do
    # Abort-skipped tests in a fully-skipped module: suppress (module_finished handles it).
    if state.module_header_printed or not abort_skipped?(test) do
      name = display_name(test)
      IO.puts("#{timestamp()} #{colorize("[    SKIPPED ]", :yellow, state)} #{name}")
    end
  end

  defp print_test_result(%ExUnit.Test{state: {:excluded, _}}, _state) do
    # Filter-excluded tests are silently counted
    :ok
  end

  defp print_test_result(%ExUnit.Test{state: {:invalid, _}} = test, state) do
    name = display_name(test)
    IO.puts("#{timestamp()} #{colorize("[    INVALID ]", :red, state)} #{name}")
  end

  defp print_failure_details(test, failures, state) do
    formatted =
      ExUnit.Formatter.format_test_failure(
        test,
        failures,
        state.failure_counter + 1,
        :infinity,
        &formatter_cb/2
      )

    IO.puts("\n#{formatted}")
  end

  # --- Session summary ---

  defp print_session_summary(state) do
    c = state.counters
    elapsed = elapsed_ms(state.suite_start_time)
    abort_reason = ToastTest.Abort.reason()
    failure? = c.failed > 0 || c.invalid > 0 || abort_reason != nil

    status_text = if failure?, do: "FAILED", else: "PASSED"
    status_color = if failure?, do: :red, else: :green

    # Status line
    status_bracket = String.pad_leading(status_text, 10)

    IO.puts(
      "\n#{timestamp()} #{colorize("[#{status_bracket} ]", status_color, state)} #{c.total} tests."
    )

    # Abort reason
    if abort_reason do
      abort_msg = ToastTest.Abort.display_reason(abort_reason)
      IO.puts("#{timestamp()} #{colorize("[   ABORTED ]", :red, state)} #{abort_msg}")
    end

    # Detail line
    detail = c |> build_detail_parts(state) |> Enum.join(", ")

    IO.puts(
      "#{timestamp()} #{colorize("[============]", :cyan, state)} " <>
        "Ran: #{c.total} tests (#{detail}) (#{elapsed}ms total)"
    )
  end

  defp build_detail_parts(c, state) do
    # "failed" is always shown (colored when > 0, plain otherwise)
    failed_part =
      if c.failed > 0,
        do: colorize("#{c.failed} failed", :red, state),
        else: "#{c.failed} failed"

    [
      if(c.passed > 0, do: colorize("#{c.passed} passed", :green, state)),
      failed_part,
      if(c.skipped > 0, do: colorize("#{c.skipped} skipped", :yellow, state)),
      if(c.excluded > 0, do: "#{c.excluded} excluded"),
      if(c.invalid > 0, do: colorize("#{c.invalid} invalid", :red, state))
    ]
    |> Toast.Utils.compact()
  end

  @doc """
  Print a failure summary for the given list of `%ExUnit.Test{}` structs.

  All tests must have `state: {:failed, _}` — the function pattern-matches on
  this and will raise on tests with any other state.
  """
  @spec print_failure_summary([ExUnit.Test.t()]) :: :ok
  def print_failure_summary([]), do: :ok

  def print_failure_summary(failures) do
    colors_enabled = IO.ANSI.enabled?()

    colorize(
      [
        "\n",
        String.duplicate("=", 80),
        "\n TEST FAILURES\n",
        String.duplicate("=", 80)
      ],
      :red,
      colors_enabled
    )
    |> IO.puts()

    failures
    |> Enum.with_index(1)
    |> Enum.each(fn {%ExUnit.Test{state: {:failed, failure_details}} = test, idx} ->
      formatted =
        ExUnit.Formatter.format_test_failure(
          test,
          failure_details,
          idx,
          :infinity,
          &formatter_cb/2
        )

      IO.puts("\n#{formatted}")
    end)
  end

  # --- Counter management ---

  defp update_counters(state, %ExUnit.Test{} = test) do
    key =
      case test.state do
        nil -> :passed
        {:failed, _} -> :failed
        {:skipped, _} -> :skipped
        {:excluded, _} -> :excluded
        {:invalid, _} -> :invalid
      end

    counters =
      state.counters
      |> Map.update!(key, &(&1 + 1))
      |> Map.update!(:total, &(&1 + 1))

    %{state | counters: counters, module_test_count: state.module_test_count + 1}
  end

  defp track_abort_skipped(state, %ExUnit.Test{} = test) do
    if abort_skipped?(test) do
      %{state | module_skipped_count: state.module_skipped_count + 1}
    else
      state
    end
  end

  defp abort_skipped?(%ExUnit.Test{state: {:skipped, msg}}) when is_binary(msg) do
    String.starts_with?(msg, ToastTest.Abort.prefix())
  end

  defp abort_skipped?(_test), do: false

  defp count_failure(state, %ExUnit.Test{state: {:failed, _}}) do
    %{state | failure_counter: state.failure_counter + 1}
  end

  defp count_failure(state, _test), do: state

  # --- Helpers ---

  defp display_name(%ExUnit.Test{name: name}), do: display_test_name(name)

  defp module_file(%ExUnit.TestModule{state: {:failed, %{tags: %{file: file}}}}), do: file

  defp module_file(%ExUnit.TestModule{name: name}) do
    if function_exported?(name, :__ex_unit__, 0),
      do: Map.get(name.__ex_unit__(), :file, inspect(name)),
      else: inspect(name)
  end

  defp timestamp do
    DateTime.utc_now()
    |> DateTime.truncate(:millisecond)
    |> DateTime.to_iso8601(:extended)
  end

  defp elapsed_ms(nil), do: 0

  defp elapsed_ms(start_time) do
    System.monotonic_time(:millisecond) - start_time
  end

  defp colors_enabled?(opts) do
    Keyword.get_lazy(opts, :colors_enabled, fn ->
      colors = Keyword.get(opts, :colors, [])
      Keyword.get(colors, :enabled, IO.ANSI.enabled?())
    end)
  end
end
