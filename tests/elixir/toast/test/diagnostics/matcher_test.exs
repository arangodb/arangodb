defmodule Toast.Diagnostics.MatcherTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.Matcher

  import Toast.DiagnosticsTestHelpers,
    only: [base_time: 0, at: 1, make_test: 0, make_test: 1]

  defp at_ms(milliseconds), do: DateTime.add(base_time(), milliseconds, :millisecond)

  defp make_item(opts \\ []) do
    %{
      timestamp: Keyword.get(opts, :timestamp, at(5)),
      content: Keyword.get(opts, :content, "some diagnostic content")
    }
  end

  describe "calculate_confidence/4" do
    test "returns :high when timestamp is between start and end" do
      assert :high == Matcher.calculate_confidence(at(5), at(0), at(10))
    end

    test "returns :high when timestamp equals test start" do
      assert :high == Matcher.calculate_confidence(at(0), at(0), at(10))
    end

    test "returns :high when timestamp equals test end" do
      assert :high == Matcher.calculate_confidence(at(10), at(0), at(10))
    end

    test "returns :low when timestamp is within tolerance after test end" do
      assert :low == Matcher.calculate_confidence(at(13), at(0), at(10))
    end

    test "returns :low at exactly the tolerance boundary (5s default)" do
      # 5000ms after test_end => exactly at boundary => :low
      assert :low == Matcher.calculate_confidence(at(15), at(0), at(10))
    end

    test "returns :none when timestamp is 1ms past the tolerance boundary" do
      # 5001ms after test_end => just past boundary => :none
      tolerance_boundary_plus_one = DateTime.add(at(10), 5001, :millisecond)
      assert :none == Matcher.calculate_confidence(tolerance_boundary_plus_one, at(0), at(10))
    end

    test "returns :none when timestamp is before test start" do
      assert :none == Matcher.calculate_confidence(at(-1), at(0), at(10))
    end

    test "returns :none when timestamp is well before test start" do
      assert :none == Matcher.calculate_confidence(at(-100), at(0), at(10))
    end

    test "returns :none when timestamp is well past tolerance" do
      assert :none == Matcher.calculate_confidence(at(30), at(0), at(10))
    end

    test "respects custom tolerance_seconds" do
      # 2s tolerance: at(12) is 2000ms after test_end => exactly at boundary => :low
      assert :low == Matcher.calculate_confidence(at(12), at(0), at(10), 2)
      # 2001ms after test_end => past boundary => :none
      past_2s = DateTime.add(at(10), 2001, :millisecond)
      assert :none == Matcher.calculate_confidence(past_2s, at(0), at(10), 2)
    end

    test "zero tolerance means no low-confidence window" do
      # Exactly at test_end is :high (within window)
      assert :high == Matcher.calculate_confidence(at(10), at(0), at(10), 0)
      # 1ms after test_end with zero tolerance => :none
      just_after = DateTime.add(at(10), 1, :millisecond)
      assert :none == Matcher.calculate_confidence(just_after, at(0), at(10), 0)
    end

    test "handles sub-second precision" do
      start = at(0)
      finish = at_ms(500)
      within = at_ms(250)
      assert :high == Matcher.calculate_confidence(within, start, finish)
    end

    test "handles zero-length test window (start == end)" do
      assert :high == Matcher.calculate_confidence(at(5), at(5), at(5))
      assert :low == Matcher.calculate_confidence(at(7), at(5), at(5))
      assert :none == Matcher.calculate_confidence(at(4), at(5), at(5))
    end
  end

  describe "match/4 with empty or nil inputs" do
    test "returns empty result for empty item list" do
      result = Matcher.match([], [make_test()], :error)
      assert result == %{matched: [], unmatched: []}
    end

    test "returns empty result for nil test_results" do
      result = Matcher.match([make_item()], nil, :error)
      assert result == %{matched: [], unmatched: []}
    end

    test "returns empty result for both empty" do
      result = Matcher.match([], nil, :error)
      assert result == %{matched: [], unmatched: []}
    end
  end

  describe "match/4 with single item" do
    test "matches item within test window with high confidence" do
      item = make_item(timestamp: at(5))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = Matcher.match([item], [test], :error)

      assert [
               %{
                 confidence: :high,
                 module: SmokeTest.VersionTest,
                 test: "test server version",
                 error: ^item
               }
             ] =
               result.matched

      assert result.unmatched == []
    end

    test "matches item in tolerance window with low confidence" do
      item = make_item(timestamp: at(13))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = Matcher.match([item], [test], :error)

      assert [%{confidence: :low}] = result.matched
      assert result.unmatched == []
    end

    test "returns item as unmatched when no test correlates" do
      item = make_item(timestamp: at(30))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = Matcher.match([item], [test], :error)

      assert result.matched == []
      assert result.unmatched == [item]
    end
  end

  describe "match/4 with multiple items and tests" do
    test "matches different items to different tests" do
      item1 = make_item(timestamp: at(5), content: "first")
      item2 = make_item(timestamp: at(25), content: "second")

      test1 = make_test(name: "test one", started_at: at(0), finished_at: at(10))
      test2 = make_test(name: "test two", started_at: at(20), finished_at: at(30))

      result = Matcher.match([item1, item2], [test1, test2], :crash)

      assert length(result.matched) == 2
      names = Enum.map(result.matched, & &1.test)
      assert "test one" in names
      assert "test two" in names
      assert result.unmatched == []
    end

    test "multiple items can match the same test" do
      item1 = make_item(timestamp: at(3), content: "first")
      item2 = make_item(timestamp: at(7), content: "second")

      test = make_test(started_at: at(0), finished_at: at(10))
      result = Matcher.match([item1, item2], [test], :error)

      assert length(result.matched) == 2
      assert Enum.all?(result.matched, &(&1.confidence == :high))
      assert result.unmatched == []
    end

    test "preserves order: matched and unmatched lists are in input order" do
      item1 = make_item(timestamp: at(5), content: "first")
      item2 = make_item(timestamp: at(99), content: "unmatched")
      item3 = make_item(timestamp: at(25), content: "third")

      test1 = make_test(name: "test one", started_at: at(0), finished_at: at(10))
      test2 = make_test(name: "test two", started_at: at(20), finished_at: at(30))

      result = Matcher.match([item1, item2, item3], [test1, test2], :error)

      assert [%{test: "test one"}, %{test: "test two"}] = result.matched
      assert [^item2] = result.unmatched
    end

    test "prefers high confidence over low confidence" do
      # item falls in tolerance window of test1 but within window of test2
      item = make_item(timestamp: at(15))
      test1 = make_test(name: "test one", started_at: at(0), finished_at: at(10))
      test2 = make_test(name: "test two", started_at: at(12), finished_at: at(20))

      result = Matcher.match([item], [test1, test2], :error)

      assert [%{confidence: :high, test: "test two"}] = result.matched
    end

    test "stops searching after finding high confidence match" do
      # item is in window of test1 (high), also in window of test2 — should pick test1
      item = make_item(timestamp: at(5))
      test1 = make_test(name: "test one", started_at: at(0), finished_at: at(10))
      test2 = make_test(name: "test two", started_at: at(3), finished_at: at(8))

      result = Matcher.match([item], [test1, test2], :error)

      # Should halt at first high-confidence match (test1)
      assert [%{confidence: :high, test: "test one"}] = result.matched
    end
  end

  describe "match/4 uses item_key correctly" do
    test "uses :error as item key" do
      item = make_item(timestamp: at(5))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = Matcher.match([item], [test], :error)
      assert [entry] = result.matched
      assert Map.has_key?(entry, :error)
      assert entry.error == item
    end

    test "uses :crash as item key" do
      item = make_item(timestamp: at(5))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = Matcher.match([item], [test], :crash)
      assert [entry] = result.matched
      assert Map.has_key?(entry, :crash)
      assert entry.crash == item
    end

    test "uses arbitrary atom as item key" do
      item = make_item(timestamp: at(5))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = Matcher.match([item], [test], :custom_key)
      assert [entry] = result.matched
      assert Map.has_key?(entry, :custom_key)
    end
  end

  describe "match/4 handles tests with missing timestamps" do
    test "skips tests with nil started_at and finished_at" do
      item = make_item(timestamp: at(5))
      test = make_test(started_at: nil, finished_at: nil)

      result = Matcher.match([item], [test], :error)

      assert result.matched == []
      assert result.unmatched == [item]
    end

    test "skips tests with nil started_at" do
      item = make_item(timestamp: at(5))
      test = make_test(started_at: nil, finished_at: at(10))

      result = Matcher.match([item], [test], :error)

      assert result.matched == []
      assert result.unmatched == [item]
    end

    test "skips tests with nil finished_at" do
      item = make_item(timestamp: at(5))
      test = make_test(started_at: at(0), finished_at: nil)

      result = Matcher.match([item], [test], :error)

      assert result.matched == []
      assert result.unmatched == [item]
    end

    test "matches against valid tests even when some tests have nil timestamps" do
      item = make_item(timestamp: at(5))
      bad_test = make_test(name: "broken", started_at: nil, finished_at: nil)
      good_test = make_test(name: "good", started_at: at(0), finished_at: at(10))

      result = Matcher.match([item], [bad_test, good_test], :error)

      assert [%{confidence: :high, test: "good"}] = result.matched
    end
  end

  describe "match/4 with empty test list" do
    test "returns all items as unmatched for empty test list" do
      item = make_item(timestamp: at(5))

      result = Matcher.match([item], [], :error)

      assert result.matched == []
      assert result.unmatched == [item]
    end
  end

  describe "match/4 with tolerance option" do
    test "respects custom tolerance_seconds" do
      item = make_item(timestamp: at(12))
      test = make_test(started_at: at(0), finished_at: at(10))

      result_low = Matcher.match([item], [test], :error, tolerance_seconds: 3)
      assert [%{confidence: :low}] = result_low.matched

      result_none = Matcher.match([item], [test], :error, tolerance_seconds: 1)
      assert result_none.matched == []
      assert length(result_none.unmatched) == 1
    end
  end

  describe "confidence_label/1" do
    test "returns high confidence label when :high is present" do
      assert "high confidence" == Matcher.confidence_label([:high])
    end

    test "returns high confidence label when both :high and :low are present" do
      assert "high confidence" == Matcher.confidence_label([:low, :high])
    end

    test "returns low confidence label when only :low is present" do
      assert "low confidence" == Matcher.confidence_label([:low])
    end

    test "returns empty string for empty list" do
      assert "" == Matcher.confidence_label([])
    end

    test "returns empty string for list with only :none" do
      assert "" == Matcher.confidence_label([:none])
    end

    test ":high takes precedence regardless of order" do
      assert "high confidence" == Matcher.confidence_label([:none, :low, :high, :none])
    end

    test ":low takes precedence over :none" do
      assert "low confidence" == Matcher.confidence_label([:none, :low, :none])
    end
  end

  describe "empty_result/0" do
    test "returns map with empty matched and unmatched lists" do
      result = Matcher.empty_result()
      assert result == %{matched: [], unmatched: []}
    end
  end
end
