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

defmodule ToastTest.Formatting.AgencyLogTest do
  use ExUnit.Case, async: true

  alias ToastTest.Formatting.AgencyLog

  describe "format_entry/2 — summary" do
    test "includes term, raft index (from _key), and client id" do
      entry = %{
        "term" => 3,
        "_key" => "00000000000000000042",
        "clientId" => "97ffda9e-e128-4800-ba0f-d6669dec77be"
      }

      {summary, _detail} = AgencyLog.format_entry(entry)

      assert summary =~ "term=3"
      assert summary =~ "#42"
      assert summary =~ "client=97ffda9e"
    end

    test "omits the client part when clientId is empty" do
      {summary, _} = AgencyLog.format_entry(%{"term" => 0, "_key" => "0", "clientId" => ""})

      refute summary =~ "client="
    end

    test "omits parts that are absent" do
      {summary, _} = AgencyLog.format_entry(%{"_key" => "00000000000000000007"})

      assert summary == "#7"
    end
  end

  describe "format_entry/2 — request detail" do
    test "renders the request as pretty-printed JSON" do
      entry = %{"request" => %{"arango/Foo" => %{"op" => "set"}}}

      {_summary, detail} = AgencyLog.format_entry(entry, %{limit: :unlimited})

      assert detail =~ ~s("arango/Foo")
      assert detail =~ ~s("op")
      assert detail =~ ~s("set")
      # Pretty-printing puts nested keys on their own indented lines.
      assert detail =~ "\n"
    end

    test "returns nil detail when the request is absent or empty" do
      assert {_, nil} = AgencyLog.format_entry(%{"term" => 1})
      assert {_, nil} = AgencyLog.format_entry(%{"request" => %{}})
    end

    test "truncates the rendered request to the body limit with an ellipsis" do
      big = %{"request" => %{"k" => String.duplicate("x", 500)}}

      {_summary, detail} = AgencyLog.format_entry(big, %{limit: 50})

      # The JSON payload is sliced to the limit before indentation; counting
      # payload chars (here, "x") avoids coupling to the indentation width.
      assert String.contains?(detail, "…")
      assert detail |> String.graphemes() |> Enum.count(&(&1 == "x")) <= 50
    end

    test "does not truncate when the limit is :unlimited" do
      big = %{"request" => %{"k" => String.duplicate("x", 500)}}

      {_summary, detail} = AgencyLog.format_entry(big, %{limit: :unlimited})

      refute String.contains?(detail, "…")
      assert detail =~ String.duplicate("x", 500)
    end

    test "defaults the limit to 500 characters" do
      big = %{"request" => %{"k" => String.duplicate("x", 1_000)}}

      {_summary, detail} = AgencyLog.format_entry(big)

      assert String.contains?(detail, "…")
      assert detail |> String.graphemes() |> Enum.count(&(&1 == "x")) <= 500
    end
  end
end
