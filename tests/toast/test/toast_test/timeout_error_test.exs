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

defmodule ToastTest.TimeoutErrorTest do
  use ExUnit.Case, async: true

  alias ToastTest.TimeoutError

  describe "message/1" do
    test "normal test timeout" do
      error = TimeoutError.exception(timeout: 300_000, type: "test")
      msg = Exception.message(error)
      assert msg =~ "test timed out after 300s"
      assert msg =~ "per test by setting"
    end

    test "global deadline shows configured timeout" do
      error =
        TimeoutError.exception(
          timeout: 45_000,
          type: "test",
          source: {:global_deadline, 3_600_000}
        )

      msg = Exception.message(error)
      assert msg =~ "test killed after 45s — global execution timeout reached"
      assert msg =~ "Global timeout: 3600s"
      assert msg =~ "--global-timeout"
      refute msg =~ "per test by setting"
    end

    test "suite deadline shows configured timeout" do
      error =
        TimeoutError.exception(
          timeout: 10_000,
          type: "test",
          source: {:suite_deadline, 1_800_000}
        )

      msg = Exception.message(error)
      assert msg =~ "test killed after 10s — suite timeout reached"
      assert msg =~ "Suite timeout: 1800s"
      assert msg =~ "deployment_config :timeout"
      refute msg =~ "per test by setting"
    end

    test "sub-second timeout formats as milliseconds" do
      error = TimeoutError.exception(timeout: 500, type: "test")
      assert Exception.message(error) =~ "500ms"
    end

    test "source defaults to :test" do
      error = TimeoutError.exception(timeout: 5_000, type: "test")
      assert error.source == :test
    end
  end
end
