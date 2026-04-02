defmodule Toast.EnvTest do
  use ExUnit.Case, async: false

  # Toast.Env.load/1 writes to Application env, so tests must be serial
  # and clean up after themselves.

  setup do
    # Snapshot all :toast app env keys so we can restore them
    original = Application.get_all_env(:toast)
    on_exit(fn -> restore_app_env(original) end)

    # Clear any TOAST_RR env var
    prev_rr = System.get_env("TOAST_RR")
    System.delete_env("TOAST_RR")
    on_exit(fn -> if prev_rr, do: System.put_env("TOAST_RR", prev_rr) end)

    # Create a fake rr executable so validation passes
    tmp_dir = Path.join(System.tmp_dir!(), "toast_env_test_#{System.unique_integer([:positive])}")
    File.mkdir_p!(tmp_dir)
    on_exit(fn -> File.rm_rf!(tmp_dir) end)
    Toast.PathTestHelpers.create_fake_executable("rr", tmp_dir)

    :ok
  end

  defp restore_app_env(original) do
    # Delete all current keys
    for {key, _} <- Application.get_all_env(:toast) do
      Application.delete_env(:toast, key)
    end

    # Restore original
    for {key, val} <- original do
      Application.put_env(:toast, key, val)
    end
  end

  describe "rr option parsing" do
    test "rr nil when not specified" do
      Toast.Env.load(local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :rr) == nil
    end

    test "rr 'default' resolves to :single for single_server mode" do
      Toast.Env.load(rr: "default", local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :rr) == MapSet.new([:single])
    end

    test "rr 'default' resolves to dbserver,coordinator for cluster mode" do
      Toast.Env.load(
        rr: "default",
        deployment_mode: :cluster,
        local_config_dir: "/nonexistent"
      )

      assert Application.get_env(:toast, :rr) == MapSet.new([:dbserver, :coordinator])
    end

    test "rr 'all' from opts" do
      Toast.Env.load(rr: "all", local_config_dir: "/nonexistent")

      assert Application.get_env(:toast, :rr) ==
               MapSet.new([:single, :agent, :dbserver, :coordinator])
    end

    test "rr specific roles from opts" do
      Toast.Env.load(rr: "dbserver,coordinator", local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :rr) == MapSet.new([:dbserver, :coordinator])
    end

    test "rr single role from opts" do
      Toast.Env.load(rr: "dbserver", local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :rr) == MapSet.new([:dbserver])
    end

    test "rr from TOAST_RR env var" do
      System.put_env("TOAST_RR", "dbserver,coordinator")
      Toast.Env.load(local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :rr) == MapSet.new([:dbserver, :coordinator])
    end

    test "rr opts override env var" do
      System.put_env("TOAST_RR", "all")
      Toast.Env.load(rr: "dbserver", local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :rr) == MapSet.new([:dbserver])
    end

    test "rr invalid role raises" do
      assert_raise ArgumentError, ~r/invalid rr role/, fn ->
        Toast.Env.load(rr: "bogus", local_config_dir: "/nonexistent")
      end
    end

    test "rr_path is set when rr is active" do
      Toast.Env.load(rr: "default", local_config_dir: "/nonexistent")
      rr_path = Application.get_env(:toast, :rr_path)
      assert is_binary(rr_path)
      assert String.ends_with?(rr_path, "/rr")
    end

    test "rr_path is nil when rr is not active" do
      Toast.Env.load(local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :rr_path) == nil
    end

    test "rr raises when rr executable not found" do
      # Override PATH to exclude the fake rr
      prev_path = System.get_env("PATH")
      System.put_env("PATH", "/usr/bin:/bin")

      try do
        assert_raise ArgumentError, ~r/rr.*not found in PATH/, fn ->
          Toast.Env.load(rr: "default", local_config_dir: "/nonexistent")
        end
      after
        System.put_env("PATH", prev_path)
      end
    end
  end

  describe "timeout factor with rr" do
    test "auto-sets timeout_factor to 10 when rr is active" do
      Toast.Env.load(rr: "default", local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :timeout_factor) == 10
    end

    test "explicit timeout_factor overrides rr auto-factor" do
      Toast.Env.load(rr: "default", timeout_factor: 5, local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :timeout_factor) == 5
    end

    test "rr factor wins over sanitizer default" do
      # Sanitizer default is 3, rr default is 10
      Toast.Env.load(
        rr: "default",
        sanitizer_override: "alubsan",
        local_config_dir: "/nonexistent"
      )

      assert Application.get_env(:toast, :timeout_factor) == 10
    end

    test "no rr, no sanitizer: timeout_factor defaults to 1" do
      Toast.Env.load(local_config_dir: "/nonexistent")
      assert Application.get_env(:toast, :timeout_factor) == 1
    end
  end
end
