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

  @impl true
  def init(opts) do
    colors_enabled = colors_enabled?(opts)

    state = %{
      config: opts,
      colors_enabled: colors_enabled,
      # Module tracking — header is deferred until the first real test starts
      pending_module: nil,
      module_header_printed: false,
      module_start_time: nil,
      module_test_count: 0,
      module_skipped_count: 0,
      module_skipped_reason: nil,
      # Suite-level stats
      failures: [],
      failure_counter: 0,
      counters: %{passed: 0, failed: 0, skipped: 0, excluded: 0, invalid: 0, total: 0},
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
         module_skipped_count: 0,
         module_skipped_reason: nil
     }}
  end

  # Don't print RUN for excluded/skipped tests (filter-excluded or abort-skipped)
  def handle_cast({:test_started, %ExUnit.Test{state: {:excluded, _}}}, state) do
    {:noreply, state}
  end

  def handle_cast({:test_started, %ExUnit.Test{state: {:skipped, _}}}, state) do
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
    state = maybe_record_failure(state, test)
    {:noreply, state}
  end

  def handle_cast({:module_finished, %ExUnit.TestModule{} = _mod}, state) do
    if state.module_header_printed do
      print_module_summary(state)
    else
      print_skipped_module_summary(state)
    end

    {:noreply, state}
  end

  def handle_cast({:suite_finished, _times_us}, state) do
    print_session_summary(state)

    # When aborted due to crash, skip the failure summary here —
    # the after_suite CRASH ATTRIBUTION section handles it with better context.
    if ToastTest.Runner.aborted?() do
      :ok
    else
      print_failure_summary(state)
    end

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
    if abort_skipped?(test) do
      # Abort-skipped tests in a running module: show individually since [ RUN ] was printed.
      # Abort-skipped tests in a fully-skipped module: suppress (module_finished handles it).
      if state.module_header_printed do
        name = display_name(test)
        IO.puts("#{timestamp()} #{colorize("[    SKIPPED ]", :yellow, state)} #{name}")
      end
    else
      # Regular @tag :skip
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

  # ExUnit.Formatter callback for diff coloring.
  # Diff callbacks may receive Inspect.Algebra documents (ExUnit 1.19+),
  # not just iodata, so they use colorize_diff/2.
  defp formatter_cb(:diff_enabled?, _default), do: true

  defp formatter_cb(:error_info, msg), do: colorize(msg, :red, true)
  defp formatter_cb(:extra_info, msg), do: colorize(msg, :cyan, true)
  defp formatter_cb(:location_info, msg), do: colorize(msg, [:bright, :default_color], true)
  defp formatter_cb(:diff_delete, msg), do: colorize_diff(msg, :red)

  defp formatter_cb(:diff_delete_whitespace, msg),
    do: colorize_diff(msg, IO.ANSI.color_background(1, 0, 0))

  defp formatter_cb(:diff_insert, msg), do: colorize_diff(msg, :green)

  defp formatter_cb(:diff_insert_whitespace, msg),
    do: colorize_diff(msg, IO.ANSI.color_background(0, 1, 0))

  defp formatter_cb(:blame_diff, msg), do: colorize_diff(msg, [:red, :bright])
  defp formatter_cb(_, msg), do: msg

  # --- Session summary ---

  defp print_session_summary(state) do
    c = state.counters
    elapsed = elapsed_ms(state.suite_start_time)
    abort_reason = ToastTest.Runner.aborted?()
    is_failure = c.failed > 0 || c.invalid > 0 || abort_reason != nil

    status_text = if is_failure, do: "FAILED", else: "PASSED"
    status_color = if is_failure, do: :red, else: :green

    # Status line
    status_bracket = String.pad_leading(status_text, 10)

    IO.puts(
      "\n#{timestamp()} #{colorize("[#{status_bracket} ]", status_color, state)} #{c.total} tests."
    )

    # Abort reason
    if abort_reason do
      IO.puts("#{timestamp()} #{colorize("[   ABORTED ]", :red, state)} #{abort_reason}")
    end

    # Detail line
    detail = build_detail_parts(c, state) |> Enum.join(", ")

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

  defp print_failure_summary(%{failures: []}), do: :ok

  defp print_failure_summary(state) do
    colorize(
      [
        "\n",
        String.duplicate("=", 80),
        "\n TEST FAILURES\n",
        String.duplicate("=", 80)
      ],
      :red,
      state
    )
    |> IO.puts()

    state.failures
    |> Enum.reverse()
    |> Enum.with_index(1)
    |> Enum.each(fn {%ExUnit.Test{state: {:failed, failures}} = test, idx} ->
      print_failure_details(test, failures, %{state | failure_counter: idx - 1})
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
    String.starts_with?(msg, ToastTest.Runner.abort_prefix())
  end

  defp abort_skipped?(_test), do: false

  defp maybe_record_failure(state, %ExUnit.Test{state: {:failed, _}} = test) do
    %{state | failures: [test | state.failures], failure_counter: state.failure_counter + 1}
  end

  defp maybe_record_failure(state, _test), do: state

  # --- Helpers ---

  defp display_name(%ExUnit.Test{name: name}) do
    name
    |> to_string()
    |> String.replace_prefix("test ", "")
  end

  defp module_file(%ExUnit.TestModule{} = mod) do
    case mod.state do
      {:failed, %{tags: %{file: file}}} ->
        file

      _ ->
        if function_exported?(mod.name, :__ex_unit__, 0) do
          info = mod.name.__ex_unit__()
          Map.get(info, :file, inspect(mod.name))
        else
          inspect(mod.name)
        end
    end
  end

  defp timestamp do
    DateTime.utc_now()
    |> DateTime.to_iso8601(:extended)
    |> String.replace(~r/\.\d{1,6}Z$/, fn match ->
      # Truncate to milliseconds
      String.slice(match, 0, 4) <> "Z"
    end)
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

  # Colorize diff content — handles both iodata and Inspect.Algebra documents.
  defp colorize_diff(msg, color) when is_binary(msg) or is_list(msg) do
    colorize(msg, color, true)
  end

  defp colorize_diff(msg, color) do
    Inspect.Algebra.concat([ansi_code(color), msg, IO.ANSI.reset()])
  end

  defp ansi_code(color) when is_list(color),
    do: IO.iodata_to_binary(Enum.map(color, &apply(IO.ANSI, &1, [])))

  defp ansi_code(:bold), do: IO.ANSI.bright()
  defp ansi_code(color) when is_atom(color), do: apply(IO.ANSI, color, [])
  defp ansi_code(color) when is_binary(color), do: color

  # Accepts state map or boolean for colors_enabled
  defp colorize(text, color, %{colors_enabled: enabled}), do: colorize(text, color, enabled)
  defp colorize(text, _color, false), do: text

  defp colorize(text, color, true) when is_list(color) do
    ansi = Enum.map(color, &apply(IO.ANSI, &1, []))
    IO.iodata_to_binary([ansi, text, IO.ANSI.reset()])
  end

  defp colorize(text, :bold, true) do
    IO.iodata_to_binary([IO.ANSI.bright(), text, IO.ANSI.reset()])
  end

  defp colorize(text, color, true) when is_atom(color) do
    IO.iodata_to_binary([apply(IO.ANSI, color, []), text, IO.ANSI.reset()])
  end

  defp colorize(text, color, true) when is_binary(color) do
    IO.iodata_to_binary([color, text, IO.ANSI.reset()])
  end
end
