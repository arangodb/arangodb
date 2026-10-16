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
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "FoxxFeature.h"

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

FoxxFeature::FoxxFeature(application_features::ApplicationServer& server)
    : FoxxFeature(server, FoxxFeatureOptions{}) {}

FoxxFeature::FoxxFeature(application_features::ApplicationServer& server,
                         FoxxFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _queueVersion(0),
      _localQueueInserts(0) {
  setOptional(true);
  startsAfter<application_features::ServerFeaturePhase>();
}

void FoxxFeature::prepare() {
  if (!_options.foxxEnabled) {
    auto& ssf = server().getFeature<ServerSecurityFeature>();
    if (!ssf.isFoxxApiDisabled()) {
      ssf.disableFoxxApi();
      LOG_TOPIC("a19bd", WARN, Logger::FIXME)
          << "automatically disabling management APIs for Foxx, as access to "
             "Foxx apps is "
             "also turned off";
    }
  }
}

double FoxxFeature::pollInterval() const noexcept {
  if (!_options.queuesEnabled) {
    return -1.0;
  }
  return _options.queuesPollInterval;
}

bool FoxxFeature::startupWaitForSelfHeal() const noexcept {
  return _options.startupWaitForSelfHeal;
}

bool FoxxFeature::foxxEnabled() const noexcept { return _options.foxxEnabled; }

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
