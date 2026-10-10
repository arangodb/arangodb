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

defmodule ToastTest.Runner.TimeoutDeadlineTest do
  use ExUnit.Case, async: false

  alias ToastTest.{Abort, EventStore}
  alias ToastTest.Runner.Timeout
  alias ToastTest.Runner.Timeout.Settings

  setup do
    Abort.clear!()
    EventStore.clear()

    on_exit(fn ->
      Abort.clear!()
      EventStore.clear()
    end)

    :ok
  end

  defp expired_suite_config do
    %{
      timeout_settings: %Settings{
        base_timeout: 300_000,
        timeout_factor: 1,
        suite_deadline: System.monotonic_time(:millisecond) - 1000,
        suite_timeout: 600_000,
        global_deadline: nil,
        global_timeout: nil,
        disable_timeouts: false
      }
    }
  end

  defp expired_global_deadline do
    System.monotonic_time(:millisecond) - 1000
  end

  describe "check_suite_deadline!/1" do
    test "fires exactly one timeout_kill event on repeated calls past deadline" do
      config = expired_suite_config()

      Timeout.check_suite_deadline!(config)
      Timeout.check_suite_deadline!(config)
      Timeout.check_suite_deadline!(config)

      kills = EventStore.timeout_kills()
      assert length(kills) == 1
      assert hd(kills).source == :suite
    end

    test "sets abort reason on first call" do
      Timeout.check_suite_deadline!(expired_suite_config())
      assert {:timeout, "Suite timeout exceeded"} = Abort.reason()
    end

    test "is a no-op when deadline is nil" do
      config = %{
        timeout_settings: %Settings{
          base_timeout: 300_000,
          timeout_factor: 1,
          suite_deadline: nil,
          suite_timeout: nil,
          global_deadline: nil,
          global_timeout: nil,
          disable_timeouts: false
        }
      }

      Timeout.check_suite_deadline!(config)
      assert is_nil(Abort.reason())
      assert EventStore.timeout_kills() == []
    end

    test "is a no-op when deadline has not been reached" do
      config = %{
        timeout_settings: %Settings{
          base_timeout: 300_000,
          timeout_factor: 1,
          suite_deadline: System.monotonic_time(:millisecond) + 60_000,
          suite_timeout: 600_000,
          global_deadline: nil,
          global_timeout: nil,
          disable_timeouts: false
        }
      }

      Timeout.check_suite_deadline!(config)
      assert is_nil(Abort.reason())
      assert EventStore.timeout_kills() == []
    end

    test "does not fire timeout_kill when already aborted for another reason" do
      Abort.abort!({:crash, "Server crashed"})

      Timeout.check_suite_deadline!(expired_suite_config())

      assert {:crash, "Server crashed"} = Abort.reason()
      assert EventStore.timeout_kills() == []
    end
  end

  describe "check_global_deadline!/1" do
    test "fires exactly one timeout_kill event on repeated calls past deadline" do
      deadline = expired_global_deadline()

      Timeout.check_global_deadline!(deadline)
      Timeout.check_global_deadline!(deadline)
      Timeout.check_global_deadline!(deadline)

      kills = EventStore.timeout_kills()
      assert length(kills) == 1
      assert hd(kills).source == :global
    end

    test "sets abort reason on first call" do
      Timeout.check_global_deadline!(expired_global_deadline())
      assert {:timeout, "Global execution timeout exceeded"} = Abort.reason()
    end

    test "is a no-op when deadline is nil" do
      Timeout.check_global_deadline!(nil)
      assert is_nil(Abort.reason())
      assert EventStore.timeout_kills() == []
    end

    test "is a no-op when deadline has not been reached" do
      Timeout.check_global_deadline!(System.monotonic_time(:millisecond) + 60_000)
      assert is_nil(Abort.reason())
      assert EventStore.timeout_kills() == []
    end

    test "does not fire timeout_kill when already aborted for another reason" do
      Abort.abort!({:crash, "Server crashed"})

      Timeout.check_global_deadline!(expired_global_deadline())

      assert {:crash, "Server crashed"} = Abort.reason()
      assert EventStore.timeout_kills() == []
    end
  end
end
