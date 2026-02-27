defmodule Toast.DeploymentTest do
  use ExUnit.Case, async: true

  describe "start/2" do
    test "unsupported mode returns error" do
      assert {:error, {:unsupported_mode, :bogus}} = Toast.Deployment.start(:bogus)
    end
  end

  describe "struct" do
    test "enforces required keys" do
      assert_raise ArgumentError, ~r/the following keys must also be given/, fn ->
        struct!(Toast.Deployment, %{})
      end
    end
  end
end
