defmodule ToastTest.Runner.TestFilterTest do
  use ExUnit.Case, async: true

  alias ToastTest.Runner.TestFilter

  defp make_test(module, name, tags \\ %{}) do
    %ExUnit.Test{
      module: module,
      name: name,
      tags: Map.merge(%{}, tags),
      state: nil,
      logs: "",
      time: 0,
      parameters: %{}
    }
  end

  defp no_filters do
    %{include: [], exclude: [], only_test_ids: nil, test_name_pattern: nil}
  end

  describe "filter/2" do
    test "returns all tests when no filters are set" do
      tests = [make_test(MyMod, :"test one"), make_test(MyMod, :"test two")]

      {to_run, to_skip} = TestFilter.filter(no_filters(), tests)

      assert length(to_run) == 2
      assert to_skip == []
    end

    test "returns empty lists for empty test list" do
      assert {[], []} = TestFilter.filter(no_filters(), [])
    end

    test "filters by only_test_ids" do
      t1 = make_test(MyMod, :"test one")
      t2 = make_test(MyMod, :"test two")
      ids = MapSet.new([{MyMod, :"test one"}])
      filters = %{no_filters() | only_test_ids: ids}

      {to_run, to_skip} = TestFilter.filter(filters, [t1, t2])

      assert length(to_run) == 1
      assert hd(to_run).name == :"test one"
      assert to_skip == []
    end

    test "filters by test_name_pattern (case insensitive)" do
      t1 = make_test(MyMod, :"test Alpha thing")
      t2 = make_test(MyMod, :"test beta thing")
      filters = %{no_filters() | test_name_pattern: "ALPHA"}

      {to_run, to_skip} = TestFilter.filter(filters, [t1, t2])

      assert length(to_run) == 1
      assert hd(to_run).name == :"test Alpha thing"
      assert to_skip == []
    end

    test "filters by exclude tags" do
      t1 = make_test(MyMod, :"test one", %{slow: true})
      t2 = make_test(MyMod, :"test two", %{})
      filters = %{no_filters() | exclude: [slow: true]}

      {to_run, to_skip} = TestFilter.filter(filters, [t1, t2])

      assert length(to_run) == 1
      assert hd(to_run).name == :"test two"
      assert length(to_skip) == 1
      assert hd(to_skip).name == :"test one"
      assert hd(to_skip).state != nil
    end

    test "filters by include tags" do
      t1 = make_test(MyMod, :"test one", %{slow: true})
      t2 = make_test(MyMod, :"test two", %{})
      filters = %{no_filters() | include: [slow: true]}

      {to_run, _to_skip} = TestFilter.filter(filters, [t1, t2])

      assert Enum.any?(to_run, &(&1.name == :"test one"))
    end

    test "all tests filtered out" do
      t1 = make_test(MyMod, :"test one")
      ids = MapSet.new([{OtherMod, :"test nonexistent"}])
      filters = %{no_filters() | only_test_ids: ids}

      {to_run, to_skip} = TestFilter.filter(filters, [t1])

      assert to_run == []
      assert to_skip == []
    end

    test "combines test_ids and name_pattern filters" do
      t1 = make_test(MyMod, :"test alpha")
      t2 = make_test(MyMod, :"test beta")
      t3 = make_test(MyMod, :"test gamma")
      ids = MapSet.new([{MyMod, :"test alpha"}, {MyMod, :"test beta"}])
      filters = %{no_filters() | only_test_ids: ids, test_name_pattern: "beta"}

      {to_run, to_skip} = TestFilter.filter(filters, [t1, t2, t3])

      assert length(to_run) == 1
      assert hd(to_run).name == :"test beta"
      assert to_skip == []
    end

    test "preserves test order" do
      tests =
        for i <- 1..5 do
          make_test(MyMod, :"test #{i}")
        end

      {to_run, _} = TestFilter.filter(no_filters(), tests)

      names = Enum.map(to_run, & &1.name)
      assert names == [:"test 1", :"test 2", :"test 3", :"test 4", :"test 5"]
    end
  end

  describe "include_test?/2" do
    test "returns true when test_ids is nil" do
      test = make_test(MyMod, :"test one")
      assert TestFilter.include_test?(nil, test) == true
    end

    test "returns true when test is in the set" do
      test = make_test(MyMod, :"test one")
      ids = MapSet.new([{MyMod, :"test one"}])
      assert TestFilter.include_test?(ids, test) == true
    end

    test "returns false when test is not in the set" do
      test = make_test(MyMod, :"test one")
      ids = MapSet.new([{MyMod, :"test other"}])
      assert TestFilter.include_test?(ids, test) == false
    end

    test "returns false for empty MapSet" do
      test = make_test(MyMod, :"test one")
      assert TestFilter.include_test?(MapSet.new(), test) == false
    end
  end

  describe "match_test_name?/2" do
    test "returns true when pattern is nil" do
      test = make_test(MyMod, :"test anything")
      assert TestFilter.match_test_name?(nil, test) == true
    end

    test "returns true when pattern matches" do
      test = make_test(MyMod, :"test create user")
      assert TestFilter.match_test_name?("create", test) == true
    end

    test "returns false when pattern does not match" do
      test = make_test(MyMod, :"test create user")
      assert TestFilter.match_test_name?("delete", test) == false
    end

    test "matching is case insensitive" do
      test = make_test(MyMod, :"test Create User")
      assert TestFilter.match_test_name?("CREATE", test) == true
      assert TestFilter.match_test_name?("create user", test) == true
    end

    test "matches substring" do
      test = make_test(MyMod, :"test something important here")
      assert TestFilter.match_test_name?("important", test) == true
    end
  end
end
