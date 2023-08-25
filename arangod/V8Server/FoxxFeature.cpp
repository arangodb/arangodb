////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

FoxxFeature::FoxxFeature(Server& server)
    : ArangodFeature{server, *this},
      _queueVersion(0),
      _localQueueInserts(0),
      _queuesPollInterval(1.0),
      _queuesEnabled(true),
      _startupWaitForSelfHeal(false),
      _foxxEnabled(true) {
  setOptional(true);
  startsAfter<application_features::ServerFeaturePhase>();
}

void FoxxFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("foxx", "Foxx services");

  options->addOldOption("server.foxx-queues", "foxx.queues");
  options->addOldOption("server.foxx-queues-poll-interval",
                        "foxx.queues-poll-interval");

  options
      ->addOption("--foxx.queues", "Enable or disable Foxx queues.",
                  new BooleanParameter(&_queuesEnabled),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If set to `true`, the Foxx queues are available
and jobs in the queues are executed asynchronously.

If set to `false`, the queue manager is disabled and any jobs are prevented from
being processed, which may reduce CPU load a bit.)");

  options
      ->addOption("--foxx.queues-poll-interval",
                  "The poll interval for the Foxx queue manager (in seconds)",
                  new DoubleParameter(&_queuesPollInterval),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Lower values lead to more immediate and more
frequent Foxx queue job execution, but make the queue thread wake up and query
the queues more often. If set to a low value, the queue thread might cause
CPU load.

If you don't use Foxx queues much, then you may increase this value to make the
queues thread wake up less.)");

  options
      ->addOption("--foxx.force-update-on-startup",
                  "Ensure that all Foxx services are synchronized before "
                  "completing the startup sequence.",
                  new BooleanParameter(&_startupWaitForSelfHeal),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If set to `true`, all Foxx services in all
databases are synchronized between multiple Coordinators during the startup
sequence. This ensures that all Foxx services are up-to-date when a Coordinator
reports itself as ready.

In case the option is set to `false` (i.e. no waiting), the Coordinator
completes the startup sequence faster, and the Foxx services are propagated
lazily. Until the initialization procedure has completed for the local Foxx
apps, any request to a Foxx app is responded to with an HTTP 500 error and a
message `waiting for initialization of Foxx services in this database`. This can
cause an unavailability window for Foxx services on Coordinator startup for the
initial requests to Foxx apps until the app propagation has completed.

If you don't use Foxx, you should set this option to `false` to benefit from a
faster Coordinator startup. Deployments relying on Foxx apps being available as
soon as a Coordinator is integrated or responding should set this option to
`true`.

The option only has an effect for cluster setups. On single servers and in
Active Failover mode, all Foxx apps are available from the very beginning.

**Note**: ArangoDB 3.8 changes the default value to `false` for this option.
In previous versions, this option had a default value of `true`.)");

  options
      ->addOption("--foxx.enable", "Enable Foxx.",
                  new BooleanParameter(&_foxxEnabled),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31005)
      .setLongDescription(R"(If set to `false`, access to any custom Foxx 
services in the deployment will be forbidden. Access to ArangoDB's built-in
web interface will still be possible though.

**Note**: when setting this option to `false`, the management API for Foxx
services will automatically be disabled as well. This is the same as manually
setting the startup option `--foxx.api false`.)");
}

void FoxxFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // use a minimum for the interval
  if (_queuesPollInterval < 0.1) {
    _queuesPollInterval = 0.1;
  }
}

void FoxxFeature::prepare() {
  if (!_foxxEnabled) {
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
  if (!_queuesEnabled) {
    return -1.0;
  }
  return _queuesPollInterval;
}

bool FoxxFeature::startupWaitForSelfHeal() const noexcept {
  return _startupWaitForSelfHeal;
}

bool FoxxFeature::foxxEnabled() const noexcept { return _foxxEnabled; }

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
