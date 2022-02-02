////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include "FoxxFeature.h"

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"

#include <shared_mutex>

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

FoxxFeature::FoxxFeature(Server& server)
    : ArangodFeature{server, *this},
      _queueVersion(0),
      _localQueueInserts(0),
      _pollInterval(1.0),
      _enabled(true),
      _startupWaitForSelfHeal(false) {
  setOptional(true);
  startsAfter<application_features::ServerFeaturePhase>();
}

void FoxxFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("foxx", "Foxx services");

  options->addOldOption("server.foxx-queues", "foxx.queues");
  options->addOldOption("server.foxx-queues-poll-interval",
                        "foxx.queues-poll-interval");

  options->addOption("--foxx.queues", "enable Foxx queues",
                     new BooleanParameter(&_enabled),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--foxx.queues-poll-interval",
                     "poll interval (in seconds) for Foxx queue manager",
                     new DoubleParameter(&_pollInterval),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options
      ->addOption("--foxx.force-update-on-startup",
                  "ensure all Foxx services are synchronized before "
                  "completeing the boot sequence",
                  new BooleanParameter(&_startupWaitForSelfHeal),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30705);
}

void FoxxFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // use a minimum for the interval
  if (_pollInterval < 0.1) {
    _pollInterval = 0.1;
  }
}

bool FoxxFeature::startupWaitForSelfHeal() const {
  return _startupWaitForSelfHeal;
}

uint64_t FoxxFeature::queueVersion() const noexcept {
  std::shared_lock lock(_queueLock);
  return _queueVersion;
}

uint64_t FoxxFeature::setQueueVersion(uint64_t version) noexcept {
  std::unique_lock lock(_queueLock);
  if (version > _queueVersion) {
    _queueVersion = version;
  }
  return _queueVersion;
}

void FoxxFeature::trackLocalQueueInsert() noexcept {
  std::unique_lock lock(_queueLock);
  ++_localQueueInserts;
}

void FoxxFeature::bumpQueueVersionIfRequired() {
  uint64_t localQueueInserts = 0;
  // fetch value from _localQueueInserts and set it to 0 under the lock
  {
    std::unique_lock lock(_queueLock);
    localQueueInserts = _localQueueInserts;
    _localQueueInserts = 0;
  }

  bool success = true;
  // if any queue updates have been posted on this coordinator, inform
  // other coordinators about it by increasing the shared counter in the
  // agency.
  if (localQueueInserts > 0) {
    try {
      // this is a magic constant, but there seems little value in making it
      // configurable. if we can't contact the agency within 10 seconds,
      // something seems wrong anyway and all sorts of other things will
      // start failing. if we set the timeout too low we may see a lot of
      // warnings in the log, which we also want to avoid.
      // if agency communication fails here, it is not a large problem, as
      // we will simply try to inform the agency in the next iteration. the
      // counter value is preserved in this case.
      constexpr double timeout = 10.0;

      AgencyComm agency(server());
      AgencyOperation incrementVersion("Sync/FoxxQueueVersion",
                                       AgencySimpleOperationType::INCREMENT_OP);
      AgencyWriteTransaction trx(incrementVersion);
      AgencyCommResult res = agency.sendTransactionWithFailover(trx, timeout);
      success = res.successful();
    } catch (std::exception const& ex) {
      LOG_TOPIC("a80c9", WARN, Logger::FIXME)
          << "unable to send Foxx queue update status to agency: " << ex.what();
      success = false;
    }

    if (!success) {
      // if updating the shared counter in the agency failed, we restore
      // the previous value to our counter. we intentionally use += here
      // because new queue jobs may have been posted in the meantime.
      std::unique_lock lock(_queueLock);
      _localQueueInserts += localQueueInserts;
    }
  }
}

}  // namespace arangodb
