defmodule Toast.PathTestHelpers do
  @moduledoc false

  @doc """
  Create a fake executable on PATH for testing.

  Writes a stub script to a temp directory and prepends it to PATH.
  Returns the path to the fake executable. Registers on_exit callbacks
  to restore PATH and clean up the temp directory.

  Must be called within an ExUnit test or setup block (uses `ExUnit.Callbacks.on_exit/1`).
  """
  @spec create_fake_executable(String.t(), Path.t()) :: Path.t()
  def create_fake_executable(name, tmp_dir) do
    bin_dir = Path.join(tmp_dir, "fake_bin")
    File.mkdir_p!(bin_dir)
    executable = Path.join(bin_dir, name)
    File.write!(executable, "#!/bin/sh\n")
    File.chmod!(executable, 0o755)

    prev_path = System.get_env("PATH")
    System.put_env("PATH", "#{bin_dir}:#{prev_path}")

    ExUnit.Callbacks.on_exit(fn -> System.put_env("PATH", prev_path) end)

    executable
  end
end
