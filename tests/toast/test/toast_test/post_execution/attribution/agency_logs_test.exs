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

defmodule ToastTest.PostExecution.Attribution.AgencyLogsTest do
  use ExUnit.Case, async: true

  import ExUnit.CaptureLog

  alias ToastTest.PostExecution.Attribution.AgencyLogs

  # A crash issue produces a [-20s, 0] window around its effective_at
  # (ServerLogs crash padding is {-20_000, 0} in milliseconds).
  @crash_us 1_700_000_000_000_000
  @window_start @crash_us - 20_000 * 1_000

  defp crash_issue(effective_at \\ @crash_us) do
    %{type: :crash, scope: :suite, detail: %{effective_at: effective_at}}
  end

  # epoch_millis * 1_000 == time_us, so pick millis to land in/out of the window.
  defp entry(epoch_millis, message) do
    %{"epoch_millis" => epoch_millis, "message" => message}
  end

  defp write_dump!(dir, name, entries) do
    path = Path.join(dir, name)
    File.write!(path, Jason.encode!(%{"log" => entries}))
    path
  end

  defp windows do
    %{suite: %{started_at: @window_start, finished_at: @crash_us}, modules: %{}, tests: %{}}
  end

  describe "collect/3" do
    @tag :tmp_dir
    test "returns an empty map when there are no issues (no windows)", %{tmp_dir: tmp_dir} do
      path = write_dump!(tmp_dir, "agency-dump-d1.json", [entry(1_700_000_000_000, "x")])

      assert AgencyLogs.collect([], %{"d1" => path}, windows()) == %{}
    end

    test "returns an empty map when there are no agency dumps" do
      assert AgencyLogs.collect([crash_issue()], %{}, windows()) == %{}
    end

    @tag :tmp_dir
    test "keeps entries within the inclusive window bounds, keyed by deployment", %{
      tmp_dir: tmp_dir
    } do
      # Window is [@window_start, @crash_us], both bounds inclusive.
      entries = [
        # 1ms before the window start — outside
        entry(div(@window_start, 1_000) - 1, "before-start"),
        # exactly at the window start — inside (start inclusive)
        entry(div(@window_start, 1_000), "at-start"),
        # mid-window — inside
        entry(1_699_999_999_990, "inside"),
        # exactly at the crash / window end — inside (end inclusive)
        entry(1_700_000_000_000, "at-end"),
        # 1ms after the window end — outside
        entry(div(@crash_us, 1_000) + 1, "after-end")
      ]

      path = write_dump!(tmp_dir, "agency-dump-d1.json", entries)

      assert %{"d1" => [{start_us, end_us, kept}]} =
               AgencyLogs.collect([crash_issue()], %{"d1" => path}, windows())

      assert {start_us, end_us} == {@window_start, @crash_us}
      assert Enum.map(kept, & &1["message"]) == ["at-start", "inside", "at-end"]
    end

    @tag :tmp_dir
    test "keeps each deployment's logs under its own key", %{tmp_dir: tmp_dir} do
      d1 = write_dump!(tmp_dir, "agency-dump-d1.json", [entry(1_699_999_999_990, "from-d1")])
      d2 = write_dump!(tmp_dir, "agency-dump-d2.json", [entry(1_699_999_999_995, "from-d2")])

      result = AgencyLogs.collect([crash_issue()], %{"d1" => d1, "d2" => d2}, windows())

      assert %{"d1" => [{_, _, [e1]}], "d2" => [{_, _, [e2]}]} = result
      assert e1["message"] == "from-d1"
      assert e2["message"] == "from-d2"
    end

    @tag :tmp_dir
    test "produces a separate excerpt per merged window, dropping gaps", %{tmp_dir: tmp_dir} do
      # Two crashes 100s apart → two non-overlapping 20s windows.
      later_crash = @crash_us + 100_000_000

      entries = [
        # inside the first crash's window
        entry(1_699_999_999_990, "in-first"),
        # between the two windows — dropped
        entry(div(@crash_us, 1_000) + 50_000, "in-gap"),
        # inside the second crash's window (at its end)
        entry(div(later_crash, 1_000), "in-second")
      ]

      path = write_dump!(tmp_dir, "agency-dump-d1.json", entries)
      issues = [crash_issue(@crash_us), crash_issue(later_crash)]

      assert %{"d1" => [{_, _, first}, {_, _, second}]} =
               AgencyLogs.collect(issues, %{"d1" => path}, windows())

      assert Enum.map(first, & &1["message"]) == ["in-first"]
      assert Enum.map(second, & &1["message"]) == ["in-second"]
    end

    @tag :tmp_dir
    test "an unreadable dump yields no entries but does not affect the rest", %{tmp_dir: tmp_dir} do
      good = write_dump!(tmp_dir, "agency-dump-good.json", [entry(1_699_999_999_990, "good")])
      missing = Path.join(tmp_dir, "agency-dump-missing.json")

      log =
        capture_log(fn ->
          assert %{"good" => [{_, _, [e]}], "missing" => []} =
                   AgencyLogs.collect(
                     [crash_issue()],
                     %{"good" => good, "missing" => missing},
                     windows()
                   )

          assert e["message"] == "good"
        end)

      assert log =~ "missing"
    end
  end
end
