defmodule ToastTest.TimeoutError do
  @moduledoc "Raised when a test exceeds its configured timeout."

  defexception [:timeout, :type]

  @impl true
  def message(%{timeout: timeout, type: type}) do
    "#{type} timed out after #{format_time(timeout)}.\n\n" <>
      "You can change the timeout:\n\n" <>
      "  1. per test by setting \"@tag timeout: x\" (accepts :infinity)\n" <>
      "  2. per test module by setting \"@moduletag timeout: x\" (accepts :infinity)\n" <>
      "  3. globally via the test_timeout config option\n" <>
      "  4. via the \"--test-timeout x\" CLI option (overrides config)\n\n" <>
      "where \"x\" is the timeout given as integer in milliseconds.\n\n" <>
      "Note: timeouts are scaled by \"timeout_factor\" for sanitizer builds."
  end

  defp format_time(ms) when ms >= 1_000, do: "#{div(ms, 1_000)}s"
  defp format_time(ms), do: "#{ms}ms"
end
