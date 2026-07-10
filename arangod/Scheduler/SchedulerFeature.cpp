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
#include <limits>
#include <memory>

#include "SchedulerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/asio_ns.h"
#include "Basics/NumberOfCores.h"
#include "Basics/application-exit.h"
#include "Basics/signals.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Scheduler/SchedulerOptionsProvider.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SupervisedScheduler.h"
#include "Scheduler/ThreadPoolScheduler.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace {
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

SchedulerFeature::SchedulerFeature(
    application_features::ApplicationServer& server,
    metrics::IRegistry& metricsRegistry, basics::SharedPRNG& sharedPRNG)
    : SchedulerFeature(server, metricsRegistry, sharedPRNG,
                       SchedulerFeatureOptions{}) {}

SchedulerFeature::SchedulerFeature(
    application_features::ApplicationServer& server,
    metrics::IRegistry& metricsRegistry, basics::SharedPRNG& sharedPRNG,
    SchedulerFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _scheduler(nullptr),
      _sharedPRNG(sharedPRNG),
      _metricsRegistry(metricsRegistry),
      _asioHandler(std::make_unique<AsioHandler>()) {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();
  if (server.hasFeature<FileDescriptorsFeature>()) {
    startsAfter<FileDescriptorsFeature>();
  }
}

SchedulerFeature::~SchedulerFeature() = default;

void SchedulerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  SchedulerOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void SchedulerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  SchedulerOptionsProvider provider;
  provider.validateOptions(options, _options);
}

void SchedulerFeature::prepare() {
  TRI_ASSERT(4 <= _options.nrMinimalThreads);
  TRI_ASSERT(_options.nrMinimalThreads <= _options.nrMaximalThreads);
  TRI_ASSERT(_options.queueSize > 0);

  auto metrics = std::make_shared<SchedulerMetrics>(_metricsRegistry);
  _scheduler = std::invoke([&]() -> std::unique_ptr<Scheduler> {
    if (_options.schedulerType == "supervised") {
      // on a DB server we intentionally disable throttling of incoming
      // requests. this is because coordinators are the gatekeepers, and they
      // should perform all the throttling.
      uint64_t ongoingLowPriorityLimit =
          ServerState::instance()->isDBServer() ||
                  ServerState::instance()->isAgent()
              ? 0
              : static_cast<uint64_t>(_options.ongoingLowPriorityMultiplier *
                                      _options.nrMaximalThreads);
      return std::make_unique<SupervisedScheduler>(
          server(), _options.nrMinimalThreads, _options.nrMaximalThreads,
          _options.queueSize, _options.fifo1Size, _options.fifo2Size,
          _options.fifo3Size, ongoingLowPriorityLimit,
          _options.unavailabilityQueueFillGrade, metrics, _sharedPRNG);
    } else {
      TRI_ASSERT(_options.schedulerType == "threadpools");
      return std::make_unique<ThreadPoolScheduler>(
          server(), _options.nrMaximalThreads, std::move(metrics));
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
  return _options.nrMaximalThreads;
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
          Logger::reopen();
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
