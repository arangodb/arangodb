defmodule Toast.DeploymentTest do
  use ExUnit.Case, async: true

  describe "start/2" do
    test "unsupported mode returns error" do
      assert {:error, {:unsupported_mode, :bogus}} = Toast.Deployment.start(:bogus)
    end
  end

  describe "struct" do
    test "contains immutable fields" do
      fields = Toast.Deployment.__struct__() |> Map.keys() |> MapSet.new()

      assert :id in fields
      assert :mode in fields
      assert :config in fields
      assert :controller in fields
      assert :endpoint in fields
      assert :work_dir in fields
    end

    test "does not contain mutable state fields" do
      fields = Toast.Deployment.__struct__() |> Map.keys() |> MapSet.new()

      refute :servers in fields
      refute :crash_monitor in fields
    end

    test "enforces required keys" do
      assert_raise ArgumentError, ~r/the following keys must also be given/, fn ->
        struct!(Toast.Deployment, %{})
      end
    end
  end
end
