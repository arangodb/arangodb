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

defmodule Toast.Diagnostics.Coredump.ThreadFilter do
  @moduledoc false

  # Idle thread filtering for --threads relevant
  #
  # A thread is considered idle (irrelevant) if it's waiting for work
  # rather than actively doing something.  The crash thread is always kept.
  #
  # Two categories:
  # 1. Immediate idle — the function IS the blocking call (epoll, asio event wait).
  #    Presence anywhere in the backtrace → thread is idle.
  # 2. Conditional idle — thread pool / worker loops that are only idle when the
  #    frame directly above them (closer to top of stack) is a condition wait.

  @immediate_idle_functions [
    "epoll_wait",
    "boost::asio::detail::posix_event::wait"
  ]

  @cond_wait_patterns [
    "pthread_cond_wait",
    "pthread_cond_timedwait",
    "pthread_cond_clockwait",
    "std::condition_variable::wait_for"
  ]

  @idle_loop_functions [
    # gdb unnamed symbol
    "??",
    "___lldb_unnamed_symbol",
    "arangodb::application_features::ApplicationServer::wait",
    "arangodb::async_registry::Feature::PromiseCleanupThread",
    "arangodb::CacheRebalancerThread::run",
    "arangodb::IOHeartbeatThread::run",
    "arangodb::RocksDBBackgroundThread::run",
    "arangodb::RocksDBIndexCacheRefillThread::run",
    "arangodb::RocksDBSyncThread::run",
    "arangodb::Scheduler::runCronThread",
    "arangodb::StatisticsWorker::run",
    "arangodb::SupervisedScheduler::getWork",
    "arangodb::SupervisedScheduler::runSupervisor",
    "arangodb::TtlThread::run",
    "arangodb::V8DealerFeature::collectGarbage",
    "background_thread_sleep",
    "background_work_sleep_once",
    "boost::asio::detail::scheduler::do_run_one",
    "irs::async_utils::ThreadPool",
    "rocksdb::ThreadPoolImpl::Impl::BGThread"
  ]

  @idle_loop_files [
    "default-worker-threads-task-runner"
  ]

  @doc """
  Filters out idle threads from a coredump's thread list.

  Returns only threads that are actively doing something (not waiting for work).
  The crash thread is always kept.
  """
  def filter_relevant(threads, coredump) do
    crash_id = coredump[:crash_thread]

    kept =
      Enum.reject(threads, fn thread ->
        to_string(thread.id) != to_string(crash_id) and idle_thread?(thread[:frames] || [])
      end)

    # If nothing survived (e.g., no crash_thread set), keep at least the first thread.
    if kept == [], do: Enum.take(threads, 1), else: kept
  end

  defp idle_thread?(frames) do
    frames == [] or immediate_idle?(frames) or cond_wait_idle?(frames)
  end

  defp immediate_idle?(frames) do
    # Use String.contains? because LLDB may inline functions into a single frame,
    # e.g., "do_run_one(...) [inlined] posix_event::wait(...)".
    Enum.any?(frames, fn frame ->
      Enum.any?(@immediate_idle_functions, &String.contains?(frame.function, &1))
    end)
  end

  defp cond_wait_idle?(frames) do
    # Check consecutive frame pairs [inner (closer to top), outer (closer to bottom)].
    # If inner is a cond_wait and outer is an idle loop → thread is idle.
    # Use String.contains? because LLDB includes argument lists in function names
    # and may inline multiple functions into a single frame.
    frames
    |> Enum.chunk_every(2, 1, :discard)
    |> Enum.any?(fn [inner, outer] ->
      cond_wait_frame?(inner) and idle_loop_frame?(outer)
    end)
  end

  defp cond_wait_frame?(frame) do
    Enum.any?(@cond_wait_patterns, &String.contains?(frame.function, &1))
  end

  defp idle_loop_frame?(frame) do
    Enum.any?(@idle_loop_functions, &String.contains?(frame.function, &1)) or
      (frame[:file] != nil and
         Enum.any?(@idle_loop_files, &String.contains?(frame.file, &1)))
  end
end
