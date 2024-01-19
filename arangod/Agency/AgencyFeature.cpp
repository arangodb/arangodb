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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "AgencyFeature.h"

#include "Actions/ActionFeature.h"
#include "Agency/Agent.h"
#include "Agency/Job.h"
#include "Agency/Supervision.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "Endpoint/Endpoint.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#ifdef USE_V8
#include "RestServer/FrontendFeature.h"
#include "RestServer/ScriptFeature.h"
#include "V8/V8PlatformFeature.h"
#include "V8Server/FoxxFeature.h"
#include "V8Server/V8DealerFeature.h"
#endif

#include <limits>

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace arangodb {

AgencyFeature::AgencyFeature(Server& server)
    : ArangodFeature{server, *this},
      _activated(false),
      _size(1),
      _poolSize(1),
      _minElectionTimeout(1.0),
      _maxElectionTimeout(5.0),
      _supervision(false),
      _supervisionTouched(false),
      _waitForSync(true),
      _supervisionFrequency(1.0),
      _compactionStepSize(1000),
      _compactionKeepSize(50000),
      _maxAppendSize(250),
      _supervisionGracePeriod(10.0),
      _supervisionOkThreshold(5.0),
      _supervisionDelayAddFollower(0),
      _supervisionDelayFailedFollower(0),
      _failedLeaderAddsFollower(true) {
  setOptional(true);
#ifdef USE_V8
  startsAfter<application_features::FoxxFeaturePhase>();
#else
  startsAfter<application_features::ServerFeaturePhase>();
#endif
}

AgencyFeature::~AgencyFeature() = default;

void AgencyFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("agency", "agency");

  options->addOption("--agency.activate", "Activate the Agency.",
                     new BooleanParameter(&_activated),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options->addOption("--agency.size", "The number of Agents.",
                     new UInt64Parameter(&_size),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options
      ->addOption("--agency.pool-size", "The number of Agents in the pool.",
                  new UInt64Parameter(&_poolSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setDeprecatedIn(31100);

  options->addOption(
      "--agency.election-timeout-min",
      "The minimum timeout before an Agent calls for a new election (in "
      "seconds).",
      new DoubleParameter(&_minElectionTimeout, /*base*/ 1.0, /*minValue*/ 0.0,
                          /*maxValue*/ std::numeric_limits<double>::max(),
                          /*minInclusive*/ false),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent));

  options->addOption("--agency.election-timeout-max",
                     "The maximum timeout before an Agent calls for a new "
                     "election (in seconds).",
                     new DoubleParameter(&_maxElectionTimeout),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options->addOption("--agency.endpoint", "The Agency endpoints.",
                     new VectorParameter<StringParameter>(&_agencyEndpoints),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options->addOption("--agency.my-address",
                     "Which address to advertise to the outside.",
                     new StringParameter(&_agencyMyAddress),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options->addOption("--agency.supervision",
                     "Perform ArangoDB cluster supervision.",
                     new BooleanParameter(&_supervision),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options->addOption("--agency.supervision-frequency",
                     "The ArangoDB cluster supervision frequency (in seconds).",
                     new DoubleParameter(&_supervisionFrequency),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options
      ->addOption("--agency.supervision-grace-period",
                  "The supervision time after which a server is considered to "
                  "have failed (in seconds).",
                  new DoubleParameter(&_supervisionGracePeriod),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setLongDescription(R"(A value of `10` seconds is recommended for regular
cluster deployments.)");

  options->addOption("--agency.supervision-ok-threshold",
                     "The supervision time after which a server is considered "
                     "to be bad (in seconds).",
                     new DoubleParameter(&_supervisionOkThreshold),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options
      ->addOption("--agency.supervision-delay-add-follower",
                  "The delay in supervision, before an AddFollower job is "
                  "executed (in seconds).",
                  new UInt64Parameter(&_supervisionDelayAddFollower),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002);

  options
      ->addOption("--agency.supervision-delay-failed-follower",
                  "The delay in supervision, before a FailedFollower job is "
                  "executed (in seconds).",
                  new UInt64Parameter(&_supervisionDelayFailedFollower),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002);

  options
      ->addOption("--agency.supervision-failed-leader-adds-follower",
                  "Flag indicating whether or not the FailedLeader job adds a "
                  "new follower.",
                  new BooleanParameter(&_failedLeaderAddsFollower),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent))
      .setIntroducedIn(30907)
      .setIntroducedIn(31002);

  options->addOption("--agency.compaction-step-size",
                     "The step size between state machine compactions.",
                     new UInt64Parameter(&_compactionStepSize),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::OnAgent));

  options->addOption("--agency.compaction-keep-size",
                     "Keep as many Agency log entries before compaction point.",
                     new UInt64Parameter(&_compactionKeepSize),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent));

  options->addOption("--agency.wait-for-sync",
                     "Wait for hard disk syncs on every persistence call "
                     "(required in production).",
                     new BooleanParameter(&_waitForSync),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::OnAgent));

  options->addOption(
      "--agency.max-append-size",
      "The maximum size of appendEntries document (number of log entries).",
      new UInt64Parameter(&_maxAppendSize),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::OnAgent));

  options->addOption("--agency.disaster-recovery-id",
                     "Specify the ID for this agent. WARNING: This is a "
                     "dangerous option, for disaster recover only!",
                     new StringParameter(&_recoveryId),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::OnAgent));
}

void AgencyFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  ProgramOptions::ProcessingResult const& result = options->processingResult();

  if (!result.touched("agency.activate") || !_activated) {
    disable();
    return;
  }

  ServerState::instance()->setRole(ServerState::ROLE_AGENT);

  // Agency size
  if (result.touched("agency.size")) {
    if (_size < 1) {
      LOG_TOPIC("98510", FATAL, Logger::AGENCY)
          << "agency must have size greater 0";
      FATAL_ERROR_EXIT();
    }
  } else {
    _size = 1;
  }

  // Agency pool size
  if (result.touched("agency.pool-size") && _poolSize != _size) {
    // using a pool size different to the number of agents
    // has never been implemented properly, so bail out early here.
    LOG_TOPIC("af108", FATAL, Logger::AGENCY)
        << "agency pool size is deprecated and is not expected to be set";
    FATAL_ERROR_EXIT();
  }
  _poolSize = _size;

  // Size needs to be odd
  if (_size % 2 == 0) {
    LOG_TOPIC("0eab5", FATAL, Logger::AGENCY)
        << "AGENCY: agency must have odd number of members";
    FATAL_ERROR_EXIT();
  }

  // Check Timeouts
  if (_minElectionTimeout < 0.15) {
    LOG_TOPIC("0cce9", WARN, Logger::AGENCY)
        << "very short agency.election-timeout-min!";
  }

  if (_maxElectionTimeout <= _minElectionTimeout) {
    LOG_TOPIC("62fc3", FATAL, Logger::AGENCY)
        << "agency.election-timeout-max must not be shorter than or"
        << "equal to agency.election-timeout-min.";
    FATAL_ERROR_EXIT();
  }

  if (_maxElectionTimeout <= 2. * _minElectionTimeout) {
    LOG_TOPIC("99f84", WARN, Logger::AGENCY)
        << "agency.election-timeout-max should probably be chosen longer!";
  }

  if (_compactionKeepSize == 0) {
    LOG_TOPIC("ca485", WARN, Logger::AGENCY)
        << "agency.compaction-keep-size must not be 0, set to 50000";
    _compactionKeepSize = 50000;
  }

  if (!_agencyMyAddress.empty()) {
    std::string const unified = Endpoint::unifiedForm(_agencyMyAddress);

    if (unified.empty()) {
      LOG_TOPIC("4faa0", FATAL, Logger::AGENCY)
          << "invalid endpoint '" << _agencyMyAddress
          << "' specified for --agency.my-address";
      FATAL_ERROR_EXIT();
    }

    std::string fallback = unified;
    // Now extract the hostname/IP:
    auto pos = fallback.find("://");
    if (pos != std::string::npos) {
      fallback = fallback.substr(pos + 3);
    }
    pos = fallback.rfind(':');
    if (pos != std::string::npos) {
      fallback.resize(pos);
    }
    auto ss = ServerState::instance();
    ss->findHost(fallback);
  }

  if (result.touched("agency.supervision")) {
    _supervisionTouched = true;
  }

  // turn off the following features, as they are not needed in an agency:
  // - ArangoSearch: not needed by agency
  // - IResearchAnalyzer: analyzers are not needed by agency
  // - Action/Script/FoxxQueues/Frontend: Foxx and JavaScript APIs
  {
    server().disableFeatures(std::array{
        ArangodServer::id<iresearch::IResearchFeature>(),
        ArangodServer::id<iresearch::IResearchAnalyzerFeature>(),
#ifdef USE_V8
        ArangodServer::id<FoxxFeature>(), ArangodServer::id<FrontendFeature>(),
#endif
        ArangodServer::id<ActionFeature>()});
  }

#ifdef USE_V8
  if (!V8DealerFeature::javascriptRequestedViaOptions(options)) {
    // specifying --console requires JavaScript, so we can only turn Javascript
    // off if not requested

    // console mode inactive. so we can turn off V8
    server().disableFeatures(std::array{ArangodServer::id<ScriptFeature>(),
                                        ArangodServer::id<V8PlatformFeature>(),
                                        ArangodServer::id<V8DealerFeature>()});
  }
#endif
}

void AgencyFeature::prepare() {
  TRI_ASSERT(isEnabled());

  // Available after validateOptions of ClusterFeature
  // Find the agency prefix:
  auto& feature = server().getFeature<ClusterFeature>();
  if (!feature.agencyPrefix().empty()) {
    arangodb::consensus::Supervision::setAgencyPrefix(std::string("/") +
                                                      feature.agencyPrefix());
    arangodb::consensus::Job::agencyPrefix = feature.agencyPrefix();
  }

  std::string endpoint;

  if (_agencyMyAddress.empty()) {
    std::string port = "8529";

    // Available after prepare of EndpointFeature
    HttpEndpointProvider& endpointFeature =
        server().getFeature<HttpEndpointProvider>();
    auto endpoints = endpointFeature.httpEndpoints();

    if (!endpoints.empty()) {
      std::string const& tmp = endpoints.front();
      size_t pos = tmp.find(':', 10);

      if (pos != std::string::npos) {
        port = tmp.substr(pos + 1, tmp.size() - pos);
      }
    }

    endpoint = std::string("tcp://localhost:" + port);
  } else {
    endpoint = _agencyMyAddress;
  }
  LOG_TOPIC("693a2", DEBUG, Logger::AGENCY) << "Agency endpoint " << endpoint;

  if (_waitForSync) {
    _maxAppendSize /= 10;
  }

  _agent = std::make_unique<consensus::Agent>(
      server(),
      consensus::config_t(
          _recoveryId, _size, _minElectionTimeout, _maxElectionTimeout,
          endpoint, _agencyEndpoints, _supervision, _supervisionTouched,
          _waitForSync, _supervisionFrequency, _compactionStepSize,
          _compactionKeepSize, _supervisionGracePeriod, _supervisionOkThreshold,
          _supervisionDelayAddFollower, _supervisionDelayFailedFollower,
          _failedLeaderAddsFollower, _maxAppendSize));
}

void AgencyFeature::start() {
  TRI_ASSERT(isEnabled());

  LOG_TOPIC("a77c8", DEBUG, Logger::AGENCY) << "Starting agency personality";
  _agent->start();

  LOG_TOPIC("b481d", DEBUG, Logger::AGENCY) << "Loading agency";
  _agent->load();
}

void AgencyFeature::beginShutdown() {
  TRI_ASSERT(isEnabled());

  // pass shutdown event to _agent so it can notify all its sub-threads
  _agent->beginShutdown();
}

void AgencyFeature::stop() {
  TRI_ASSERT(isEnabled());

  if (_agent->inception() != nullptr) {  // can only exist in resilient agents
    int counter = 0;
    while (_agent->inception()->isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // emit warning after 5 seconds
      if (++counter == 10 * 5) {
        LOG_TOPIC("bf6a6", WARN, Logger::AGENCY)
            << "waiting for inception thread to finish";
      }
    }
  }

  if (_agent != nullptr) {
    int counter = 0;
    while (_agent->isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // emit warning after 5 seconds
      if (++counter == 10 * 5) {
        LOG_TOPIC("5d3a5", WARN, Logger::AGENCY)
            << "waiting for agent thread to finish";
      }
    }

    // Wait until all agency threads have been shut down. Note that the
    // actual agent object is only destroyed in the destructor to allow
    // server jobs from RestAgencyHandlers to complete without incident:
    _agent->waitForThreadsStop();
  }
}

void AgencyFeature::unprepare() {
  TRI_ASSERT(isEnabled());
  // delete the Agent object here ensures it shuts down all of its threads
  // this is a precondition that it must fulfill before we can go on with the
  // shutdown
  _agent.reset();
}

consensus::Agent* AgencyFeature::agent() const { return _agent.get(); }

}  // namespace arangodb
