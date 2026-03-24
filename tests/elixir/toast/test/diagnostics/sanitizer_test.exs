defmodule Toast.Diagnostics.SanitizerTest do
  use ExUnit.Case, async: false

  alias Toast.Diagnostics.Sanitizer

  @sanitizer_vars ["ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS", "TSAN_OPTIONS"]

  describe "detect/1" do
    setup do
      saved = Map.new(@sanitizer_vars, fn var -> {var, System.get_env(var)} end)
      for var <- @sanitizer_vars, do: System.delete_env(var)

      on_exit(fn ->
        for {var, val} <- saved do
          if val, do: System.put_env(var, val), else: System.delete_env(var)
        end
      end)

      :ok
    end

    test "returns empty set when no sanitizer env vars" do
      assert MapSet.size(Sanitizer.detect()) == 0
    end

    test "detects ASAN_OPTIONS" do
      System.put_env("ASAN_OPTIONS", "halt_on_error=0")

      result = Sanitizer.detect()
      assert MapSet.member?(result, "ASAN_OPTIONS")
    end

    test "detects TSAN_OPTIONS" do
      System.put_env("TSAN_OPTIONS", "halt_on_error=0")

      result = Sanitizer.detect()
      assert MapSet.member?(result, "TSAN_OPTIONS")
    end

    test "detects multiple sanitizers" do
      System.put_env("ASAN_OPTIONS", "halt_on_error=0")
      System.put_env("LSAN_OPTIONS", "halt_on_error=0")

      result = Sanitizer.detect()
      assert MapSet.member?(result, "ASAN_OPTIONS")
      assert MapSet.member?(result, "LSAN_OPTIONS")
    end

    test "explicit :tsan forces TSAN_OPTIONS active" do
      result = Sanitizer.detect(:tsan)
      assert MapSet.equal?(result, MapSet.new(["TSAN_OPTIONS"]))
    end

    test "explicit :alubsan forces ASAN/LSAN/UBSAN active" do
      result = Sanitizer.detect(:alubsan)
      assert MapSet.equal?(result, MapSet.new(["ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS"]))
    end

    test "explicit mode ignores env vars" do
      System.put_env("TSAN_OPTIONS", "halt_on_error=0")

      result = Sanitizer.detect(:alubsan)
      refute MapSet.member?(result, "TSAN_OPTIONS")
    end

    test "raises on invalid explicit sanitizer" do
      assert_raise ArgumentError, ~r/invalid sanitizer/, fn ->
        Sanitizer.detect(:invalid)
      end
    end
  end

  describe "detect_from_build_dir/1" do
    test "returns nil for nil" do
      assert Sanitizer.detect_from_build_dir(nil) == nil
    end

    test "returns nil for regular build dir" do
      assert Sanitizer.detect_from_build_dir("/home/user/arangodb/build-clang") == nil
    end

    test "detects alubsan from asan in path" do
      assert Sanitizer.detect_from_build_dir("/home/user/arangodb/build-clang-asan-debug") ==
               :alubsan
    end

    test "detects tsan from tsan in path" do
      assert Sanitizer.detect_from_build_dir("/home/user/arangodb/build-clang-tsan") == :tsan
    end

    test "is case-insensitive" do
      assert Sanitizer.detect_from_build_dir("/home/user/arangodb/build-ASAN") == :alubsan
    end

    test "tsan takes precedence when both present" do
      assert Sanitizer.detect_from_build_dir("/home/user/arangodb/build-tsan-asan") == :tsan
    end
  end

  describe "build_env/4" do
    setup do
      saved = Map.new(@sanitizer_vars, fn var -> {var, System.get_env(var)} end)
      for var <- @sanitizer_vars, do: System.delete_env(var)

      on_exit(fn ->
        for {var, val} <- saved do
          if val, do: System.put_env(var, val), else: System.delete_env(var)
        end
      end)

      :ok
    end

    test "returns empty list for empty set" do
      assert Sanitizer.build_env(MapSet.new(), "/tmp/log", "/repo") == []
    end

    test "generates ASAN_OPTIONS with log_path" do
      active = MapSet.new(["ASAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", "/repo")

      assert [{"ASAN_OPTIONS", value}] = env
      assert String.contains?(value, "log_path=/tmp/server1/alubsan.log")
      assert String.contains?(value, "log_exe_name=true")
    end

    test "generates TSAN_OPTIONS with tsan log path" do
      active = MapSet.new(["TSAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", "/repo")

      assert [{"TSAN_OPTIONS", value}] = env
      assert String.contains?(value, "log_path=/tmp/server1/tsan.log")
      assert String.contains?(value, "log_exe_name=true")
    end

    test "preserves existing env var options" do
      System.put_env("ASAN_OPTIONS", "halt_on_error=0:detect_leaks=1")

      active = MapSet.new(["ASAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", "/repo")

      assert [{"ASAN_OPTIONS", value}] = env
      assert String.contains?(value, "halt_on_error=0")
      assert String.contains?(value, "detect_leaks=1")
      assert String.contains?(value, "log_path=")
    end

    test "includes suppression file when present" do
      tmp_dir =
        Path.join(System.tmp_dir!(), "toast_san_supp_#{System.unique_integer([:positive])}")

      File.mkdir_p!(tmp_dir)
      supp_file = Path.join(tmp_dir, "lsan_arangodb_suppressions.txt")
      File.write!(supp_file, "leak:some_function\n")

      on_exit(fn -> File.rm_rf!(tmp_dir) end)

      active = MapSet.new(["LSAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", tmp_dir)

      assert [{"LSAN_OPTIONS", value}] = env
      assert String.contains?(value, "suppressions=")
      assert String.contains?(value, "lsan_arangodb_suppressions.txt")
    end

    test "explicit mode applies default ASAN options" do
      active = MapSet.new(["ASAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", "/repo", :alubsan)

      assert [{"ASAN_OPTIONS", value}] = env
      assert String.contains?(value, "halt_on_error=0")
      assert String.contains?(value, "detect_leaks=1")
    end

    test "explicit mode applies default TSAN options" do
      active = MapSet.new(["TSAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", "/repo", :tsan)

      assert [{"TSAN_OPTIONS", value}] = env
      assert String.contains?(value, "halt_on_error=0")
      assert String.contains?(value, "history_size=7")
    end

    test "explicit mode applies default UBSAN options" do
      active = MapSet.new(["UBSAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", "/repo", :alubsan)

      assert [{"UBSAN_OPTIONS", value}] = env
      assert String.contains?(value, "halt_on_error=0")
      assert String.contains?(value, "print_stacktrace=1")
    end

    test "env vars override explicit defaults" do
      System.put_env("TSAN_OPTIONS", "halt_on_error=1:history_size=4")

      active = MapSet.new(["TSAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", "/repo", :tsan)

      assert [{"TSAN_OPTIONS", value}] = env
      assert String.contains?(value, "halt_on_error=1")
      assert String.contains?(value, "history_size=4")
    end

    test "auto-detect mode does not apply defaults" do
      active = MapSet.new(["ASAN_OPTIONS"])
      env = Sanitizer.build_env(active, "/tmp/server1", "/repo")

      assert [{"ASAN_OPTIONS", value}] = env
      refute String.contains?(value, "halt_on_error=")
      refute String.contains?(value, "detect_leaks=")
    end
  end
end
