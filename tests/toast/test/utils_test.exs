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

defmodule Toast.UtilsTest do
  use ExUnit.Case, async: true

  import Toast.Utils

  describe "compact/1" do
    test "removes nil values" do
      assert compact([1, nil, 2, nil, 3]) == [1, 2, 3]
    end

    test "returns empty list when all nil" do
      assert compact([nil, nil]) == []
    end

    test "returns same list when no nils" do
      assert compact([1, 2, 3]) == [1, 2, 3]
    end
  end

  describe "compact_join/2" do
    test "joins non-nil values" do
      assert compact_join(["a", nil, "b"], "-") == "a-b"
    end

    test "uses empty string joiner by default" do
      assert compact_join(["a", nil, "b"]) == "ab"
    end
  end
end
