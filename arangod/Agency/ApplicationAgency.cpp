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

#include "Basics/Logger.h"
#include "Scheduler/PeriodicTask.h"

#include "ApplicationAgency.h"

using namespace std;
using namespace arangodb::basics;
using namespace arangodb::rest;

ApplicationAgency::ApplicationAgency()
  : ApplicationFeature("agency"), _size(1), _min_election_timeout(.5),
	  _max_election_timeout(1.), _election_call_rate_mul(.75),
    _append_entries_retry_interval(1.0),
    _agent_id(std::numeric_limits<uint32_t>::max()) {
  
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
    ("agency.append_entries_retry_interval [s]", &_append_entries_retry_interval,
     "Interval at which appendEntries are attempted on unresponsive slaves"
     "in seconds");
}



bool ApplicationAgency::prepare() {
  
  if (_disabled) {
    return true;
  }

  if (_size < 1)
    LOG(FATAL) << "agency must have size greater 0";

  if (_agent_id == std::numeric_limits<uint32_t>::max())
    LOG(FATAL) << "agency.id must be specified";

  if (_min_election_timeout <= 0.) {
    LOG(FATAL) << "agency.election-timeout-min must not be negative!";
  } else if (_min_election_timeout < .15) {
    LOG(WARN)  << "very short agency.election-timeout-min!";
  }
  
  if (_max_election_timeout <= _min_election_timeout) {
    LOG(FATAL) << "agency.election-timeout-max must not be shorter than or"
               << "equal to agency.election-timeout-min.";
  }
  
  if (_max_election_timeout <= 2*_min_election_timeout) {
    LOG(WARN)  << "agency.election-timeout-max should probably be chosen longer!";
  }
  
  _agency_endpoints.resize(_size);
  std::iter_swap(_agency_endpoints.begin(),
                 _agency_endpoints.begin() + _agent_id);
  
  _agent = std::unique_ptr<agent_t>(
    new agent_t(config_t(
                  _agent_id, _min_election_timeout, _max_election_timeout,
                  _append_entries_retry_interval, _agency_endpoints)));
  
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



