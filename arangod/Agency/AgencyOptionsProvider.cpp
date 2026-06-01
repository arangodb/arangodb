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

#include "AgencyOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <limits>

using namespace arangodb::options;

namespace arangodb {

void AgencyOptionsProvider::declareOptions(std::shared_ptr<ProgramOptions> opts,
                                           AgencyOptions& options) {
  opts->addSection("agency", "agency");

  opts->addOption("--agency.activate", "Activate the Agency.",
                  new BooleanParameter(&options.activated),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.size", "The number of Agents.",
                  new UInt64Parameter(&options.size),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.pool-size", "The number of Agents in the pool.",
                  new UInt64Parameter(&options.poolSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setDeprecatedIn(31100);

  opts->addOption(
      "--agency.election-timeout-min",
      "The minimum timeout before an Agent calls for a new election (in "
      "seconds).",
      new DoubleParameter(&options.minElectionTimeout, /*base*/ 1.0,
                          /*minValue*/ 0.0,
                          /*maxValue*/ std::numeric_limits<double>::max(),
                          /*minInclusive*/ false),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.election-timeout-max",
                  "The maximum timeout before an Agent calls for a new "
                  "election (in seconds).",
                  new DoubleParameter(&options.maxElectionTimeout),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent));

  opts->addOption(
      "--agency.endpoint", "The Agency endpoints.",
      new VectorParameter<StringParameter>(&options.agencyEndpoints),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.my-address",
                  "Which address to advertise to the outside.",
                  new StringParameter(&options.agencyMyAddress),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.supervision",
                  "Perform ArangoDB cluster supervision.",
                  new BooleanParameter(&options.supervision),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.supervision-frequency",
                  "The ArangoDB cluster supervision frequency (in seconds).",
                  new DoubleParameter(&options.supervisionFrequency),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.supervision-grace-period",
                  "The supervision time after which a server is considered to "
                  "have failed (in seconds).",
                  new DoubleParameter(&options.supervisionGracePeriod),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setLongDescription(R"(A value of `10` seconds is recommended for regular
cluster deployments.)");

  opts->addOption("--agency.supervision-ok-threshold",
                  "The supervision time after which a server is considered "
                  "to be bad (in seconds).",
                  new DoubleParameter(&options.supervisionOkThreshold),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent));

  opts->addOption(
          "--agency.supervision-expired-servers-grace-period",
          "The supervision time after which a server is removed "
          "from the agency if it does no longer send heartbeats "
          "(in seconds).",
          new DoubleParameter(&options.supervisionExpiredServersGracePeriod),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent))
      .setIntroducedIn(31204);

  opts->addOption("--agency.supervision-delay-add-follower",
                  "The delay in supervision, before an AddFollower job is "
                  "executed (in seconds).",
                  new UInt64Parameter(&options.supervisionDelayAddFollower),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002);

  opts->addOption("--agency.supervision-delay-failed-follower",
                  "The delay in supervision, before a FailedFollower job is "
                  "executed (in seconds).",
                  new UInt64Parameter(&options.supervisionDelayFailedFollower),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002);

  opts->addOption("--agency.supervision-failed-leader-adds-follower",
                  "Flag indicating whether or not the FailedLeader job adds a "
                  "new follower.",
                  new BooleanParameter(&options.failedLeaderAddsFollower),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setIntroducedIn(30907)
      .setIntroducedIn(31002);

  opts->addOption("--agency.compaction-step-size",
                  "The step size between state machine compactions.",
                  new UInt64Parameter(&options.compactionStepSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.compaction-keep-size",
                  "Keep as many Agency log entries before compaction point.",
                  new UInt64Parameter(&options.compactionKeepSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.wait-for-sync",
                  "Wait for hard disk syncs on every persistence call "
                  "(required in production).",
                  new BooleanParameter(&options.waitForSync),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::OnAgent));

  opts->addOption(
      "--agency.max-append-size",
      "The maximum size of appendEntries document (number of log entries).",
      new UInt64Parameter(&options.maxAppendSize),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::OnAgent));

  opts->addOption("--agency.disaster-recovery-id",
                  "Specify the ID for this agent. WARNING: This is a "
                  "dangerous option, for disaster recover only!",
                  new StringParameter(&options.recoveryId),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::OnAgent));
}

void AgencyOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> opts, AgencyOptions& options) {
  auto const& result = opts->processingResult();

  if (result.touched("agency.size")) {
    if (options.size < 1) {
      LOG_TOPIC("98510", FATAL, Logger::AGENCY)
          << "agency must have size greater 0";
      FATAL_ERROR_EXIT();
    }
  } else {
    options.size = 1;
  }

  if (result.touched("agency.pool-size") && options.poolSize != options.size) {
    // using a pool size different to the number of agents
    // has never been implemented properly, so bail out early here.
    LOG_TOPIC("af108", FATAL, Logger::AGENCY)
        << "agency pool size is deprecated and is not expected to be set";
    FATAL_ERROR_EXIT();
  }
  options.poolSize = options.size;

  // Size needs to be odd
  if (options.size % 2 == 0) {
    LOG_TOPIC("0eab5", FATAL, Logger::AGENCY)
        << "AGENCY: agency must have odd number of members";
    FATAL_ERROR_EXIT();
  }

  if (options.minElectionTimeout < 0.15) {
    LOG_TOPIC("0cce9", WARN, Logger::AGENCY)
        << "very short agency.election-timeout-min!";
  }

  if (options.maxElectionTimeout <= options.minElectionTimeout) {
    LOG_TOPIC("62fc3", FATAL, Logger::AGENCY)
        << "agency.election-timeout-max must not be shorter than or"
        << "equal to agency.election-timeout-min.";
    FATAL_ERROR_EXIT();
  }

  if (options.maxElectionTimeout <= 2. * options.minElectionTimeout) {
    LOG_TOPIC("99f84", WARN, Logger::AGENCY)
        << "agency.election-timeout-max should probably be chosen longer!";
  }

  if (options.compactionKeepSize == 0) {
    LOG_TOPIC("ca485", WARN, Logger::AGENCY)
        << "agency.compaction-keep-size must not be 0, set to 50000";
    options.compactionKeepSize = 50000;
  }

  if (result.touched("agency.supervision")) {
    options.supervisionTouched = true;
  }
}

}  // namespace arangodb
