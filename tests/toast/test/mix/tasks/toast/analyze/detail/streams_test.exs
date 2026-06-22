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

defmodule Mix.Tasks.Toast.Analyze.Detail.StreamsTest do
  use ExUnit.Case, async: false

  alias Mix.Tasks.Toast.Analyze.Detail.Streams

  @crash_us DateTime.to_unix(~U[2026-03-09 10:00:00Z], :microsecond)

  setup do
    Mix.shell(Mix.Shell.Process)
    on_exit(fn -> Mix.shell(Mix.Shell.IO) end)
    :ok
  end

  defp collect_output(acc \\ []) do
    receive do
      {:mix_shell, :info, [msg]} -> collect_output([msg | acc])
    after
      0 -> acc |> Enum.reverse() |> Enum.join("\n")
    end
  end

  # An agency log entry within the crash window ([-5s, 0] for crashes).
  defp agency_entry(time_us, key, request),
    do: %{"time_us" => time_us, "_key" => key, "term" => 1, "request" => request}

  defp crash_issue(agency_logs) do
    %{
      type: :crash,
      scope: :suite,
      time_bounds: {@crash_us, @crash_us},
      detail: %{effective_at: @crash_us, server: "cluster-00-coordinator-0"},
      servers: %{},
      deployments: %{},
      events: [],
      traffic: [],
      agency_logs: agency_logs
    }
  end

  defp display(overrides) do
    base = %{
      log: %{enabled: false, window_spec: nil},
      traffic: %{
        enabled: false,
        window_spec: nil,
        body_limit: 200,
        raw_body: false,
        all_headers: false
      },
      agency: %{enabled: false, window_spec: nil, body_limit: 200},
      event: %{detail: :none},
      backtrace: %{}
    }

    Map.merge(base, overrides)
  end

  describe "agency log stream" do
    test "renders an in-window agency entry tagged by deployment with its request" do
      logs = %{
        "cluster-00" => [
          {@crash_us - 5_000_000, @crash_us,
           [agency_entry(@crash_us - 1_000_000, "00000000000000000007", %{"arango/Foo" => 1})]}
        ]
      }

      Streams.print(
        crash_issue(logs),
        display(%{agency: %{enabled: true, window_spec: nil, body_limit: 200}}),
        false
      )

      out = collect_output()

      assert out =~ "Agency logs"
      assert out =~ "[AGENCY cluster-00]"
      assert out =~ "#7"
      assert out =~ ~s("arango/Foo")
    end

    test "omits entries outside the crash window" do
      logs = %{
        "cluster-00" => [
          {@crash_us - 5_000_000, @crash_us,
           [agency_entry(@crash_us - 60_000_000, "00000000000000000007", %{"k" => 1})]}
        ]
      }

      Streams.print(
        crash_issue(logs),
        display(%{agency: %{enabled: true, window_spec: nil, body_limit: 200}}),
        false
      )

      out = collect_output()

      refute out =~ "[AGENCY cluster-00]"
    end

    test "is absent when agency display is disabled" do
      logs = %{
        "cluster-00" => [
          {@crash_us - 5_000_000, @crash_us,
           [agency_entry(@crash_us - 1_000_000, "00000000000000000007", %{"k" => 1})]}
        ]
      }

      Streams.print(crash_issue(logs), display(%{event: %{detail: :basic}}), false)
      out = collect_output()

      refute out =~ "[AGENCY cluster-00]"
    end
  end
end
