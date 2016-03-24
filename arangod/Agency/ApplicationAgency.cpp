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

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Logger/Logger.h"
#include "Scheduler/PeriodicTask.h"

#include "ApplicationAgency.h"

using namespace std;
using namespace arangodb::basics;
using namespace arangodb::rest;

ApplicationAgency::ApplicationAgency()
  : ApplicationFeature("agency"), _size(1), _min_election_timeout(0.1),
	  _max_election_timeout(1.0), _election_call_rate_mul(0.85), _notify(false),
    _agent_id((std::numeric_limits<uint32_t>::max)()) {
}


ApplicationAgency::~ApplicationAgency() {}


////////////////////////////////////////////////////////////////////////////////
/// @brief sets the processor affinity
////////////////////////////////////////////////////////////////////////////////

void ApplicationAgency::setupOptions(
    std::map<std::string, ProgramOptionsDescription>& options) {
  options["Agency Options:help-agency"]("agency.size", &_size, "Agency size")
    ("agency.id", &_agent_id, "This agent's id")
		("agency.election-timeout-min", &_min_election_timeout, "Minimum "
		 "timeout before an agent calls for new election [s]")
		("agency.election-timeout-max", &_max_election_timeout, "Minimum "
		 "timeout before an agent calls for new election [s]")
		("agency.endpoint", &_agency_endpoints, "Agency endpoints")
    ("agency.election_call_rate_mul [au]", &_election_call_rate_mul,
     "Multiplier (<1.0) defining how long the election timeout is with respect "
     "to the minumum election timeout")
    ("agency.notify", &_notify, "Notify others [beta :)]");
}


bool ApplicationAgency::prepare() {

  _disabled = (_agent_id == (std::numeric_limits<uint32_t>::max)());

  if (_disabled) {
    return true;
  }

  if (_size < 1) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "AGENCY: agency must have size greater 0";
    return false;    
  }
  

  if (_size % 2 == 0) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "AGENCY: agency must have odd number of members";
    return false;
  }
  
  if (_agent_id == (std::numeric_limits<uint32_t>::max)()) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "agency.id must be specified";
    return false;
  }

  if (_min_election_timeout <= 0.) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "agency.election-timeout-min must not be negative!";
    return false;
  } else if (_min_election_timeout < .15) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "very short agency.election-timeout-min!";
  }
  
  if (_max_election_timeout <= _min_election_timeout) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "agency.election-timeout-max must not be shorter than or"
      << "equal to agency.election-timeout-min.";
    return false;
  }
  
  if (_max_election_timeout <= 2*_min_election_timeout) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "agency.election-timeout-max should probably be chosen longer!";
  }
  
  _agency_endpoints.resize(_size);

  _agent = std::unique_ptr<agent_t>(
    new agent_t(arangodb::consensus::config_t(
                  _agent_id, _min_election_timeout, _max_election_timeout,
                  _agency_endpoints, _notify)));
  
  return true;
  
}


bool ApplicationAgency::start() {
  if (_disabled) {
    return true;
  }
  _agent->start();
  return true;
}


bool ApplicationAgency::open() { return true; }


void ApplicationAgency::close() {
  if (_disabled) {
    return;
  }
}


void ApplicationAgency::stop() {
  if (_disabled) {
    return;
  }
  _agent->beginShutdown();
}

agent_t* ApplicationAgency::agent () const {
  return _agent.get();
}



