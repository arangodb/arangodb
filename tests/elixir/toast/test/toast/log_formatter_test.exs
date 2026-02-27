defmodule Toast.LogFormatterTest do
  use ExUnit.Case, async: true

  alias Toast.LogFormatter

  describe "format/4 (Elixir Logger console callback)" do
    test "formats message with module from MFA metadata" do
      metadata = [mfa: {Toast.SomeModule, :run, 2}]

      result =
        LogFormatter.format(:info, "hello world", {{2024, 1, 15}, {10, 30, 0, 0}}, metadata)

      output = IO.iodata_to_binary(result)
      assert output == "[info] [Toast.SomeModule] hello world\n"
    end

    test "formats message without MFA metadata" do
      result = LogFormatter.format(:warning, "no module", {{2024, 1, 15}, {10, 30, 0, 0}}, [])

      output = IO.iodata_to_binary(result)
      assert output == "[warning] no module\n"
    end

    test "strips Elixir. prefix from module name" do
      metadata = [mfa: {Elixir.Toast.Deployment, :start, 0}]
      result = LogFormatter.format(:debug, "deploying", {{2024, 1, 15}, {10, 30, 0, 0}}, metadata)

      output = IO.iodata_to_binary(result)
      assert output =~ "[Toast.Deployment]"
      refute output =~ "Elixir."
    end

    test "handles Erlang module (no Elixir. prefix)" do
      metadata = [mfa: {:gen_server, :init_it, 6}]
      result = LogFormatter.format(:info, "gen_server", {{2024, 1, 15}, {10, 30, 0, 0}}, metadata)

      output = IO.iodata_to_binary(result)
      assert output =~ "[gen_server]"
    end

    test "includes level in output" do
      metadata = [mfa: {Toast.Runner, :run, 1}]

      for level <- [:debug, :info, :warning, :error] do
        output = LogFormatter.format(level, "msg", {{2024, 1, 15}, {10, 30, 0, 0}}, metadata)
        assert IO.iodata_to_binary(output) =~ "[#{level}]"
      end
    end

    test "handles nil MFA gracefully" do
      result = LogFormatter.format(:info, "test", {{2024, 1, 15}, {10, 30, 0, 0}}, mfa: nil)

      output = IO.iodata_to_binary(result)
      assert output == "[info] test\n"
    end

    test "handles iolist message" do
      result =
        LogFormatter.format(:info, ["hello", " ", "world"], {{2024, 1, 15}, {10, 30, 0, 0}}, [])

      output = IO.iodata_to_binary(result)
      assert output == "[info] hello world\n"
    end

    test "rescue clause handles malformed input gracefully" do
      # Pass something that will cause format_module or IO.iodata_to_binary to fail
      # A non-atom level will raise in Atom.to_string, but format has a rescue
      # Actually the rescue catches any error during formatting
      result =
        LogFormatter.format(:info, "test", {{2024, 1, 15}, {10, 30, 0, 0}},
          mfa: {"not_an_atom", :f, 1}
        )

      output = IO.iodata_to_binary(result)
      # Rescue path produces: [level] message\n
      assert output =~ "[info]"
      assert output =~ "test"
      assert String.ends_with?(output, "\n")
    end
  end

  describe "format/2 (Erlang :logger handler callback)" do
    test "formats string message with timestamp and module" do
      now_usec = DateTime.to_unix(~U[2024-06-15 10:30:45.123456Z], :microsecond)

      log_event = %{
        level: :info,
        msg: {:string, "server started"},
        meta: %{
          time: now_usec,
          mfa: {Toast.Server, :start, 1}
        }
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "2024-06-15 10:30:45.123"
      assert output =~ "[info]"
      assert output =~ "[Toast.Server]"
      assert output =~ "server started"
      assert String.ends_with?(output, "\n")
    end

    test "formats report message" do
      now_usec = DateTime.to_unix(~U[2024-01-01 00:00:00Z], :microsecond)

      log_event = %{
        level: :warning,
        msg: {:report, %{reason: :timeout, module: SomeWorker}},
        meta: %{time: now_usec}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "[warning]"
      assert output =~ "timeout"
      assert output =~ "SomeWorker"
    end

    test "formats io_lib format+args message" do
      now_usec = DateTime.to_unix(~U[2024-01-01 12:00:00Z], :microsecond)

      log_event = %{
        level: :error,
        msg: {~c"Connection to ~s failed: ~p", [~c"localhost", :econnrefused]},
        meta: %{time: now_usec}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "[error]"
      assert output =~ "Connection to localhost failed"
      assert output =~ "econnrefused"
    end

    test "handles missing MFA in meta" do
      now_usec = DateTime.to_unix(~U[2024-01-01 00:00:00Z], :microsecond)

      log_event = %{
        level: :info,
        msg: {:string, "no module info"},
        meta: %{time: now_usec}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "[info]"
      assert output =~ "no module info"
      # No nested brackets (would indicate a spurious module tag)
      refute output =~ "[["
    end

    test "handles missing timestamp in meta" do
      log_event = %{
        level: :debug,
        msg: {:string, "no time"},
        meta: %{}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "[debug]"
      assert output =~ "no time"
    end

    test "rescue clause produces error marker on malformed event" do
      # Passing a non-map meta will cause pattern match to fail in format_timestamp
      # but the rescue clause should catch it
      log_event = %{
        level: :info,
        msg: {:string, "test"},
        meta: %{time: "not_a_number", mfa: {"not_an_atom", :f, 1}}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)
      assert output == "[log format error]\n"
    end

    test "config parameter is ignored" do
      now_usec = DateTime.to_unix(~U[2024-01-01 00:00:00Z], :microsecond)
      log_event = %{level: :info, msg: {:string, "msg"}, meta: %{time: now_usec}}

      result1 = LogFormatter.format(log_event, %{})
      result2 = LogFormatter.format(log_event, %{some: :config})
      result3 = LogFormatter.format(log_event, nil)

      assert IO.iodata_to_binary(result1) == IO.iodata_to_binary(result2)
      assert IO.iodata_to_binary(result1) == IO.iodata_to_binary(result3)
    end
  end

  describe "timestamp formatting" do
    test "formats with millisecond precision" do
      # 2024-06-15 10:30:45.678 UTC
      now_usec = DateTime.to_unix(~U[2024-06-15 10:30:45.678000Z], :microsecond)

      log_event = %{
        level: :info,
        msg: {:string, "ts test"},
        meta: %{time: now_usec}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "2024-06-15 10:30:45.678"
    end

    test "formats zero milliseconds correctly" do
      now_usec = DateTime.to_unix(~U[2024-01-01 00:00:00.000000Z], :microsecond)

      log_event = %{
        level: :info,
        msg: {:string, "midnight"},
        meta: %{time: now_usec}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "2024-01-01 00:00:00.000"
    end

    test "truncates microseconds to milliseconds" do
      # .123456 should become .123
      now_usec = DateTime.to_unix(~U[2024-06-15 10:30:45.123456Z], :microsecond)

      log_event = %{
        level: :info,
        msg: {:string, "precision test"},
        meta: %{time: now_usec}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "10:30:45.123"
      refute output =~ "123456"
    end

    test "handles epoch zero" do
      log_event = %{
        level: :info,
        msg: {:string, "epoch"},
        meta: %{time: 0}
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "1970-01-01 00:00:00.000"
    end
  end

  describe "module name formatting" do
    test "strips Elixir prefix from Elixir module" do
      log_event = %{
        level: :info,
        msg: {:string, "test"},
        meta: %{
          time: 0,
          mfa: {Toast.Diagnostics.Matcher, :match, 4}
        }
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "[Toast.Diagnostics.Matcher]"
      refute output =~ "Elixir."
    end

    test "preserves Erlang module names as-is" do
      log_event = %{
        level: :info,
        msg: {:string, "test"},
        meta: %{
          time: 0,
          mfa: {:logger, :log, 3}
        }
      }

      result = LogFormatter.format(log_event, %{})
      output = IO.iodata_to_binary(result)

      assert output =~ "[logger]"
    end
  end
end
