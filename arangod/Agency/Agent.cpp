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

#include "Agent.h"

using namespace arangodb::consensus;

Agent::Agent () {
	_constituent.start();
}

Agent::~Agent () {
	_constituent.stop();
}

Constituent::term_t Agent::term () const {
  return _constituent.term();
}

bool Agent::vote(Constituent::id_t id, Constituent::term_t term) {
	return _constituent.vote(id, term);
}

Slice const& Agent::log (const Slice& slice) {
  if (_constituent.leader())
    return _log.log(slice);
  else
    return redirect(slice);
}

Slice const& Agent::redirect (const Slice& slice) {
  return slice; //TODO
}
