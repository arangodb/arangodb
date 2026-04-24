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
end
