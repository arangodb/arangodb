defmodule Toast.DeploymentTest do
  use ExUnit.Case, async: true

  describe "struct" do
    test "enforces required keys" do
      assert_raise ArgumentError, ~r/the following keys must also be given/, fn ->
        struct!(Toast.Deployment, %{})
      end
    end
  end
end
