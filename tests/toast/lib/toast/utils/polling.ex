################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

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
