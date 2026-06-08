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

defmodule ToastTest.Runner.TimeoutTest do
  use ExUnit.Case, async: true

  alias ToastTest.Runner.Timeout
  alias ToastTest.Runner.Timeout.Settings

  defp make_config(settings_overrides) do
    settings =
      Map.merge(
        %Settings{
          base_timeout: 300_000,
          timeout_factor: 1,
          suite_deadline: nil,
          suite_timeout: nil,
          global_deadline: nil,
          global_timeout: nil,
          disable_timeouts: false
        },
        Map.new(settings_overrides)
      )

    %{timeout_settings: settings}
  end

  defp tags(overrides \\ %{}), do: Map.merge(%{test_type: :test}, overrides)

  describe "get_timeout/2 — basic" do
    test "returns base timeout with :test source when no deadline" do
      config = make_config(base_timeout: 5_000)
      assert {5_000, :test} = Timeout.get_timeout(config, tags())
    end

    test "per-test tag overrides base timeout" do
      config = make_config(base_timeout: 5_000)
      assert {10_000, :test} = Timeout.get_timeout(config, tags(%{timeout: 10_000}))
    end

    test "timeout_factor scales the timeout" do
      config = make_config(base_timeout: 5_000, timeout_factor: 3)
      assert {15_000, :test} = Timeout.get_timeout(config, tags())
    end

    test "attach-debugger disables timeouts" do
      config = make_config(base_timeout: 5_000, disable_timeouts: true)
      assert {:infinity, :test} = Timeout.get_timeout(config, tags())
    end
  end

  describe "get_timeout/2 — suite deadline clamping" do
    test "clamped to suite deadline when test timeout exceeds it" do
      suite_deadline = System.monotonic_time(:millisecond) + 100

      config =
        make_config(
          base_timeout: 500_000,
          suite_deadline: suite_deadline,
          suite_timeout: 600_000
        )

      {timeout, source} = Timeout.get_timeout(config, tags())
      assert timeout <= 101
      assert source == {:suite_deadline, 600_000}
    end

    test "not clamped when test timeout fits within suite deadline" do
      suite_deadline = System.monotonic_time(:millisecond) + 500_000
      config = make_config(base_timeout: 5_000, suite_deadline: suite_deadline)

      assert {5_000, :test} = Timeout.get_timeout(config, tags())
    end
  end

  describe "get_timeout/2 — global deadline clamping" do
    test "clamped to global deadline when it is tighter than suite deadline" do
      now = System.monotonic_time(:millisecond)
      global_deadline = now + 100
      suite_deadline = now + 500_000

      config =
        make_config(
          base_timeout: 500_000,
          suite_deadline: suite_deadline,
          suite_timeout: 600_000,
          global_deadline: global_deadline,
          global_timeout: 3_600_000
        )

      {timeout, source} = Timeout.get_timeout(config, tags())
      assert timeout <= 101
      assert source == {:global_deadline, 3_600_000}
    end

    test "reports suite_deadline when suite is tighter than global" do
      now = System.monotonic_time(:millisecond)
      suite_deadline = now + 100
      global_deadline = now + 500_000

      config =
        make_config(
          base_timeout: 500_000,
          suite_deadline: suite_deadline,
          suite_timeout: 600_000,
          global_deadline: global_deadline,
          global_timeout: 3_600_000
        )

      {timeout, source} = Timeout.get_timeout(config, tags())
      assert timeout <= 101
      assert source == {:suite_deadline, 600_000}
    end

    test "global deadline alone (no suite deadline) clamps with global_deadline" do
      global_deadline = System.monotonic_time(:millisecond) + 100

      config =
        make_config(
          base_timeout: 500_000,
          suite_deadline: nil,
          global_deadline: global_deadline,
          global_timeout: 3_600_000
        )

      {timeout, source} = Timeout.get_timeout(config, tags())
      assert timeout <= 101
      assert source == {:global_deadline, 3_600_000}
    end
  end
end
