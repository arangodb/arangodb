defmodule Toast.DeploymentTest do
  use ExUnit.Case, async: true

  describe "start/2" do
    test "unsupported mode returns error" do
      assert {:error, {:unsupported_mode, :bogus}} = Toast.Deployment.start(:bogus)
    end
  end
end
