defmodule Toast.UtilsTest do
  use ExUnit.Case, async: true

  import Toast.Utils

  describe "conditional_put/3" do
    test "skips nil values" do
      assert conditional_put(%{a: 1}, :b, nil) == %{a: 1}
    end

    test "inserts non-nil values" do
      assert conditional_put(%{a: 1}, :b, 2) == %{a: 1, b: 2}
    end

    test "inserts false as a value" do
      assert conditional_put(%{}, :flag, false) == %{flag: false}
    end
  end

  describe "conditional_put/4 with boolean" do
    test "skips when condition is false" do
      assert conditional_put(%{a: 1}, :b, 2, false) == %{a: 1}
    end

    test "inserts when condition is true" do
      assert conditional_put(%{a: 1}, :b, 2, true) == %{a: 1, b: 2}
    end
  end

  describe "conditional_put/4 with modifier" do
    test "skips nil values" do
      assert conditional_put(%{a: 1}, :b, nil, &String.upcase/1) == %{a: 1}
    end

    test "applies modifier to non-nil values" do
      assert conditional_put(%{}, :name, "hello", &String.upcase/1) == %{name: "HELLO"}
    end
  end
end
