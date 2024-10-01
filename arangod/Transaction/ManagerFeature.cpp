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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ManagerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FunctionUtils.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/MetricsFeature.h"
#include "Metrics/CounterBuilder.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Manager.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb::transaction {

DECLARE_COUNTER(arangodb_transactions_expired_total,
                "Total number of expired transactions");

std::unique_ptr<transaction::Manager> ManagerFeature::MANAGER;

ManagerFeature::ManagerFeature(Server& server)
    : ArangodFeature{server, *this},
      _streamingMaxTransactionSize(defaultStreamingMaxTransactionSize),
      _streamingLockTimeout(8.0),
      _streamingIdleTimeout(defaultStreamingIdleTimeout),
      _numExpiredTransactions(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_transactions_expired_total{})) {
  static_assert(
      Server::isCreatedAfter<ManagerFeature, metrics::MetricsFeature>());

  setOptional(false);
  startsAfter<BasicFeaturePhaseServer>();
  startsAfter<EngineSelectorFeature>();
  startsAfter<metrics::MetricsFeature>();
  startsAfter<SchedulerFeature>();
  startsBefore<DatabaseFeature>();

  _gcfunc = [this](bool canceled) {
    if (canceled) {
      return;
    }

    if (MANAGER != nullptr) {
      MANAGER->garbageCollect(/*abortAll*/ false);
    }

    if (!this->server().isStopping()) {
      queueGarbageCollection();
    }
  };
}

ManagerFeature::~ManagerFeature() {
  std::lock_guard<std::mutex> guard(_workItemMutex);
  _workItem.reset();
}

void ManagerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("transaction", "transactions");

  options->addOption(
      "--transaction.streaming-lock-timeout",
      "The lock timeout (in seconds) "
      "in case of parallel access to the same Stream Transaction.",
      new DoubleParameter(&_streamingLockTimeout),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption("--transaction.streaming-idle-timeout",
                  "The idle timeout (in seconds) for Stream Transactions.",
                  new DoubleParameter(&_streamingIdleTimeout, /*base*/ 1.0,
                                      /*minValue*/ 0.0,
                                      /*maxValue*/ maxStreamingIdleTimeout),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800)
      .setLongDescription(R"(Stream Transactions automatically expire after
this period when no further operations are posted into them. Posting an
operation into a non-expired Stream Transaction resets the transaction's
timeout to the configured idle timeout.)");

  options
      ->addOption(
          "--transaction.streaming-max-transaction-size",
          "The maximum transaction size (in bytes) for Stream Transactions.",
          new SizeTParameter(&_streamingMaxTransactionSize),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200);
}

void ManagerFeature::prepare() {
  TRI_ASSERT(MANAGER.get() == nullptr);
  TRI_ASSERT(server().getFeature<EngineSelectorFeature>().selected());
  MANAGER = server()
                .getFeature<EngineSelectorFeature>()
                .engine()
                .createTransactionManager(*this);
}

void ManagerFeature::start() {
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  if (scheduler != nullptr) {  // is nullptr in catch tests
    queueGarbageCollection();
  }
}

void ManagerFeature::initiateSoftShutdown() {
  if (MANAGER != nullptr) {
    MANAGER->initiateSoftShutdown();
  }
}

void ManagerFeature::beginShutdown() {
  {
    // when we get here, ApplicationServer::isStopping() will always return
    // true already. So it is ok to wait here until the workItem has been
    // fully canceled. We are grabbing the mutex here, so the workItem cannot
    // reschedule itself if it doesn't have the mutex. If it is executed
    // directly afterwards, it will check isStopping(), which will return
    // false, so no rescheduled will be performed
    // if it doesn't hold the mutex, we will cancel it here (under the mutex)
    // and when the callback is executed, it will check isStopping(), which
    // will always return false
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }

  MANAGER->disallowInserts();
  // at this point all cursors should have been aborted already
  MANAGER->garbageCollect(/*abortAll*/ true);
  // make sure no lingering managed trx remain
  while (MANAGER->garbageCollect(/*abortAll*/ true)) {
    LOG_TOPIC("96298", INFO, Logger::TRANSACTIONS)
        << "still waiting for managed transaction";
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void ManagerFeature::stop() {
  // reset again, as there may be a race between beginShutdown and
  // the execution of the deferred _workItem
  {
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }

  // at this point all cursors should have been aborted already
  MANAGER->garbageCollect(/*abortAll*/ true);
}

void ManagerFeature::unprepare() { MANAGER.reset(); }

size_t ManagerFeature::streamingMaxTransactionSize() const noexcept {
  return _streamingMaxTransactionSize;
}

double ManagerFeature::streamingLockTimeout() const noexcept {
  return _streamingLockTimeout;
}

double ManagerFeature::streamingIdleTimeout() const noexcept {
  return _streamingIdleTimeout;
}

/*static*/ transaction::Manager* ManagerFeature::manager() noexcept {
  return MANAGER.get();
}

void ManagerFeature::queueGarbageCollection() {
  // The RequestLane needs to be something which is `HIGH` priority, otherwise
  // all threads executing this might be blocking, waiting for a lock to be
  // released.
  auto workItem = arangodb::SchedulerFeature::SCHEDULER->queueDelayed(
      "transactions-gc", RequestLane::CLUSTER_INTERNAL, std::chrono::seconds(2),
      _gcfunc);
  std::lock_guard<std::mutex> guard(_workItemMutex);
  _workItem = std::move(workItem);
}

void ManagerFeature::trackExpired(uint64_t numExpired) noexcept {
  if (numExpired > 0) {
    _numExpiredTransactions.count(numExpired);
  }
}

}  // namespace arangodb::transaction
