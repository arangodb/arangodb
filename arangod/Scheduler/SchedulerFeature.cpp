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
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <chrono>
#include <limits>
#include <memory>
#include <thread>

#include "SchedulerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/asio_ns.h"
#include "Basics/NumberOfCores.h"
#include "Basics/application-exit.h"
#include "Basics/signals.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Logger/LogAppender.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/ServerFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SupervisedScheduler.h"
#include "Scheduler/ThreadPoolScheduler.h"
#ifdef USE_V8
#include "VocBase/Methods/Tasks.h"
#endif

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace {
/// @brief return the default number of threads to use (upper bound)
size_t defaultNumberOfThreads() {
  // use two times the number of hardware threads as the default
  size_t result = arangodb::NumberOfCores::getValue() * 2;
  // but only if higher than 64. otherwise use a default minimum value of 32
  if (result < 32) {
    result = 32;
  }
  return result;
}

// atomic flag to track shutdown requests
std::atomic<bool> receivedShutdownRequest{false};

// id of process that will not be used to send SIGHUP requests
constexpr pid_t processIdUnspecified{std::numeric_limits<pid_t>::min()};

static_assert(processIdUnspecified != 0, "minimum pid number must be != 0");

// id of process that requested a log rotation via SIGHUP
std::atomic<pid_t> processIdRequestingLogRotate{processIdUnspecified};

}  // namespace

namespace arangodb {

Scheduler* SchedulerFeature::SCHEDULER = nullptr;

struct SchedulerFeature::AsioHandler {
  std::shared_ptr<asio_ns::signal_set> _exitSignals;
  std::shared_ptr<asio_ns::signal_set> _hangupSignals;
};

SchedulerFeature::SchedulerFeature(Server& server,
                                   metrics::MetricsFeature& metrics)
    : ArangodFeature{server, *this},
      _scheduler(nullptr),
      _metricsFeature(metrics),
      _asioHandler(std::make_unique<AsioHandler>()) {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();
  if constexpr (Server::contains<FileDescriptorsFeature>()) {
    startsAfter<FileDescriptorsFeature>();
  }
}

SchedulerFeature::~SchedulerFeature() = default;

void SchedulerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  // Different implementations of the Scheduler may require different
  // options to be set. This requires a solution here.

  // max / min number of threads
  options
      ->addOption(
          "--server.maximal-threads",
          std::string("The maximum number of request handling threads to run "
                      "(0 = use system-specific default of ") +
              std::to_string(defaultNumberOfThreads()) + ")",
          new UInt64Parameter(&_nrMaximalThreads),
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
                  new UInt64Parameter(&_nrMinimalThreads),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(This option determines the minimum number of
request processing threads the server starts and always keeps around.)");

  // max / min number of threads

  // Concurrency throttling:
  options
      ->addOption("--server.ongoing-low-priority-multiplier",
                  "Controls the number of low priority requests that can be "
                  "ongoing at a given point in time, relative to the "
                  "maximum number of request handling threads.",
                  new DoubleParameter(&_ongoingLowPriorityMultiplier),
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
                  new UInt64Parameter(&_fifo3Size))
      .setLongDescription(R"(You can specify the maximum size of the queue for
asynchronous task execution. If the queue already contains this many tasks, new
tasks are rejected until other tasks are popped from the queue. Setting this
value may help preventing an instance from being overloaded or from running out
of memory if the queue is filled up faster than the server can process
requests.)");

  options
      ->addOption(
          "--server.unavailability-queue-fill-grade",
          "The queue fill grade from which onwards the server is "
          "considered unavailable because of an overload (ratio, "
          "0 = disable)",
          new DoubleParameter(&_unavailabilityQueueFillGrade, /*base*/ 1.0,
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
      new UInt64Parameter(&_queueSize),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption("--server.prio2-size", "The size of the priority 2 FIFO.",
                  new UInt64Parameter(&_fifo2Size),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30800);

  options->addOption(
      "--server.prio1-size", "The size of the priority 1 FIFO.",
      new UInt64Parameter(&_fifo1Size),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption(
          "--server.scheduler", "The scheduler type to use.",
          new DiscreteValuesParameter<StringParameter>(
              &_schedulerType,
              std::unordered_set<std::string>{"supervised", "threadpools"}),
          arangodb::options::makeFlags(arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31210);

  // obsolete options
  options->addObsoleteOption("--server.threads", "number of threads", true);

  options->addObsoleteOption(
      "--server.max-number-detached-threads",
      "The maximum number of detached scheduler threads.", true);

  // renamed options
  options->addOldOption("scheduler.threads", "server.maximal-threads");
}

void SchedulerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const N = NumberOfCores::getValue();

  LOG_TOPIC("2ef39", DEBUG, arangodb::Logger::THREADS)
      << "Detected number of processors: " << N;

  TRI_ASSERT(N > 0);
  if (options->processingResult().touched("server.maximal-threads") &&
      _nrMaximalThreads > 8 * N) {
    LOG_TOPIC("0a92a", WARN, arangodb::Logger::THREADS)
        << "--server.maximal-threads (" << _nrMaximalThreads
        << ") is more than eight times the number of cores (" << N
        << "), this might overload the server";
  } else if (_nrMaximalThreads == 0) {
    _nrMaximalThreads = defaultNumberOfThreads();
  }

  if (_nrMinimalThreads < 4) {
    LOG_TOPIC("bf034", WARN, arangodb::Logger::THREADS)
        << "--server.minimal-threads (" << _nrMinimalThreads
        << ") must be at least 4";
    _nrMinimalThreads = 4;
  }

  if (_ongoingLowPriorityMultiplier < 1.0) {
    LOG_TOPIC("0a93a", WARN, arangodb::Logger::THREADS)
        << "--server.ongoing-low-priority-multiplier ("
        << _ongoingLowPriorityMultiplier
        << ") is less than 1.0, setting to default (4.0)";
    _ongoingLowPriorityMultiplier = 4.0;
  }

  if (_nrMinimalThreads >= _nrMaximalThreads) {
    LOG_TOPIC("48e02", WARN, arangodb::Logger::THREADS)
        << "--server.maximal-threads (" << _nrMaximalThreads
        << ") should be at least " << (_nrMinimalThreads + 1) << ", raising it";
    _nrMaximalThreads = _nrMinimalThreads;
  }

  if (_queueSize == 0) {
    // Note that this is way smaller than the default of 4096!
    TRI_ASSERT(_nrMaximalThreads > 0);
    _queueSize = _nrMaximalThreads * 8;
    TRI_ASSERT(_queueSize > 0);
  }

  if (_fifo1Size < 1) {
    _fifo1Size = 1;
  }

  if (_fifo2Size < 1) {
    _fifo2Size = 1;
  }

  TRI_ASSERT(_queueSize > 0);
}

void SchedulerFeature::prepare() {
  TRI_ASSERT(4 <= _nrMinimalThreads);
  TRI_ASSERT(_nrMinimalThreads <= _nrMaximalThreads);
  TRI_ASSERT(_queueSize > 0);

  auto metrics = std::make_shared<SchedulerMetrics>(_metricsFeature);
  _scheduler = std::invoke([&]() -> std::unique_ptr<Scheduler> {
    if (_schedulerType == "supervised") {
      // on a DB server we intentionally disable throttling of incoming
      // requests. this is because coordinators are the gatekeepers, and they
      // should perform all the throttling.
      uint64_t ongoingLowPriorityLimit =
          ServerState::instance()->isDBServer()
              ? 0
              : static_cast<uint64_t>(_ongoingLowPriorityMultiplier *
                                      _nrMaximalThreads);
      return std::make_unique<SupervisedScheduler>(
          server(), _nrMinimalThreads, _nrMaximalThreads, _queueSize,
          _fifo1Size, _fifo2Size, _fifo3Size, ongoingLowPriorityLimit,
          _unavailabilityQueueFillGrade, metrics);
    } else {
      TRI_ASSERT(_schedulerType == "threadpools");
      return std::make_unique<ThreadPoolScheduler>(server(), _nrMaximalThreads,
                                                   std::move(metrics));
    }
  });

  SCHEDULER = _scheduler.get();
}

void SchedulerFeature::start() {
  signalStuffInit();

  bool ok = _scheduler->start();
  if (!ok) {
    LOG_TOPIC("7f497", FATAL, arangodb::Logger::FIXME)
        << "the scheduler cannot be started";
    FATAL_ERROR_EXIT();
  }
  LOG_TOPIC("14e6f", DEBUG, Logger::STARTUP) << "scheduler has started";
}

void SchedulerFeature::stop() {
  // shutdown user jobs again, in case new ones appear
#ifdef USE_V8
  arangodb::Task::shutdownTasks();
#endif
  signalStuffDeinit();
  _scheduler->shutdown();
}

void SchedulerFeature::unprepare() {
  // SCHEDULER = nullptr;
  // This is to please the TSAN sanitizer: On shutdown, we set this global
  // pointer to nullptr. Other threads read the pointer, but the logic of
  // ApplicationFeatures should ensure that nobody will read the pointer
  // out after the SchedulerFeature has run its unprepare method.
  // Sometimes the TSAN sanitizer cannot recognize this indirect
  // synchronization and complains about reads that have happened before
  // this write here, but are not officially inter-thread synchronized.
  // We use the atomic reference here and in these places to silence TSAN.
  std::atomic_ref<Scheduler*> schedulerRef{SCHEDULER};
  schedulerRef.store(nullptr, std::memory_order_relaxed);
  _scheduler.reset();
}

uint64_t SchedulerFeature::maximalThreads() const noexcept {
  return _nrMaximalThreads;
}

// ---------------------------------------------------------------------------
// Signal Handler stuff - no body knows what this has to do with scheduling
// ---------------------------------------------------------------------------

void SchedulerFeature::signalStuffInit() {
  arangodb::signals::maskAllSignalsServer();

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);

  // ignore broken pipes
  action.sa_handler = SIG_IGN;

  int res = sigaction(SIGPIPE, &action, nullptr);

  if (res < 0) {
    LOG_TOPIC("91d20", ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handler for SIGPIPE";
  }

  buildHangupHandler();
}

void SchedulerFeature::signalStuffDeinit() {
  // cancel signals
  if (_asioHandler->_exitSignals != nullptr) {
    auto exitSignals = _asioHandler->_exitSignals;
    _asioHandler->_exitSignals.reset();
    exitSignals->cancel();
  }

  if (_asioHandler->_hangupSignals != nullptr) {
    _asioHandler->_hangupSignals->cancel();
    _asioHandler->_hangupSignals.reset();
  }
}

extern "C" void c_exit_handler(int signal, siginfo_t* info, void*) {
  if (signal == SIGQUIT || signal == SIGTERM || signal == SIGINT) {
    if (!::receivedShutdownRequest.exchange(true)) {
      LOG_TOPIC("b4133", INFO, arangodb::Logger::FIXME)
          << signals::name(signal) << " received (sender pid "
          << (info ? info->si_pid : 0) << "), beginning shut down sequence";
      application_features::ApplicationServer::CTRL_C.store(true);
    } else {
      LOG_TOPIC("11ca3", FATAL, arangodb::Logger::FIXME)
          << signals::name(signal)
          << " received during shutdown sequence (sender pid " << info->si_pid
          << "), terminating!";
      FATAL_ERROR_EXIT();
    }
  }
}

extern "C" void c_hangup_handler(int signal, siginfo_t* info, void*) {
  TRI_ASSERT(signal == SIGHUP);

  // id of process that issued the SIGHUP.
  // if we don't have any information about the issuing process, we
  // assume a pid of 0.
  pid_t processIdRequesting = info ? info->si_pid : 0;
  // note that we need to be able to tell pid 0 and the "unspecified"
  // process id apart.
  static_assert(::processIdUnspecified != 0, "unspecified pid should be != 0");

  // the expected process id that we want to see
  pid_t processIdExpected = ::processIdUnspecified;

  // only set log rotate request if we don't have one queued already. this
  // prevents duplicate execution of log rotate requests.
  // if the CAS fails, it doesn't matter, because it means that a log rotate
  // request was already queued
  if (!::processIdRequestingLogRotate.compare_exchange_strong(
          processIdExpected, processIdRequesting)) {
    // already a log rotate request queued. do nothing...
    return;
  }

  // no log rotate request queued before. now issue one.
  SchedulerFeature::SCHEDULER->queue(
      RequestLane::CLIENT_SLOW, [processIdRequesting]() {
        try {
          LOG_TOPIC("33eae", INFO, arangodb::Logger::FIXME)
              << "hangup received, about to reopen logfile (sender pid "
              << processIdRequesting << ")";
          LogAppender::reopen();
          LOG_TOPIC("23db2", INFO, arangodb::Logger::FIXME)
              << "hangup received, reopened logfile";
        } catch (...) {
          // cannot do much if log rotate request goes wrong
        }
        ::processIdRequestingLogRotate.store(::processIdUnspecified);
      });
}

void SchedulerFeature::buildHangupHandler() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = c_hangup_handler;

  int res = sigaction(SIGHUP, &action, nullptr);

  if (res < 0) {
    LOG_TOPIC("b7ed0", ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handler for hang up";
  }
}

void SchedulerFeature::buildControlCHandler() {
  // Signal masking on POSIX platforms
  //
  // POSIX allows signals to be blocked using functions such as sigprocmask()
  // and pthread_sigmask(). For signals to be delivered, programs must ensure
  // that any signals registered using signal_set objects are unblocked in at
  // least one thread.
  arangodb::signals::unmaskAllSignals();

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = c_exit_handler;

  int res = sigaction(SIGINT, &action, nullptr);
  if (res == 0) {
    res = sigaction(SIGQUIT, &action, nullptr);
    if (res == 0) {
      res = sigaction(SIGTERM, &action, nullptr);
    }
  }
  if (res < 0) {
    LOG_TOPIC("e666b", ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for SIGINT/SIGQUIT/SIGTERM";
  }
}

}  // namespace arangodb
