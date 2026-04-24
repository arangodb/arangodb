defmodule Toast.Utils.Polling do
  @moduledoc """
  Deadline-based polling: invoke a probe function repeatedly until it reports
  completion or the monotonic deadline passes.

  The probe runs immediately on entry (probe-first); between probes the caller
  sleeps for `poll_interval` ms, clamped to never overshoot `deadline`.
  """

  @type probe_result(t) :: {:done, t} | :not_ready

  @spec poll_until((-> probe_result(t)), integer(), pos_integer()) ::
          {:ok, t} | {:error, :timeout}
        when t: term()
  def poll_until(probe_fn, deadline, poll_interval)
      when is_function(probe_fn, 0) and is_integer(deadline) and is_integer(poll_interval) and
             poll_interval > 0 do
    case probe_fn.() do
      {:done, result} ->
        {:ok, result}

      :not_ready ->
        now = System.monotonic_time(:millisecond)

        if now >= deadline do
          {:error, :timeout}
        else
          Process.sleep(min(poll_interval, deadline - now))
          poll_until(probe_fn, deadline, poll_interval)
        end
    end
  end
end
