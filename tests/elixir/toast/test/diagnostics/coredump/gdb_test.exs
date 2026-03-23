defmodule Toast.Diagnostics.Coredump.GDBTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.Coredump.GDB

  @gdb_output """
  Program terminated with signal SIGSEGV, Segmentation fault.
  #0  0x00007f1234567890 in crashFunction (arg=0x1) at /path/to/file.cpp:42
  #1  0x00007f1234567891 in callerFunction () at /path/to/file.cpp:100
  #2  0x00007f1234567892 in __libc_start_main ()

  Thread 1 (LWP 12345):
  #0  0x00007f1234567890 in crashFunction (arg=0x1) at /path/to/file.cpp:42
  #1  0x00007f1234567891 in callerFunction () at /path/to/file.cpp:100
  #2  0x00007f1234567892 in arangodb::doSomething () at /path/to/arango.cpp:200
  #3  0x00007f1234567893 in __libc_start_main ()

  Thread 2 (Thread 0x7f12345 (LWP 12346)):
  #0  0x00007f1234567894 in __GI___poll (fds=0x1) at poll.c:29
  #1  0x00007f1234567895 in arangodb::waitForEvent () at /path/to/event.cpp:50
  """

  @gdb_multi_thread """
  Program terminated with signal SIGABRT, Aborted.
  Thread 1 (LWP 10001):
  #0  0x00007f0000000001 in raise (sig=6) at raise.c:51
  #1  0x00007f0000000002 in abort () at abort.c:79
  #2  0x00007f0000000003 in arangodb::handleAssert (msg=0x1) at /src/assert.cpp:30
  #3  0x00007f0000000004 in arangodb::query::execute () at /src/query.cpp:150
  #4  0x00007f0000000005 in __libc_start_main ()
  #5  0x00007f0000000006 in _start ()

  Thread 2 (Thread 0x7f123 (LWP 10002)):
  #0  0x00007f0000000010 in __GI___poll (fds=0x1) at poll.c:29
  #1  0x00007f0000000011 in arangodb::network::poll () at /src/network.cpp:42

  Thread 3 (Thread 0x7f456 (LWP 10003)):
  #0  0x00007f0000000020 in clone () at clone.S:78
  #1  0x00007f0000000021 in start_thread () at pthread_create.c:477
  #2  0x00007f0000000022 in arangodb::scheduler::run () at /src/scheduler.cpp:88
  """

  describe "executable/0" do
    test "returns gdb" do
      assert GDB.executable() == "gdb"
    end
  end

  describe "command/2" do
    test "returns correct argument list" do
      args = GDB.command("/usr/bin/arangod", "/tmp/core.12345")

      assert args == [
               "-batch",
               "-ex",
               "thread apply all bt full",
               "-ex",
               "quit",
               "/usr/bin/arangod",
               "/tmp/core.12345"
             ]
    end
  end

  describe "parse_output/1" do
    test "extracts signal info" do
      result = GDB.parse_output(@gdb_output)

      assert result.signal == "SIGSEGV"
    end

    # T11: exact thread count — pre-header crash thread (id=1) + Thread 1 (id=1) + Thread 2 (id=2)
    test "extracts threads" do
      result = GDB.parse_output(@gdb_output)

      assert length(result.threads) == 3
      thread_ids = Enum.map(result.threads, & &1.id)
      assert Enum.count(thread_ids, &(&1 == 1)) == 2
      assert Enum.count(thread_ids, &(&1 == 2)) == 1
    end

    test "extracts frames with file and line" do
      result = GDB.parse_output(@gdb_output)
      thread1 = Enum.find(result.threads, &(&1.id == 1))

      crash_frame = hd(thread1.frames)
      assert crash_frame.function == "crashFunction"
      assert crash_frame.file == "/path/to/file.cpp"
      assert crash_frame.line == 42
    end

    test "keeps all frames of crash thread" do
      result = GDB.parse_output(@gdb_output)
      thread1 = Enum.find(result.threads, &(&1.id == 1))

      # Crash thread keeps all frames including internal ones
      funcs = Enum.map(thread1.frames, & &1.function)
      assert "__libc_start_main" in funcs
    end

    test "filters internal frames from non-crash threads" do
      result = GDB.parse_output(@gdb_output)
      thread2 = Enum.find(result.threads, &(&1.id == 2))

      funcs = Enum.map(thread2.frames, & &1.function)
      refute Enum.any?(funcs, &String.starts_with?(&1, "__GI_"))
      assert "arangodb::waitForEvent" in funcs
    end

    test "identifies crash thread" do
      result = GDB.parse_output(@gdb_output)

      assert result.crash_thread == 1
    end

    test "parses multiple threads with various internal frames" do
      result = GDB.parse_output(@gdb_multi_thread)

      assert result.signal == "SIGABRT"
      assert length(result.threads) == 3

      # Non-crash thread 2: __GI___poll filtered
      thread2 = Enum.find(result.threads, &(&1.id == 2))
      funcs2 = Enum.map(thread2.frames, & &1.function)
      refute "__GI___poll" in funcs2
      assert "arangodb::network::poll" in funcs2

      # Non-crash thread 3: clone, start_thread filtered
      thread3 = Enum.find(result.threads, &(&1.id == 3))
      funcs3 = Enum.map(thread3.frames, & &1.function)
      refute "clone" in funcs3
      refute "start_thread" in funcs3
      assert "arangodb::scheduler::run" in funcs3
    end

    test "handles frames without file info" do
      output = """
      Thread 1 (LWP 100):
      #0  0x00007f1234 in someFunc ()
      #1  0x00007f5678 in otherFunc (x=42) at /src/file.cpp:10
      """

      result = GDB.parse_output(output)
      thread = hd(result.threads)

      assert length(thread.frames) == 2
      [f0, f1] = thread.frames
      assert f0.function == "someFunc"
      assert f0.file == nil
      assert f0.line == nil
      assert f1.file == "/src/file.cpp"
      assert f1.line == 10
    end

    test "extracts thread name before parentheses" do
      output = """
      Thread 1 "main" (Thread 0x7f123 (LWP 100)):
      #0  0x00007f1234 in crash ()
      Thread 2 "worker-0" (Thread 0x7f456 (LWP 101)):
      #0  0x00007f5678 in work ()
      """

      result = GDB.parse_output(output)
      thread1 = Enum.find(result.threads, &(&1.id == 1))
      thread2 = Enum.find(result.threads, &(&1.id == 2))
      assert thread1.name == "main"
      assert thread2.name == "worker-0"
    end

    test "extracts thread name inside parentheses" do
      output = """
      Thread 1 (Thread 0x7f123 (LWP 100) "arangod"):
      #0  0x00007f1234 in crash ()
      """

      result = GDB.parse_output(output)
      thread = hd(result.threads)
      assert thread.name == "arangod"
    end

    test "thread name is nil when not present" do
      output = """
      Thread 1 (Thread 0x7f123 (LWP 100)):
      #0  0x00007f1234 in crash ()
      """

      result = GDB.parse_output(output)
      thread = hd(result.threads)
      assert thread.name == nil
    end

    test "pre-header implicit thread has nil name" do
      output = """
      Program terminated with signal SIGSEGV, Segmentation fault.
      #0  0x00007f1234 in crash () at file.cpp:1
      """

      result = GDB.parse_output(output)
      thread = hd(result.threads)
      assert thread.name == nil
    end

    test "handles empty output" do
      result = GDB.parse_output("")

      assert result.signal == nil
      assert result.threads == []
      assert result.crash_thread == nil
    end

    test "handles malformed output gracefully" do
      result = GDB.parse_output("some random text\nwith no structure\n")

      assert result.signal == nil
      assert result.threads == []
      assert result.crash_thread == nil
    end

    test "extracts faulting address from signal line" do
      output = """
      Program terminated with signal SIGSEGV, Segmentation fault, address 0xdeadbeef.
      Thread 1 (LWP 100):
      #0  0x00007f1234 in crash ()
      """

      result = GDB.parse_output(output)
      assert result.faulting_address == "0xdeadbeef"
    end

    # T8: secondary si_addr parsing when signal line has no address
    test "extracts faulting address from si_addr line when signal line lacks address" do
      output = """
      Program terminated with signal SIGSEGV, Segmentation fault.
      si_addr = 0xBAADF00D
      Thread 1 (LWP 100):
      #0  0x00007f1234 in crash ()
      """

      result = GDB.parse_output(output)
      assert result.signal == "SIGSEGV"
      assert result.faulting_address == "0xBAADF00D"
    end

    test "primary address takes precedence over si_addr" do
      output = """
      Program terminated with signal SIGSEGV, Segmentation fault, address 0xdeadbeef.
      si_addr = 0xBAADF00D
      Thread 1 (LWP 100):
      #0  0x00007f1234 in crash ()
      """

      result = GDB.parse_output(output)
      # Primary address from the signal line is found first; si_addr is only
      # used when faulting_address is still nil
      assert result.faulting_address == "0xdeadbeef"
    end
  end
end
