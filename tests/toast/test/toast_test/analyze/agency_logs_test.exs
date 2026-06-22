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

defmodule ToastTest.Analyze.AgencyLogsTest do
  use ExUnit.Case, async: true

  alias ToastTest.Analyze.AgencyLogs

  defp entry(time_us, key), do: %{"time_us" => time_us, "_key" => key}

  describe "extract/2" do
    test "returns an empty list when there are no agency logs" do
      assert AgencyLogs.extract(%{}, {0, 100}) == []
    end

    test "keeps only entries within the inclusive window, tagged per deployment" do
      logs = %{
        "cluster-00" => [
          {0, 200, [entry(50, "a"), entry(150, "b"), entry(250, "c")]}
        ]
      }

      assert [{{:agency, "cluster-00"}, kept}] = AgencyLogs.extract(logs, {100, 200})
      assert Enum.map(kept, & &1["_key"]) == ["b"]
    end

    test "treats both window bounds as inclusive" do
      logs = %{"d1" => [{0, 300, [entry(100, "lo"), entry(200, "hi")]}]}

      assert [{{:agency, "d1"}, kept}] = AgencyLogs.extract(logs, {100, 200})
      assert Enum.map(kept, & &1["_key"]) == ["lo", "hi"]
    end

    test "flattens entries across multiple chunks and sorts by time" do
      logs = %{
        "d1" => [
          {0, 100, [entry(90, "first")]},
          {100, 200, [entry(110, "second")]}
        ]
      }

      assert [{{:agency, "d1"}, kept}] = AgencyLogs.extract(logs, {0, 200})
      assert Enum.map(kept, & &1["_key"]) == ["first", "second"]
    end

    test "drops a deployment entirely when none of its entries are in the window" do
      logs = %{
        "d1" => [{0, 100, [entry(50, "in")]}],
        "d2" => [{0, 100, [entry(999, "out")]}]
      }

      assert [{{:agency, "d1"}, _}] = AgencyLogs.extract(logs, {0, 100})
    end

    test "keeps each deployment under its own tag, sorted by deployment id" do
      logs = %{
        "d2" => [{0, 100, [entry(60, "two")]}],
        "d1" => [{0, 100, [entry(50, "one")]}]
      }

      assert [{{:agency, "d1"}, _}, {{:agency, "d2"}, _}] = AgencyLogs.extract(logs, {0, 100})
    end
  end
end
