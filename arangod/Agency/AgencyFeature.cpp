////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Agency/Agent.h"
#include "Agency/Job.h"
#include "Agency/Supervision.h"
#include "Cluster/ClusterFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/EndpointFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

consensus::Agent* AgencyFeature::AGENT = nullptr;

AgencyFeature::AgencyFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Agency"),
      _activated(false),
      _size(1),
      _poolSize(1),
      _minElectionTimeout(1.0),
      _maxElectionTimeout(5.0),
      _supervision(false),
      _waitForSync(true),
      _supervisionFrequency(1.0),
      _compactionStepSize(20000),
      _compactionKeepSize(10000),
      _maxAppendSize(250),
      _supervisionGracePeriod(10.0),
      _cmdLineTimings(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Cluster");
  startsAfter("Database");
  startsAfter("Endpoint");
  startsAfter("QueryRegistry");
  startsAfter("Random");
  startsAfter("Scheduler");
  startsAfter("Server");
}

AgencyFeature::~AgencyFeature() {}

void AgencyFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("agency", "Configure the agency");

  options->addOption("--agency.activate", "Activate agency",
                     new BooleanParameter(&_activated));

  options->addOption("--agency.size", "number of agents",
                     new UInt64Parameter(&_size));

  options->addOption("--agency.pool-size", "number of agent pool",
                     new UInt64Parameter(&_poolSize));

  options->addOption(
      "--agency.election-timeout-min",
      "minimum timeout before an agent calls for new election [s]",
      new DoubleParameter(&_minElectionTimeout));

  options->addOption(
      "--agency.election-timeout-max",
      "maximum timeout before an agent calls for new election [s]",
      new DoubleParameter(&_maxElectionTimeout));

  options->addOption("--agency.endpoint", "agency endpoints",
                     new VectorParameter<StringParameter>(&_agencyEndpoints));

  options->addOption("--agency.my-address",
                     "which address to advertise to the outside",
                     new StringParameter(&_agencyMyAddress));

  options->addOption("--agency.supervision",
                     "perform arangodb cluster supervision",
                     new BooleanParameter(&_supervision));

  options->addOption("--agency.supervision-frequency",
                     "arangodb cluster supervision frequency [s]",
                     new DoubleParameter(&_supervisionFrequency));

  options->addOption(
      "--agency.supervision-grace-period",
      "supervision time, after which a server is considered to have failed [s]",
      new DoubleParameter(&_supervisionGracePeriod));

  options->addHiddenOption("--agency.compaction-step-size",
                           "step size between state machine compactions",
                           new UInt64Parameter(&_compactionStepSize));

  options->addOption("--agency.compaction-keep-size",
                     "keep as many indices before compaction point",
                     new UInt64Parameter(&_compactionKeepSize));

  options->addHiddenOption("--agency.wait-for-sync",
                           "wait for hard disk syncs on every persistence call "
                           "(required in production)",
                           new BooleanParameter(&_waitForSync));

  options->addHiddenOption("--agency.max-append-size",
                           "maximum size of appendEntries document (# log entries)",
                           new UInt64Parameter(&_maxAppendSize));

}

void AgencyFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  ProgramOptions::ProcessingResult const& result = options->processingResult();

  if (!result.touched("agency.activate") || !_activated) {
    disable();
    return;
  }

  if (result.touched("agency.election-timeout-min")) {
    _cmdLineTimings = true;
  }

  ServerState::instance()->setRole(ServerState::ROLE_AGENT);

  // Agency size
  if (result.touched("agency.size")) {
    if (_size < 1) {
      LOG_TOPIC(FATAL, Logger::AGENCY) << "agency must have size greater 0";
      FATAL_ERROR_EXIT();
    }
  } else {
    _size = 1;
  }

  // Agency pool size
  if (result.touched("agency.pool-size")) {
    if (_poolSize < _size) {
      LOG_TOPIC(FATAL, Logger::AGENCY)
          << "agency pool size must be larger than agency size.";
      FATAL_ERROR_EXIT();
    }
  } else {
    _poolSize = _size;
  }

  // Size needs to be odd
  if (_size % 2 == 0) {
    LOG_TOPIC(FATAL, Logger::AGENCY)
        << "AGENCY: agency must have odd number of members";
    FATAL_ERROR_EXIT();
  }

  // Timeouts sanity
  if (_minElectionTimeout <= 0.) {
    LOG_TOPIC(FATAL, Logger::AGENCY)
        << "agency.election-timeout-min must not be negative!";
    FATAL_ERROR_EXIT();
  } else if (_minElectionTimeout < 0.15) {
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "very short agency.election-timeout-min!";
  }

  if (_maxElectionTimeout <= _minElectionTimeout) {
    LOG_TOPIC(FATAL, Logger::AGENCY)
        << "agency.election-timeout-max must not be shorter than or"
        << "equal to agency.election-timeout-min.";
    FATAL_ERROR_EXIT();
  }

  if (_maxElectionTimeout <= 2. * _minElectionTimeout) {
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "agency.election-timeout-max should probably be chosen longer!"
        << " " << __FILE__ << __LINE__;
  }

  if (_compactionKeepSize == 0) {
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "agency.compaction-keep-size must not be 0, set to 1000";
    _compactionKeepSize = 1000;
  }

  if (!_agencyMyAddress.empty()) {
    std::string const unified = Endpoint::unifiedForm(_agencyMyAddress);

    if (unified.empty()) {
      LOG_TOPIC(FATAL, Logger::AGENCY) << "invalid endpoint '"
                                       << _agencyMyAddress
                                       << "' specified for --agency.my-address";
      FATAL_ERROR_EXIT();
    }
  }
}

void AgencyFeature::prepare() {
}

void AgencyFeature::start() {
  if (!isEnabled()) {
    return;
  }

  // Find the agency prefix:
  auto feature = ApplicationServer::getFeature<ClusterFeature>("Cluster");
  if (!feature->agencyPrefix().empty()) {
    arangodb::consensus::Supervision::setAgencyPrefix(
      std::string("/") + feature->agencyPrefix());
    arangodb::consensus::Job::agencyPrefix
      = std::string("/") + feature->agencyPrefix();
  }
  
  // TODO: Port this to new options handling
  std::string endpoint;

  if (_agencyMyAddress.empty()) {
    std::string port = "8529";

    EndpointFeature* endpointFeature =
        ApplicationServer::getFeature<EndpointFeature>("Endpoint");
    auto endpoints = endpointFeature->httpEndpoints();

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
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Agency endpoint " << endpoint;

  if (_waitForSync) {
    _maxAppendSize /= 10;
  }

  _agent.reset(new consensus::Agent(consensus::config_t(
      _size, _poolSize, _minElectionTimeout, _maxElectionTimeout, endpoint,
      _agencyEndpoints, _supervision, _waitForSync, _supervisionFrequency,
      _compactionStepSize, _compactionKeepSize, _supervisionGracePeriod,
      _cmdLineTimings, _maxAppendSize)));

  AGENT = _agent.get();

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting agency personality";
  _agent->start();

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Loading agency";
  _agent->load();
}

void AgencyFeature::beginShutdown() {
  if (!isEnabled()) {
    return;
  }

  // pass shutdown event to _agent so it can notify all its sub-threads
  _agent->beginShutdown();
}

void AgencyFeature::stop() {
  if (!isEnabled()) {
    return;
  }

  if (_agent->inception() != nullptr) { // can only exist in resilient agents
    int counter = 0;
    while (_agent->inception()->isRunning()) {
      usleep(100000);
      // emit warning after 5 seconds
      if (++counter == 10 * 5) {
        LOG_TOPIC(WARN, Logger::AGENCY) << "waiting for inception thread to finish";
      }
    }
  }  

  if (_agent != nullptr) {
    int counter = 0;
    while (_agent->isRunning()) {
      usleep(100000);
      // emit warning after 5 seconds
      if (++counter == 10 * 5) {
        LOG_TOPIC(WARN, Logger::AGENCY) << "waiting for agent thread to finish";
      }
    }
  }

  AGENT = nullptr;
}
