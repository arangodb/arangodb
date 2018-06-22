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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "CollectionLockState.h"
#include "Logger/Logger.h"

using namespace arangodb;

/// @brief if this pointer is set to an actual set, then for each request
/// sent to a shardId using the ClusterComm library, an X-Arango-Nolock
/// header is generated.
thread_local std::unordered_set<std::string>* CollectionLockState::_noLockHeaders =
    nullptr;
  
void CollectionLockState::setNoLockHeaders(std::unordered_set<std::string>* headers) {
  LOG_TOPIC(DEBUG, arangodb::Logger::COMMUNICATION) << "Setting nolock headers";
  _noLockHeaders = headers;
}


void CollectionLockState::clearNoLockHeaders() {
  LOG_TOPIC(DEBUG, arangodb::Logger::COMMUNICATION) << "Clearing nolock headers";
  _noLockHeaders = nullptr;
}

bool CollectionLockState::isLocked(std::string const& name) {
  if (_noLockHeaders != nullptr) {
    auto it = _noLockHeaders->find(name);
    if (it != _noLockHeaders->end()) {
      LOG_TOPIC(DEBUG, arangodb::Logger::COMMUNICATION) << name << " is locked by nolock header";
      return true;
    }
  }
  LOG_TOPIC(DEBUG, arangodb::Logger::COMMUNICATION) << name << " is not locked by nolock header";
  return false;
}
