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

#include "Constituent.h"

using namespace arangodb::consensus;

Constituent::Constituent () :
  _term(0), _gen(std::random_device()()), _mode(FOLLOWER) {
}

Constituent::Constituent (const uint32_t n) :
  _mode(FOLLOWER), _votes(std::vector<bool>(n)) {
}

Constituent::Constituent (const constituency_t& constituency) :
  _gen(std::random_device()()), _mode(FOLLOWER) {
}

Constituent::~Constituent() {}

Constituent::duration_t Constituent::sleepFor () {
  dist_t dis(0., 1.);
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
	if (id == _id)
		return false;
	else {
		if (term > _term) {
			_state = FOLLOWER;
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

void Constituent::callElection() {}




