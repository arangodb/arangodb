defmodule Toast.Diagnostics.Coredump.LLDBTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.Coredump.LLDB

  @lldb_output """
  * thread #1, stop reason = signal SIGSEGV
    * frame #0: 0x00007f1234567890 arangod`crashFunction(arg=0x1) at file.cpp:42
      frame #1: 0x00007f1234567891 arangod`callerFunction() at file.cpp:100
      frame #2: 0x00007f1234567892 arangod`arangodb::doSomething() at arango.cpp:200
      frame #3: 0x00007f1234567893 libpthread.so.0`start_thread
    thread #2
      frame #0: 0x00007f1234567894 libc.so.6`__GI___poll at poll.c:29
      frame #1: 0x00007f1234567895 arangod`arangodb::waitForEvent() at event.cpp:50
  """

  @lldb_multi_thread """
  * thread #1, stop reason = signal SIGABRT
    * frame #0: 0x00007f0000000001 libc.so.6`raise(sig=6) at raise.c:51
      frame #1: 0x00007f0000000002 libc.so.6`abort() at abort.c:79
      frame #2: 0x00007f0000000003 arangod`arangodb::handleAssert(msg=0x1) at assert.cpp:30
      frame #3: 0x00007f0000000004 arangod`arangodb::query::execute() at query.cpp:150
      frame #4: 0x00007f0000000005 libc.so.6`__libc_start_main
      frame #5: 0x00007f0000000006 arangod`_start
    thread #2
      frame #0: 0x00007f0000000010 libc.so.6`__GI___poll at poll.c:29
      frame #1: 0x00007f0000000011 arangod`arangodb::network::poll() at network.cpp:42
    thread #3
      frame #0: 0x00007f0000000020 libc.so.6`clone at clone.S:78
      frame #1: 0x00007f0000000021 libpthread.so.0`start_thread at pthread_create.c:477
      frame #2: 0x00007f0000000022 arangod`arangodb::scheduler::run() at scheduler.cpp:88
  """

  # Simulates combined output of `thread list` + `thread backtrace all`
  @lldb_with_thread_list """
  (lldb) thread list
  Process 12345 stopped
  * thread #1: tid = 328296, 0x00007f1234 libc.so.6`raise + 8, stop reason = signal SIGABRT
    thread #2: tid = 328297, 0x00007f5678 libc.so.6`poll + 45
    thread #3: tid = 328298, 0x00007f9abc libc.so.6`epoll_wait + 22
  (lldb) thread backtrace all
  * thread #1, stop reason = signal SIGABRT
    * frame #0: 0x00007f1234 libc.so.6`raise(sig=6) at raise.c:51
      frame #1: 0x00007f5678 arangod`arangodb::handleAssert() at assert.cpp:30
    thread #2
      frame #0: 0x00007f9abc libc.so.6`__GI___poll at poll.c:29
      frame #1: 0x00007fdef0 arangod`arangodb::network::poll() at network.cpp:42
    thread #3
      frame #0: 0x00007f1111 libc.so.6`epoll_wait at epoll.c:30
      frame #1: 0x00007f2222 arangod`arangodb::scheduler::run() at scheduler.cpp:88
  """

  describe "executable/0" do
    test "returns lldb" do
      assert LLDB.executable() == "lldb"
    end
  end

  describe "command/2" do
    test "returns correct argument list with thread list" do
      args = LLDB.command("/usr/bin/arangod", "/tmp/core.12345")

      assert args == [
               "-c",
               "/tmp/core.12345",
               "-o",
               "thread list",
               "-o",
               "thread backtrace all",
               "-o",
               "quit",
               "--",
               "/usr/bin/arangod"
             ]
    end
  end

  describe "parse_output/1" do
    test "extracts signal from stop reason" do
      result = LLDB.parse_output(@lldb_output)

      assert result.signal == "SIGSEGV"
    end

    test "identifies crash thread from * marker" do
      result = LLDB.parse_output(@lldb_output)

      assert result.crash_thread == 1
    end

    test "extracts threads" do
      result = LLDB.parse_output(@lldb_output)

      assert length(result.threads) == 2
      thread_ids = Enum.map(result.threads, & &1.id) |> MapSet.new()
      assert MapSet.member?(thread_ids, 1)
      assert MapSet.member?(thread_ids, 2)
    end

    test "extracts frames with file and line" do
      result = LLDB.parse_output(@lldb_output)
      thread1 = Enum.find(result.threads, &(&1.id == 1))

      crash_frame = hd(thread1.frames)
      assert crash_frame.function == "crashFunction(arg=0x1)"
      assert crash_frame.file == "file.cpp"
      assert crash_frame.line == 42
    end

    test "keeps all frames of crash thread" do
      result = LLDB.parse_output(@lldb_output)
      thread1 = Enum.find(result.threads, &(&1.id == 1))

      # Crash thread keeps internal frames
      funcs = Enum.map(thread1.frames, & &1.function)
      assert "start_thread" in funcs
    end

    test "filters internal frames from non-crash threads" do
      result = LLDB.parse_output(@lldb_output)
      thread2 = Enum.find(result.threads, &(&1.id == 2))

      funcs = Enum.map(thread2.frames, & &1.function)
      refute Enum.any?(funcs, &String.starts_with?(&1, "__GI_"))
      assert "arangodb::waitForEvent()" in funcs
    end

    test "parses SIGABRT with multiple threads" do
      result = LLDB.parse_output(@lldb_multi_thread)

      assert result.signal == "SIGABRT"
      assert result.crash_thread == 1
      assert length(result.threads) == 3
    end

    test "filters internal frames from non-crash threads in multi-thread output" do
      result = LLDB.parse_output(@lldb_multi_thread)

      # Non-crash thread 2: __GI___poll filtered
      thread2 = Enum.find(result.threads, &(&1.id == 2))
      funcs2 = Enum.map(thread2.frames, & &1.function)
      refute Enum.any?(funcs2, &String.starts_with?(&1, "__GI_"))
      assert "arangodb::network::poll()" in funcs2

      # Non-crash thread 3: clone, start_thread filtered
      thread3 = Enum.find(result.threads, &(&1.id == 3))
      funcs3 = Enum.map(thread3.frames, & &1.function)
      refute "clone" in funcs3
      refute "start_thread" in funcs3
      assert "arangodb::scheduler::run()" in funcs3
    end

    test "handles frames without file info" do
      output = """
      * thread #1, stop reason = signal SIGSEGV
        * frame #0: 0x00007f1234 arangod`someFunc
          frame #1: 0x00007f5678 arangod`otherFunc(x=42) at file.cpp:10
      """

      result = LLDB.parse_output(output)
      thread = hd(result.threads)

      assert length(thread.frames) == 2
      [f0, f1] = thread.frames
      assert f0.function == "someFunc"
      assert f0.file == nil
      assert f0.line == nil
      assert f1.file == "file.cpp"
      assert f1.line == 10
    end

    test "handles empty output" do
      result = LLDB.parse_output("")

      assert result.signal == nil
      assert result.threads == []
      assert result.crash_thread == nil
    end

    test "handles malformed output gracefully" do
      result = LLDB.parse_output("some random text\nwith no structure\n")

      assert result.signal == nil
      assert result.threads == []
      assert result.crash_thread == nil
    end

    test "faulting_address is nil (LLDB does not extract it)" do
      result = LLDB.parse_output(@lldb_output)
      assert result.faulting_address == nil
    end

    # --- OS thread ID (tid) ---

    test "os_id is nil when no thread list and no tid in backtrace headers" do
      result = LLDB.parse_output(@lldb_output)

      Enum.each(result.threads, fn thread ->
        assert thread.os_id == nil
      end)
    end

    test "extracts os_id from thread list output" do
      result = LLDB.parse_output(@lldb_with_thread_list)

      thread1 = Enum.find(result.threads, &(&1.id == 1))
      thread2 = Enum.find(result.threads, &(&1.id == 2))
      thread3 = Enum.find(result.threads, &(&1.id == 3))

      assert thread1.os_id == "328296"
      assert thread2.os_id == "328297"
      assert thread3.os_id == "328298"
    end

    test "extracts os_id from hex tid in backtrace header" do
      output = """
      * thread #1, tid = 0x1a2b, stop reason = signal SIGSEGV
        * frame #0: 0x00007f1234 arangod`crash() at file.cpp:1
        thread #2, tid = 0x1a2c
          frame #0: 0x00007f5678 arangod`work() at work.cpp:10
      """

      result = LLDB.parse_output(output)
      thread1 = Enum.find(result.threads, &(&1.id == 1))
      thread2 = Enum.find(result.threads, &(&1.id == 2))

      assert thread1.os_id == "0x1a2b"
      assert thread2.os_id == "0x1a2c"
    end

    test "extracts decimal tid from thread list" do
      output = """
      * thread #1: tid = 328296, stop reason = signal SIGSEGV
      (lldb) thread backtrace all
      * thread #1, stop reason = signal SIGSEGV
        * frame #0: 0x00007f1234 arangod`crash() at file.cpp:1
      """

      result = LLDB.parse_output(output)
      thread = hd(result.threads)
      assert thread.os_id == "328296"
    end

    test "thread list provides os_id when backtrace headers lack it" do
      # thread list has tids, backtrace headers do not
      result = LLDB.parse_output(@lldb_with_thread_list)

      # All threads should have os_id from thread list
      Enum.each(result.threads, fn thread ->
        assert thread.os_id != nil, "Thread #{thread.id} should have os_id from thread list"
      end)
    end
  end
end
