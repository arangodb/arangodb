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
    : ApplicationFeature("agency"), _size(5), _min_election_timeout(.15),
	  _max_election_timeout(.3) /*, _agent(new consensus::Agent())*/{
	//arangodb::consensus::Config<double> config (5, .15, .9);
	//_agent = std::uniqe_ptr<agent>(new config);

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
		("agency.endpoint", &_agency_endpoints, "Agency endpoints");
}




bool ApplicationAgency::prepare() {
  if (_disabled) {
    return true;
  }
  _agent = std::unique_ptr<consensus::Agent>(new consensus::Agent(
     consensus::Config<double>(_agent_id, _min_election_timeout,
       _max_election_timeout, _agency_endpoints)));
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
}

arangodb::consensus::Agent* ApplicationAgency::agent () const {
  return _agent.get();
}



