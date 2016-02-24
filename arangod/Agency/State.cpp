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

#include "State.h"

#include <chrono>
#include <thread>

using namespace arangodb::consensus;

State::State() : Thread("State") {}
State::~State() {

}

void State::configure(Agent* agent) {
  _agent = agent;
}

void State::respHandler (index_t idx) {
  // Handle responses
  
}

bool State::operator()(ClusterCommResult* ccr) {
  
};

query_ret_t State::write (Slice const&) {
  
};

query_ret_t State::read (Slice const&) const {
};

void State::run() {
  while (true) {
    std::this_thread::sleep_for(duration_t(1.0));
  }
}
  


