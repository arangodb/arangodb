defmodule Toast.Process.ProcStatusTest do
  use ExUnit.Case, async: true

  alias Toast.Process.ProcStatus

  describe "classify/1" do
    test "returns :alive for a running process" do
      content = """
      Name:\thead
      State:\tR (running)
      CoreDumping:\t0
      """

      assert ProcStatus.classify(content) == :alive
    end

    test "returns :alive for a sleeping process" do
      content = """
      Name:\tarangod
      State:\tS (sleeping)
      CoreDumping:\t0
      """

      assert ProcStatus.classify(content) == :alive
    end

    test "returns :alive for uninterruptible sleep without coredump flag" do
      content = """
      State:\tD (disk sleep)
      CoreDumping:\t0
      """

      assert ProcStatus.classify(content) == :alive
    end

    test "returns {:crashing, :zombie} for a zombie process" do
      content = """
      Name:\tarangod
      State:\tZ (zombie)
      CoreDumping:\t0
      """

      assert ProcStatus.classify(content) == {:crashing, :zombie}
    end

    test "returns {:crashing, :core_dumping} when CoreDumping is 1" do
      content = """
      Name:\tarangod
      State:\tD (disk sleep)
      CoreDumping:\t1
      """

      assert ProcStatus.classify(content) == {:crashing, :core_dumping}
    end

    test "zombie takes precedence over core_dumping flag" do
      content = """
      State:\tZ (zombie)
      CoreDumping:\t1
      """

      assert ProcStatus.classify(content) == {:crashing, :zombie}
    end

    test "returns :alive when CoreDumping field is absent (older kernels)" do
      content = """
      Name:\tsome_proc
      State:\tR (running)
      """

      assert ProcStatus.classify(content) == :alive
    end
  end

  describe "probe/1" do
    test "returns :alive for the running test process" do
      os_pid = :os.getpid() |> List.to_integer()
      assert ProcStatus.probe(os_pid) == :alive
    end

    test "returns {:crashing, :proc_missing} for a nonexistent pid" do
      # 2^22 is above the default pid_max on virtually all Linux configurations,
      # so /proc/<pid> is guaranteed not to exist.
      assert ProcStatus.probe(4_194_304) == {:crashing, :proc_missing}
    end
  end
end
