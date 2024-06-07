////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SchedulerMetrics.h"

#include "Metrics/GaugeBuilder.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb;

DECLARE_GAUGE(arangodb_scheduler_num_awake_threads, uint64_t,
              "Number of awake worker threads");
DECLARE_COUNTER(arangodb_scheduler_jobs_done_total,
                "Total number of queue jobs done");
DECLARE_COUNTER(arangodb_scheduler_jobs_submitted_total,
                "Total number of jobs submitted to the scheduler");
DECLARE_COUNTER(arangodb_scheduler_jobs_dequeued_total,
                "Total number of jobs dequeued");
DECLARE_GAUGE(
    arangodb_scheduler_high_prio_queue_length, uint64_t,
    "Current queue length of the high priority queue in the scheduler");
DECLARE_GAUGE(arangodb_scheduler_low_prio_queue_last_dequeue_time, uint64_t,
              "Last recorded dequeue time for a low priority queue item [ms]");
DECLARE_GAUGE(
    arangodb_scheduler_low_prio_queue_length, uint64_t,
    "Current queue length of the low priority queue in the scheduler");
DECLARE_GAUGE(
    arangodb_scheduler_maintenance_prio_queue_length, uint64_t,
    "Current queue length of the maintenance priority queue in the scheduler");
DECLARE_GAUGE(
    arangodb_scheduler_medium_prio_queue_length, uint64_t,
    "Current queue length of the medium priority queue in the scheduler");

struct DequeueScale {
  static metrics::LogScale<double> scale() {
    return {10.0, 0.0, 10'000'000, 10};
  }
};
DECLARE_HISTOGRAM(arangodb_scheduler_low_prio_dequeue_hist, DequeueScale,
                  "Low priority deque histogram [µs]");
DECLARE_HISTOGRAM(arangodb_scheduler_medium_prio_dequeue_hist, DequeueScale,
                  "Medium priority deque histogram [µs]");
DECLARE_HISTOGRAM(arangodb_scheduler_high_prio_dequeue_hist, DequeueScale,
                  "High priority deque histogram [µs]");
DECLARE_HISTOGRAM(arangodb_scheduler_maintenance_prio_dequeue_hist,
                  DequeueScale, "Maintenance priority deque histogram [µs]");

DECLARE_GAUGE(arangodb_scheduler_num_working_threads, uint64_t,
              "Number of working threads");
DECLARE_GAUGE(arangodb_scheduler_num_worker_threads, uint64_t,
              "Number of worker threads");
DECLARE_GAUGE(arangodb_scheduler_num_detached_threads, uint64_t,
              "Number of detached worker threads");
DECLARE_GAUGE(arangodb_scheduler_stack_memory_usage, uint64_t,
              "Approximate stack memory usage of worker threads");
DECLARE_GAUGE(
    arangodb_scheduler_ongoing_low_prio, uint64_t,
    "Total number of ongoing RestHandlers coming from the low prio queue");
DECLARE_COUNTER(arangodb_scheduler_handler_tasks_created_total,
                "Number of scheduler tasks created");
DECLARE_COUNTER(arangodb_scheduler_queue_full_failures_total,
                "Tasks dropped and not added to internal queue");
DECLARE_COUNTER(arangodb_scheduler_queue_time_violations_total,
                "Tasks dropped because the client-requested queue time "
                "restriction would be violated");
DECLARE_GAUGE(arangodb_scheduler_queue_length, uint64_t,
              "Server's internal queue length");
DECLARE_COUNTER(arangodb_scheduler_threads_started_total,
                "Number of scheduler threads started");
DECLARE_COUNTER(arangodb_scheduler_threads_stopped_total,
                "Number of scheduler threads stopped");
DECLARE_GAUGE(arangodb_scheduler_queue_memory_usage, std::int64_t,
              "Number of bytes allocated for tasks in the scheduler queue");

SchedulerMetrics::SchedulerMetrics(metrics::MetricsFeature& metrics)
    : _metricsQueueLength(metrics.add(arangodb_scheduler_queue_length{})),
      _metricsJobsDoneTotal(metrics.add(arangodb_scheduler_jobs_done_total{})),
      _metricsJobsSubmittedTotal(
          metrics.add(arangodb_scheduler_jobs_submitted_total{})),
      _metricsJobsDequeuedTotal(
          metrics.add(arangodb_scheduler_jobs_dequeued_total{})),
      _metricsNumAwakeThreads(
          metrics.add(arangodb_scheduler_num_awake_threads{})),
      _metricsNumWorkingThreads(
          metrics.add(arangodb_scheduler_num_working_threads{})),
      _metricsNumWorkerThreads(
          metrics.add(arangodb_scheduler_num_worker_threads{})),
      _metricsNumDetachedThreads(
          metrics.add(arangodb_scheduler_num_detached_threads{})),
      _metricsStackMemoryWorkerThreads(
          metrics.add(arangodb_scheduler_stack_memory_usage{})),
      _schedulerQueueMemory(
          metrics.add(arangodb_scheduler_queue_memory_usage{})),
      _metricsHandlerTasksCreated(
          metrics.add(arangodb_scheduler_handler_tasks_created_total{})),
      _metricsThreadsStarted(
          metrics.add(arangodb_scheduler_threads_started_total{})),
      _metricsThreadsStopped(
          metrics.add(arangodb_scheduler_threads_stopped_total{})),
      _metricsQueueFull(
          metrics.add(arangodb_scheduler_queue_full_failures_total{})),
      _metricsQueueTimeViolations(
          metrics.add(arangodb_scheduler_queue_time_violations_total{})),
      _ongoingLowPriorityGauge(
          metrics.add(arangodb_scheduler_ongoing_low_prio{})),
      _metricsLastLowPriorityDequeueTime(
          metrics.add(arangodb_scheduler_low_prio_queue_last_dequeue_time{})),
      _metricsDequeueTimes{
          &metrics.add(arangodb_scheduler_maintenance_prio_dequeue_hist{}),
          &metrics.add(arangodb_scheduler_high_prio_dequeue_hist{}),
          &metrics.add(arangodb_scheduler_medium_prio_dequeue_hist{}),
          &metrics.add(arangodb_scheduler_low_prio_dequeue_hist{})},
      _metricsQueueLengths{
          &metrics.add(arangodb_scheduler_maintenance_prio_queue_length{}),
          &metrics.add(arangodb_scheduler_high_prio_queue_length{}),
          &metrics.add(arangodb_scheduler_medium_prio_queue_length{}),
          &metrics.add(arangodb_scheduler_low_prio_queue_length{})} {}
