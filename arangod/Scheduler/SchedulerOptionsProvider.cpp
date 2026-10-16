////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
///
////////////////////////////////////////////////////////////////////////////////

#include "SchedulerOptionsProvider.h"

#include "Basics/NumberOfCores.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void SchedulerOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    SchedulerFeatureOptions& opts) {
  options
      ->addOption(
          "--server.maximal-threads",
          std::string("The maximum number of request handling threads to run "
                      "(0 = use system-specific default of ") +
              std::to_string(SchedulerFeatureOptions::getDefaultMaxThreads()) +
              ")",
          new UInt64Parameter(&opts.nrMaximalThreads),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Dynamic))
      .setLongDescription(R"(This option determines the maximum number of
request processing threads the server is allowed to start for request handling.
If this number of threads is already running, arangod does not start further
threads for request handling. The default value is
`max(32, 2 * available cores)`, so twice the number of CPU cores, but at least
32 threads.

The actual number of request processing threads is adjusted dynamically at
runtime and is between `--server.minimal-threads` and
`--server.maximal-threads`.)");

  options
      ->addOption("--server.minimal-threads",
                  "The minimum number of request handling threads to run.",
                  new UInt64Parameter(&opts.nrMinimalThreads),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(This option determines the minimum number of
request processing threads the server starts and always keeps around.)");

  options
      ->addOption("--server.ongoing-low-priority-multiplier",
                  "Controls the number of low priority requests that can be "
                  "ongoing at a given point in time, relative to the "
                  "maximum number of request handling threads.",
                  new DoubleParameter(&opts.ongoingLowPriorityMultiplier),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30800)
      .setLongDescription(R"(There are some countermeasures built into
Coordinators to prevent a cluster from being overwhelmed by too many
concurrently executing requests.

If a request is executed on a Coordinator but needs to wait for some operation
on a DB-Server, the operating system thread executing the request can often
postpone execution on the Coordinator, put the request to one side and do
something else in the meantime. When the response from the DB-Server arrives,
another worker thread continues the work. This is a form of asynchronous
implementation, which is great to achieve better thread utilization and enhance
throughput.

On the other hand, this runs the risk that work is started on new requests
faster than old ones can be finished off. Before version 3.8, this could
overwhelm the cluster over time, and lead to out-of-memory situations and other
unwanted side effects. For example, it could lead to excessive latency for
individual requests.

There is a limit as to how many requests coming from the low priority queue
(most client requests are of this type), can be executed concurrently.
The default value for this is 4 times as many as there are scheduler threads
(see `--server.minimal-threads` and --server.maximal-threads), which is good
for most workloads. Requests in excess of this are not started but remain on
the scheduler's input queue (see `--server.maximal-queue-size`).

Very occasionally, 4 is already too much. You would notice this if the latency
for individual requests is already too high because the system tries to execute
too many of them at the same time (for example, if they fight for resources).

On the other hand, in rare cases it is possible that throughput can be improved
by increasing the value, if latency is not a big issue and all requests
essentially spend their time waiting, so that a high concurrency is acceptable.
This increases memory usage, though.)");

  options
      ->addOption("--server.maximal-queue-size",
                  "The size of the priority 3 FIFO.",
                  new UInt64Parameter(&opts.fifo3Size))
      .setLongDescription(R"(You can specify the maximum size of the queue for
asynchronous task execution. If the queue already contains this many tasks, new
tasks are rejected until other tasks are popped from the queue. Setting this
value may help preventing an instance from being overloaded or from running out
of memory if the queue is filled up faster than the server can process
requests.)");

  options
      ->addOption("--server.unavailability-queue-fill-grade",
                  "The queue fill grade from which onwards the server is "
                  "considered unavailable because of an overload (ratio, "
                  "0 = disable)",
                  new DoubleParameter(&opts.unavailabilityQueueFillGrade,
                                      /*base*/ 1.0,
                                      /*minValue*/ 0.0, /*maxValue*/ 1.0))
      .setLongDescription(R"(You can use this option to set a high-watermark
for the scheduler's queue fill grade, from which onwards the server starts
reporting unavailability via its availability API.

This option has a consequence for the `/_admin/server/availability` REST API
only, which is often called by load-balancers and other availability probing
systems.

The `/_admin/server/availability` REST API returns HTTP 200 if the fill
grade of the scheduler's queue is below the configured value, or HTTP 503 if
the fill grade is equal to or above it. This can be used to flag a server as
unavailable in case it is already highly loaded.

The default value for this option is `0.75` since version 3.8, i.e. 75%.

To prevent sending more traffic to an already overloaded server, it can be
sensible to reduce the default value to even `0.5`. This would mean that
instances with a queue longer than 50% of their maximum queue capacity would
return HTTP 503 instead of HTTP 200 when their availability API is probed.)");

  options->addOption(
      "--server.scheduler-queue-size",
      "The number of simultaneously queued requests inside the scheduler.",
      new UInt64Parameter(&opts.queueSize),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption("--server.prio2-size", "The size of the priority 2 FIFO.",
                  new UInt64Parameter(&opts.fifo2Size),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30800);

  options->addOption(
      "--server.prio1-size", "The size of the priority 1 FIFO.",
      new UInt64Parameter(&opts.fifo1Size),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption(
          "--server.scheduler", "The scheduler type to use.",
          new DiscreteValuesParameter<StringParameter>(
              &opts.schedulerType,
              std::unordered_set<std::string>{"supervised", "threadpools"}),
          arangodb::options::makeFlags(arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31201);

  options->addObsoleteOption("--server.threads", "number of threads", true);
  options->addObsoleteOption(
      "--server.max-number-detached-threads",
      "The maximum number of detached scheduler threads.", true);
  options->addOldOption("scheduler.threads", "server.maximal-threads");
}

void SchedulerOptionsProvider::processOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    SchedulerFeatureOptions& opts) {
  auto const N = NumberOfCores::getValue();

  LOG_TOPIC("2ef39", DEBUG, arangodb::Logger::THREADS)
      << "Detected number of processors: " << N;

  TRI_ASSERT(N > 0);
  if (options->processingResult().touched("server.maximal-threads") &&
      opts.nrMaximalThreads > 8 * N) {
    LOG_TOPIC("0a92a", WARN, arangodb::Logger::THREADS)
        << "--server.maximal-threads (" << opts.nrMaximalThreads
        << ") is more than eight times the number of cores (" << N
        << "), this might overload the server";
  } else if (opts.nrMaximalThreads == 0) {
    opts.nrMaximalThreads = SchedulerFeatureOptions::getDefaultMaxThreads();
  }

  if (opts.nrMinimalThreads < 4) {
    LOG_TOPIC("bf034", WARN, arangodb::Logger::THREADS)
        << "--server.minimal-threads (" << opts.nrMinimalThreads
        << ") must be at least 4";
    opts.nrMinimalThreads = 4;
  }

  if (opts.ongoingLowPriorityMultiplier < 1.0) {
    LOG_TOPIC("0a93a", WARN, arangodb::Logger::THREADS)
        << "--server.ongoing-low-priority-multiplier ("
        << opts.ongoingLowPriorityMultiplier
        << ") is less than 1.0, setting to default (4.0)";
    opts.ongoingLowPriorityMultiplier = 4.0;
  }

  if (opts.nrMinimalThreads >= opts.nrMaximalThreads) {
    LOG_TOPIC("48e02", WARN, arangodb::Logger::THREADS)
        << "--server.maximal-threads (" << opts.nrMaximalThreads
        << ") should be at least " << (opts.nrMinimalThreads + 1)
        << ", raising it";
    opts.nrMaximalThreads = opts.nrMinimalThreads;
  }

  if (opts.queueSize == 0) {
    TRI_ASSERT(opts.nrMaximalThreads > 0);
    opts.queueSize = opts.nrMaximalThreads * 8;
    TRI_ASSERT(opts.queueSize > 0);
  }

  if (opts.fifo1Size < 1) {
    opts.fifo1Size = 1;
  }

  if (opts.fifo2Size < 1) {
    opts.fifo2Size = 1;
  }

  TRI_ASSERT(opts.queueSize > 0);
}

}  // namespace arangodb
