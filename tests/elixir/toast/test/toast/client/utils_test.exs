defmodule Toast.Client.UtilsTest do
  use ExUnit.Case, async: true

  alias Toast.Client.Utils

  describe "translate_opts/2" do
    test "translates known keys" do
      key_map = %{wait_for_sync: :waitForSync, return_new: :returnNew}
      opts = [wait_for_sync: true, return_new: false]

      assert [{:waitForSync, true}, {:returnNew, false}] = Utils.translate_opts(opts, key_map)
    end

    test "filters out unknown keys" do
      key_map = %{wait_for_sync: :waitForSync}
      opts = [wait_for_sync: true, unknown_key: "ignored"]

      assert [{:waitForSync, true}] = Utils.translate_opts(opts, key_map)
    end

    test "returns empty list for empty opts" do
      assert [] = Utils.translate_opts([], %{a: :b})
    end

    test "returns empty list when no keys match" do
      assert [] = Utils.translate_opts([foo: 1], %{bar: :baz})
    end
  end
end
