defmodule ToastTest.Runner.TestProcess do
  @moduledoc false

  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.Runner.{FailureFormatter, Timeout}
  alias ToastTest.{Abort, TestLifecycle}

  def spawn_test(config, test, context) do
    parent_pid = self()
    {timeout, timeout_source} = Timeout.get_timeout(config, test.tags)
    start_time = System.monotonic_time()
    {test_pid, test_ref} = spawn_test_monitor(config, test, parent_pid, context)
    Abort.register_test_pid(test_pid)
    test = receive_test_reply(test, test_pid, test_ref, timeout, timeout_source, start_time)
    Abort.unregister_test_pid()
    exec_on_exit(test, test_pid, timeout)
  end

  def exec_on_exit(test_or_case, pid, timeout) do
    case TestLifecycle.run_on_exit(pid, timeout) do
      :ok ->
        test_or_case

      {kind, reason, stack} ->
        state =
          test_or_case.state ||
            FailureFormatter.failed(kind, reason, FailureFormatter.prune_stacktrace(stack))

        %{test_or_case | state: state}
    end
  end

  defp spawn_test_monitor(
         %{capture_log: capture_log},
         test,
         parent_pid,
         context
       ) do
    spawn_monitor(fn ->
      Process.set_label({test.case, test.name})
      ExUnit.OnExitHandler.register(self())
      context = context |> Map.merge(test.tags) |> Map.put(:test_pid, self())
      capture_log = Map.get(context, :capture_log, capture_log)

      {time, test} =
        :timer.tc(
          maybe_capture_log(capture_log, test, fn ->
            run_test_with_setup(test, context)
          end)
        )

      send(parent_pid, {self(), :test_finished, %{test | time: time}})
      exit(:shutdown)
    end)
  end

  defp run_test_with_setup(test, context) do
    case exec_test_setup(test, context) do
      {:ok, context} -> exec_test(test, context)
      {:skipped, test} -> test
      {:error, test} -> test
    end
  end

  defp maybe_capture_log(false, _test, fun), do: fun
  defp maybe_capture_log(true, test, fun), do: maybe_capture_log([], test, fun)

  defp maybe_capture_log(capture_log_opts, test, fun) when is_list(capture_log_opts) do
    fn ->
      try do
        ExUnit.CaptureLog.with_log(capture_log_opts, fun)
      catch
        :exit, :noproc ->
          message =
            "could not run test, it uses @tag :capture_log" <>
              " but the :logger application is not running"

          %{test | state: FailureFormatter.failed(:error, RuntimeError.exception(message), [])}
      else
        {test, logs} -> %{test | logs: logs}
      end
    end
  end

  defp receive_test_reply(test, test_pid, test_ref, timeout, timeout_source, start_time) do
    receive do
      {^test_pid, :test_finished, test} ->
        Process.demonitor(test_ref, [:flush])
        test

      {:DOWN, ^test_ref, :process, ^test_pid, :killed} ->
        elapsed_us = elapsed_us(start_time)

        exception =
          case Abort.reason() do
            nil -> RuntimeError.exception("test process was killed")
            reason -> RuntimeError.exception("test aborted: #{Abort.display_reason(reason)}")
          end

        %{test | state: FailureFormatter.failed(:error, exception, []), time: elapsed_us}

      {:DOWN, ^test_ref, :process, ^test_pid, error} ->
        elapsed_us = elapsed_us(start_time)
        %{test | state: FailureFormatter.failed({:EXIT, test_pid}, error, []), time: elapsed_us}
    after
      timeout ->
        case Process.info(test_pid, :current_stacktrace) do
          {:current_stacktrace, stacktrace} ->
            Process.demonitor(test_ref, [:flush])
            Process.exit(test_pid, :kill)
            elapsed_us = elapsed_us(start_time)

            exception =
              ToastTest.TimeoutError.exception(
                timeout: timeout,
                type: Atom.to_string(test.tags.test_type),
                source: timeout_source
              )

            %{
              test
              | state: FailureFormatter.failed(:error, exception, stacktrace),
                time: elapsed_us
            }

          nil ->
            receive_test_reply(test, test_pid, test_ref, timeout, timeout_source, start_time)
        end
    end
  end

  defp elapsed_us(start_time) do
    System.convert_time_unit(System.monotonic_time() - start_time, :native, :microsecond)
  end

  defp exec_test_setup(%ExUnit.Test{module: module} = test, context) do
    if reason = Abort.reason() do
      {:skipped,
       %{
         test
         | state: {:skipped, Abort.format_skip(reason)}
       }}
    else
      {:ok, Compat.get_test_setup(module, context)}
    end
  catch
    kind, error ->
      {:error,
       %{
         test
         | state:
             FailureFormatter.failed(
               kind,
               error,
               FailureFormatter.prune_stacktrace(__STACKTRACE__)
             )
       }}
  end

  defp exec_test(%ExUnit.Test{module: module, name: name} = test, context) do
    apply(module, name, [context])
    test
  catch
    kind, error ->
      %{
        test
        | state:
            FailureFormatter.failed(
              kind,
              error,
              FailureFormatter.prune_stacktrace(__STACKTRACE__)
            )
      }
  end
end
