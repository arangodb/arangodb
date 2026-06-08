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

defmodule Toast.Client.UtilsTest do
  use ExUnit.Case, async: true

  alias Toast.Client.Utils

  describe "translate_opts/2" do
    test "translates known keys" do
      key_map = %{wait_for_sync: :waitForSync, return_new: :returnNew}
      opts = [wait_for_sync: true, return_new: false]

      assert [{:waitForSync, true}, {:returnNew, false}] = Utils.translate_opts(opts, key_map)
    end

    test "filters out unknown keys" do
      key_map = %{wait_for_sync: :waitForSync}
      opts = [wait_for_sync: true, unknown_key: "ignored"]

      assert [{:waitForSync, true}] = Utils.translate_opts(opts, key_map)
    end

    test "returns empty list for empty opts" do
      assert [] = Utils.translate_opts([], %{a: :b})
    end

    test "returns empty list when no keys match" do
      assert [] = Utils.translate_opts([foo: 1], %{bar: :baz})
    end
  end
end
