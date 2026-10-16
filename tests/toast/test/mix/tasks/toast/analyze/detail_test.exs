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

defmodule Mix.Tasks.Toast.Analyze.DetailTest do
  use ExUnit.Case, async: true

  alias Mix.Tasks.Toast.Analyze.Detail

  # --- issue_spec?/1 ---
  #
  # Returns true for valid issue specs: keywords, number strings, and range strings.
  # Returns false for anything else.

  describe "issue_spec?/1 — keywords" do
    test "all" do
      assert Detail.issue_spec?("all")
    end

    test "crashes" do
      assert Detail.issue_spec?("crashes")
    end

    test "test_failures" do
      assert Detail.issue_spec?("test_failures")
    end

    test "sanitizer" do
      assert Detail.issue_spec?("sanitizer")
    end

    test "timeouts" do
      assert Detail.issue_spec?("timeouts")
    end
  end

  describe "issue_spec?/1 — number strings" do
    test "single digit" do
      assert Detail.issue_spec?("1")
    end

    test "multi-digit number" do
      assert Detail.issue_spec?("42")
    end

    test "large number" do
      assert Detail.issue_spec?("1000")
    end
  end

  describe "issue_spec?/1 — range strings" do
    test "simple range" do
      assert Detail.issue_spec?("2-4")
    end

    test "single-item range (same number both sides)" do
      assert Detail.issue_spec?("3-3")
    end

    test "range with multi-digit bounds" do
      assert Detail.issue_spec?("10-20")
    end
  end

  describe "issue_spec?/1 — non-matching inputs" do
    test "empty string returns false" do
      refute Detail.issue_spec?("")
    end

    test "plain word that is not a keyword returns false" do
      refute Detail.issue_spec?("crash")
    end

    test "partial keyword returns false" do
      refute Detail.issue_spec?("crash")
    end

    test "path-like string returns false" do
      refute Detail.issue_spec?("/some/path")
    end

    test "number with trailing non-digit returns false" do
      refute Detail.issue_spec?("1a")
    end

    test "range with non-numeric parts returns false" do
      refute Detail.issue_spec?("a-b")
    end

    test "triple component not valid (only single range dash allowed)" do
      refute Detail.issue_spec?("1-2-3")
    end

    test "negative number returns false" do
      refute Detail.issue_spec?("-1")
    end
  end

  # --- parse_event_detail/2 ---
  #
  # Parses an event detail string into an atom.
  # When the value is nil, defaults to :basic if any stream is enabled, :none otherwise.
  # Explicit values always win regardless of the stream flag.

  describe "parse_event_detail/2 — nil defaults based on stream flag" do
    test "defaults to :basic when a stream is enabled" do
      assert Detail.parse_event_detail(nil, true) == :basic
    end

    test "defaults to :none when no stream is enabled" do
      assert Detail.parse_event_detail(nil, false) == :none
    end
  end

  describe "parse_event_detail/2 — explicit values override stream flag" do
    test ~s("none" with stream enabled) do
      assert Detail.parse_event_detail("none", true) == :none
    end

    test ~s("none" with stream disabled) do
      assert Detail.parse_event_detail("none", false) == :none
    end

    test ~s("basic" with stream disabled) do
      assert Detail.parse_event_detail("basic", false) == :basic
    end

    test ~s("basic" with stream enabled) do
      assert Detail.parse_event_detail("basic", true) == :basic
    end

    test ~s("full" with stream enabled) do
      assert Detail.parse_event_detail("full", true) == :full
    end

    test ~s("full" with stream disabled) do
      assert Detail.parse_event_detail("full", false) == :full
    end
  end

  describe "parse_event_detail/2 — invalid value" do
    test "raises on unrecognized value" do
      assert_raise Mix.Error, fn ->
        Detail.parse_event_detail("invalid", true)
      end
    end
  end
end
