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
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/EndpointFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

AgencyFeature::AgencyFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Agency"),
      _size(1),
      _agentId((std::numeric_limits<uint32_t>::max)()),
      _minElectionTimeout(0.15),
      _maxElectionTimeout(2.0),
      _notify(false),
      _supervision(false),
      _waitForSync(true),
      _supervisionFrequency(5.0) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Database");
  startsAfter("Dispatcher");
  startsAfter("Endpoint");
  startsAfter("QueryRegistry");
  startsAfter("Random");
  startsAfter("Recovery");
  startsAfter("Scheduler");
  startsAfter("Server");
}

AgencyFeature::~AgencyFeature() {}

void AgencyFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("agency", "Configure the agency");

  options->addOption("--agency.size", "number of agents",
                     new UInt64Parameter(&_size));
  
  options->addOption("--agency.id", "this agent's id",
                     new UInt32Parameter(&_agentId));
  
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
  
  options->addOption("--agency.notify", "notify others",
                     new BooleanParameter(&_notify));
  
  options->addOption("--agency.supervision",
                     "perform arangodb cluster supervision",
                     new BooleanParameter(&_supervision));
  
  options->addOption("--agency.supervision-frequency",
                     "arangodb cluster supervision frequency [s]",
                     new DoubleParameter(&_supervisionFrequency));
  
  options->addHiddenOption("--agency.wait-for-sync",
                           "wait for hard disk syncs on every persistence call "
                           "(required in production)",
                           new BooleanParameter(&_waitForSync));
}

void AgencyFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_agentId == (std::numeric_limits<uint32_t>::max)()) {
    disable();
    return;
  }

  // Agency size
  if (_size < 1) {
    LOG_TOPIC(FATAL, Logger::AGENCY)
      << "AGENCY: agency must have size greater 0";
    FATAL_ERROR_EXIT();
  }
  
  // Size needs to be odd
  if (_size % 2 == 0) {
    LOG_TOPIC(FATAL, Logger::AGENCY)
      << "AGENCY: agency must have odd number of members";
    FATAL_ERROR_EXIT();
  }
  
  // Id out of range
  if (_agentId >= _size) {
    LOG_TOPIC(FATAL, Logger::AGENCY) << "agency.id must not be larger than or "
                                     << "equal to agency.size";
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
  
  if (_maxElectionTimeout <= 2 * _minElectionTimeout) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "agency.election-timeout-max should probably be chosen longer!";
  }
}

void AgencyFeature::prepare() {
  _agencyEndpoints.resize(static_cast<size_t>(_size));
}

void AgencyFeature::start() {
  if (!isEnabled()) {
    return;
  }

  // TODO: Port this to new options handling
  std::string endpoint;
  std::string port = "8529";

  EndpointFeature* endpointFeature = 
    ApplicationServer::getFeature<EndpointFeature>("Endpoint");
  auto endpoints = endpointFeature->httpEndpoints();
  
  if (!endpoints.empty()) {
    size_t pos = endpoint.find(':', 10);
    
    if (pos != std::string::npos) {
      port = endpoint.substr(pos + 1, endpoint.size() - pos);
    }
  }
  
  endpoint = std::string("tcp://localhost:" + port);
  
  _agent.reset( new consensus::Agent(
    consensus::config_t(_agentId, _minElectionTimeout, _maxElectionTimeout,
                        endpoint, _agencyEndpoints, _notify, _supervision,
                        _waitForSync, _supervisionFrequency)));
  
  _agent->start();
  _agent->load();
}

void AgencyFeature::stop() {
  if (!isEnabled()) {
    return;
  }
  
  _agent->beginShutdown();
}
