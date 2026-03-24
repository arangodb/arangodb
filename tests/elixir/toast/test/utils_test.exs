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
