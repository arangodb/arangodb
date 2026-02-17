defmodule Toast.CLIFormatter do
  @moduledoc """
  ExUnit formatter that produces Google Test-style output with timestamps.

  Replaces ExUnit.CLIFormatter when tests are run through Toast.TestCase.
  Produces output like:

      🚀 Running test/smoke_test/version_test.exs
      ─────────────────────────────────────────
      2026-02-15T14:30:45.123Z [ RUN        ] server version
      2026-02-15T14:30:45.200Z [     PASSED ] server version (77ms)
      2026-02-15T14:30:45.300Z [------------] 1 test from SmokeTest.VersionTest (200ms total)
  """

  use GenServer

  @impl true
  def init(opts) do
    colors_enabled = colors_enabled?(opts)

    state = %{
      config: opts,
      colors_enabled: colors_enabled,
      module_start_time: nil,
      module_test_count: 0,
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
    file = module_file(mod)
    print_module_header(file, state.colors_enabled)

    {:noreply, %{state | module_start_time: System.monotonic_time(:millisecond), module_test_count: 0}}
  end

  def handle_cast({:test_started, %ExUnit.Test{state: {:excluded, _}} = _test}, state) do
    # Don't print RUN for excluded tests (e.g., after suite abort)
    {:noreply, state}
  end

  def handle_cast({:test_started, %ExUnit.Test{} = test}, state) do
    name = display_name(test)
    write("#{timestamp()} #{colorize("[ RUN        ]", :yellow, state)} #{name}\n")
    {:noreply, state}
  end

  def handle_cast({:test_finished, %ExUnit.Test{} = test}, state) do
    state = update_counters(state, test)
    print_test_result(test, state)
    state = maybe_record_failure(state, test)
    {:noreply, state}
  end

  def handle_cast({:module_finished, %ExUnit.TestModule{} = mod}, state) do
    elapsed = elapsed_ms(state.module_start_time)
    mod_name = inspect(mod.name)
    count = state.module_test_count
    test_word = if count == 1, do: "test", else: "tests"

    write(
      "#{timestamp()} #{colorize("[------------]", :cyan, state)} " <>
        "#{count} #{test_word} from #{colorize(mod_name, :bold, state)} (#{elapsed}ms total)\n"
    )

    {:noreply, state}
  end

  def handle_cast({:suite_finished, _times_us}, state) do
    print_session_summary(state)
    print_failure_summary(state)
    {:noreply, state}
  end

  def handle_cast({:sigquit, running_tests}, state) do
    write("\n#{colorize("Showing running tests:", :yellow, state)}\n")

    for %ExUnit.Test{} = test <- running_tests do
      write("  #{inspect(test.module)} - #{display_name(test)}\n")
    end

    {:noreply, state}
  end

  def handle_cast({:max_failures_reached, _}, state) do
    write("\n#{colorize("--max-failures reached, aborting test suite", :red, state)}\n")
    {:noreply, state}
  end

  def handle_cast(_msg, state) do
    {:noreply, state}
  end

  # --- Module header ---

  defp print_module_header(file, colors_enabled) do
    colored = "🚀 #{colorize("Running", :cyan, colors_enabled)} #{colorize(file, :bold, colors_enabled)}"
    underline = colorize(String.duplicate("─", 80), :cyan, colors_enabled)

    write("\n#{colored}\n#{underline}\n")
  end

  # --- Test result output ---

  defp print_test_result(%ExUnit.Test{state: nil} = test, state) do
    elapsed = div(test.time, 1000)
    name = display_name(test)
    write("#{timestamp()} #{colorize("[     PASSED ]", :green, state)} #{name} (#{elapsed}ms)\n")
  end

  defp print_test_result(%ExUnit.Test{state: {:failed, failures}} = test, state) do
    elapsed = div(test.time, 1000)
    name = display_name(test)
    write("#{timestamp()} #{colorize("[     FAILED ]", :red, state)} #{name} (#{elapsed}ms)\n")
    print_failure_details(test, failures, state)
  end

  defp print_test_result(%ExUnit.Test{state: {:skipped, _}} = test, state) do
    name = display_name(test)
    write("#{timestamp()} #{colorize("[    SKIPPED ]", :yellow, state)} #{name}\n")
  end

  defp print_test_result(%ExUnit.Test{state: {:excluded, reason}} = test, state) do
    # Only print excluded tests that have a meaningful reason (e.g., suite abort).
    # Filter-excluded tests (tag-based) are silently counted.
    if is_binary(reason) and String.starts_with?(reason, "Suite aborted:") do
      name = display_name(test)
      write("#{timestamp()} #{colorize("[   EXCLUDED ]", :yellow, state)} #{name} (#{reason})\n")
    end
  end

  defp print_test_result(%ExUnit.Test{state: {:invalid, _}} = test, state) do
    name = display_name(test)
    write("#{timestamp()} #{colorize("[    INVALID ]", :red, state)} #{name}\n")
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

    write("\n#{formatted}\n")
  end

  # ExUnit.Formatter callback for diff coloring
  defp formatter_cb(:diff_enabled?, _default), do: true

  defp formatter_cb(:error_info, msg), do: colorize(msg, :red, true)
  defp formatter_cb(:extra_info, msg), do: colorize(msg, :cyan, true)
  defp formatter_cb(:location_info, msg), do: colorize(msg, [:bright, :default_color], true)
  defp formatter_cb(:diff_delete, msg), do: colorize(msg, :red, true)
  defp formatter_cb(:diff_delete_whitespace, msg), do: colorize(msg, IO.ANSI.color_background(1, 0, 0), true)
  defp formatter_cb(:diff_insert, msg), do: colorize(msg, :green, true)
  defp formatter_cb(:diff_insert_whitespace, msg), do: colorize(msg, IO.ANSI.color_background(0, 1, 0), true)
  defp formatter_cb(:blame_diff, msg), do: colorize(msg, [:red, :bright], true)
  defp formatter_cb(_, msg), do: msg

  # --- Session summary ---

  defp print_session_summary(state) do
    c = state.counters
    elapsed = elapsed_ms(state.suite_start_time)
    is_failure = c.failed > 0 || c.invalid > 0

    status_text = if is_failure, do: "FAILED", else: "PASSED"
    status_color = if is_failure, do: :red, else: :green

    # Status line
    status_bracket = String.pad_leading(status_text, 10)
    write(
      "\n#{timestamp()} #{colorize("[#{status_bracket} ]", status_color, state)} #{c.total} tests.\n"
    )

    # Detail line
    parts = []
    parts = if c.passed > 0, do: parts ++ [colorize("#{c.passed} passed", :green, state)], else: parts
    parts = if c.failed > 0, do: parts ++ [colorize("#{c.failed} failed", :red, state)], else: parts ++ ["#{c.failed} failed"]
    parts = if c.skipped > 0, do: parts ++ [colorize("#{c.skipped} skipped", :yellow, state)], else: parts
    parts = if c.excluded > 0, do: parts ++ ["#{c.excluded} excluded"], else: parts
    parts = if c.invalid > 0, do: parts ++ [colorize("#{c.invalid} invalid", :red, state)], else: parts

    detail = Enum.join(parts, ", ")

    write(
      "#{timestamp()} #{colorize("[============]", :cyan, state)} " <>
        "Ran: #{c.total} tests (#{detail}) (#{elapsed}ms total)\n"
    )
  end

  defp print_failure_summary(%{failures: []}), do: :ok

  defp print_failure_summary(state) do
    write("\n  Failed tests:\n\n")

    state.failures
    |> Enum.reverse()
    |> Enum.with_index(1)
    |> Enum.each(fn {%ExUnit.Test{} = test, idx} ->
      name = display_name(test)
      mod = inspect(test.module)
      file = test.tags[:file] || "?"
      line = test.tags[:line] || 0

      write(
        "    #{idx}) #{colorize(name, :red, state)} (#{mod})\n" <>
          "       #{colorize("#{file}:#{line}", [:bright, :default_color], state)}\n\n"
      )
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
      {:failed, %{tags: %{file: file}}} -> file
      _ ->
        # ExUnit.TestModule doesn't directly expose file in all versions,
        # but the module attribute __ex_unit__ has it
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

  defp write(msg) do
    IO.write(msg)
  end
end
