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

defmodule ToastTest.BuildProperties do
  @moduledoc """
  Detects build capabilities from `arangod --version` and defines the
  tag vocabulary used to filter JavaScript test modules.

  ## Tag Reference

  Tags marked **excluded by default** are automatically added to the exclude
  list unless explicitly included via `--include`.

  ### Deployment mode (mutually exclusive, always applied)

  | Tag              | JS filename segment | Meaning                          |
  |------------------|---------------------|----------------------------------|
  | `:cluster_only`  | `-cluster`          | Requires cluster deployment      |
  | `:single_only`   | `-noncluster`       | Requires single-server deployment|

  ### Build capability tags

  | Tag                    | JS filename segment | Excluded by default? | Meaning                                    |
  |------------------------|---------------------|----------------------|--------------------------------------------|
  | `:full_only`           | `-nightly`          | **yes**              | Only run in full/nightly builds            |
  | `:arangosearch`        | `-arangosearch`     | no                   | Requires ArangoSearch                      |
  | `:geo`                 | `-geo`              | no                   | Geo/spatial test                           |
  | `:graph`               | `-graph`            | no                   | Graph test                                 |
  | `:failure_points`      | `-fp`               | no (auto-detected)   | Requires failure point support in build    |
  | `:replication2`        | `-r2`               | **yes**              | Requires replication version 2             |
  | `:server_javascript`   | `-sjs`              | no (auto-detected)   | Requires server-side JavaScript (V8)       |
  | `:skip_arm`            | `-noarm`            | no (auto-detected)   | Cannot run on ARM/AArch64                  |
  | `:skip_instrumented`   | `-noinstr`          | no (auto-detected)   | Cannot run with sanitizer or coverage      |
  | `:skip_sanitizer`      | `-noasan`           | no (auto-detected)   | Cannot run with sanitizer (asan or tsan)   |
  | `:skip_coverage`       | `-nocov`            | no (auto-detected)   | Cannot run with coverage build             |

  Auto-detected tags are excluded automatically when the build doesn't
  support them (e.g., `:failure_points` tests are excluded when the binary
  was built without `USE_FAILURE_TESTS`).
  """

  require Logger

  defstruct [
    :arm,
    :asan,
    :tsan,
    :coverage,
    :failure_tests,
    :enterprise,
    :replication2,
    :v8,
    raw: %{}
  ]

  @type t :: %__MODULE__{
          arm: boolean(),
          asan: boolean(),
          tsan: boolean(),
          coverage: boolean(),
          failure_tests: boolean(),
          enterprise: boolean(),
          replication2: boolean(),
          v8: boolean(),
          raw: %{String.t() => String.t()}
        }

  # Maps JS filename segments to ExUnit tag names.
  @segment_tags [
    {"cluster", :cluster_only},
    {"noncluster", :single_only},
    {"arangosearch", :arangosearch},
    {"nightly", :full_only},
    {"geo", :geo},
    {"graph", :graph},
    {"noarm", :skip_arm},
    {"noinstr", :skip_instrumented},
    {"fp", :failure_points},
    {"r2", :replication2},
    {"noasan", :skip_sanitizer},
    {"nocov", :skip_coverage},
    {"sjs", :server_javascript}
  ]

  @default_exclusions [
    :full_only,
    :replication2
  ]

  @doc "Returns the mapping from JS filename segments to ExUnit tags."
  @spec segment_tags() :: [{String.t(), atom()}]
  def segment_tags, do: @segment_tags

  @doc "Returns tags that are always excluded unless explicitly included."
  @spec default_exclusions() :: [atom()]
  def default_exclusions, do: @default_exclusions

  @doc """
  Detect build properties by running `arangod --version`.

  Returns `{:ok, properties}` or `{:error, reason}`.
  """
  @spec detect(Path.t()) :: {:ok, t()} | {:error, String.t()}
  def detect(arangod_path) do
    case System.cmd(arangod_path, ["--version"], stderr_to_stdout: true, env: []) do
      {output, 0} ->
        {:ok, parse(output)}

      {output, code} ->
        {:error, "arangod --version exited with code #{code}: #{String.slice(output, 0, 200)}"}
    end
  end

  @doc "Parse the key-value output from `arangod --version`."
  @spec parse(String.t()) :: t()
  def parse(output) do
    raw =
      output
      |> String.split("\n")
      |> Enum.reduce(%{}, fn line, acc ->
        case String.split(line, ":", parts: 2) do
          [key, value] -> Map.put(acc, String.trim(key), String.trim(value))
          _ -> acc
        end
      end)

    %__MODULE__{
      arm: raw["arm"] == "true",
      asan: raw["asan"] == "true",
      tsan: raw["tsan"] == "true",
      coverage: raw["coverage"] == "true",
      failure_tests: raw["failure-tests"] == "true",
      enterprise: raw["license"] == "enterprise",
      replication2: raw["replication2-enabled"] == "true",
      v8: raw["v8-version"] != nil,
      raw: raw
    }
  end

  @doc """
  Build the exclusion list for this build.

  Combines default exclusions with auto-detected exclusions based on
  what the build supports.
  """
  @spec exclusions(t()) :: [atom()]
  def exclusions(%__MODULE__{} = props) do
    auto =
      []
      |> maybe_exclude(props.arm, :skip_arm)
      |> maybe_exclude(props.asan or props.tsan, :skip_sanitizer)
      |> maybe_exclude(props.asan or props.tsan, :skip_instrumented)
      |> maybe_exclude(props.coverage, :skip_coverage)
      |> maybe_exclude(props.coverage, :skip_instrumented)
      |> maybe_exclude(not props.failure_tests, :failure_points)
      |> maybe_exclude(not props.v8, :server_javascript)

    Enum.uniq(@default_exclusions ++ auto)
  end

  defp maybe_exclude(acc, true, tag), do: [tag | acc]
  defp maybe_exclude(acc, false, _tag), do: acc
end
