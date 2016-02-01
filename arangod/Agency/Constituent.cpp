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

#include "Basics/logging.h"
#include "Cluster/ClusterComm.h"

#include "Constituent.h"

#include <chrono>
#include <thread>

using namespace arangodb::consensus;

Constituent::Constituent (const uint32_t n) : Thread("Constituent"),
	_term(0), _gen(std::random_device()()), _mode(FOLLOWER), _run(true),
	_votes(std::vector<bool>(n)) {

}

Constituent::~Constituent() {}

Constituent::duration_t Constituent::sleepFor () {
  dist_t dis(.15, .99);
  return Constituent::duration_t(dis(_gen));
}

Constituent::term_t Constituent::term() const {
  return _term;
}

void Constituent::runForLeaderShip (bool b) {
}

Constituent::state_t Constituent::state () const {
  return _state;
}

Constituent::mode_t Constituent::mode () const {
  return _mode;
}

bool Constituent::vote (id_t id, term_t term) {
	LOG_WARNING("(%d %d) (%d %d)", _id, _term, id, term);
	if (id == _id)
		return false;
	else {
		if (term > _term) {
			_state = FOLLOWER;
			_term = term;
			return true;
		} else {
			return false;
		}
	}
}

void Constituent::gossip (const Constituent::constituency_t& constituency) {
  // Talk to all peers i have not talked to
  // Answer by sending my complete list of peers
}

const Constituent::constituency_t& Constituent::gossip () {
  return _constituency;
}

bool Constituent::leader() const {
  return _mode==LEADER;
}

void Constituent::callElection() {
  
	std::unique_ptr<std::map<std::string, std::string>> headerFields;
  
	std::vector<ClusterCommResult> results(_constituency.size());
	for (auto& i : _constituency)
		results[i.id] = arangodb::ClusterComm::instance()->asyncRequest(
      0, 0, i.address, rest::HttpRequest::HTTP_REQUEST_GET,
      "/_api/agency/vote?id=0&term=3", nullptr, headerFields, nullptr,
      .75*.15);
	std::this_thread::sleep_for(sleepFor(.9*.15));
  for (auto i : _votes)
	  arangodb::ClusterComm::instance()->enquire(results[i.id].OperationID);
	// time out
	//count votes
}

void Constituent::run() {
  while (_run) {
    if (_state == FOLLOWER) {
      LOG_WARNING ("sleeping ... ");
      std::this_thread::sleep_for(sleepFor());
    } else {
      callElection();
    }
    
  }
};


