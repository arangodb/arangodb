################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule ToastTest.TimeoutError do
  @moduledoc "Raised when a test exceeds its configured timeout."

  defexception [:timeout, :type, source: :test]

  @impl true
  def message(%{timeout: timeout, type: type, source: source}) do
    header(type, timeout, source) <> "\n\n" <> hints(source)
  end

  defp header(type, timeout, {:global_deadline, _}) do
    "#{type} killed after #{format_time(timeout)} — global execution timeout reached"
  end

  defp header(type, timeout, {:suite_deadline, _}) do
    "#{type} killed after #{format_time(timeout)} — suite timeout reached"
  end

  defp header(type, timeout, _source) do
    "#{type} timed out after #{format_time(timeout)}"
  end

  defp hints({:global_deadline, configured}) do
    "Global timeout: #{format_time(configured)}. " <>
      "Change via \"--global-timeout\" or TOAST_GLOBAL_TIMEOUT env var."
  end

  defp hints({:suite_deadline, configured}) do
    "Suite timeout: #{format_time(configured)}. " <>
      "Change via the suite's deployment_config :timeout option."
  end

  defp hints(_source) do
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
