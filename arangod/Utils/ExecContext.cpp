////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include "ExecContext.h"
#include "Logger/Logger.h"

using namespace arangodb;

thread_local ExecContext* ExecContext::CURRENT_EXECCONTEXT = nullptr;

AuthContext::AuthContext(
    AuthLevel authLevel,
    std::unordered_map<std::string, AuthLevel>&& collectionAccess)
    : _databaseAuthLevel(authLevel),
      _systemAuthLevel(AuthLevel::NONE),
      _collectionAccess(std::move(collectionAccess)) {}

AuthLevel AuthContext::collectionAuthLevel(
    std::string const& collectionName) const {
  auto const& it = _collectionAccess.find(collectionName);
  if (it != _collectionAccess.end()) {
    return it->second;
  }
  auto const& it2 = _collectionAccess.find("*");
  if (it2 != _collectionAccess.end()) {
    return it2->second;
  }
  return AuthLevel::NONE;
}

bool AuthContext::hasSpecificCollection(std::string const& collectionName) const {
  return _collectionAccess.find(collectionName) != _collectionAccess.end();
}

void AuthContext::dump() {
  LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION)
      << "Dump AuthContext rights";

  if (_databaseAuthLevel == AuthLevel::RO) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION) << "database level RO";
  }
  if (_databaseAuthLevel == AuthLevel::RW) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION) << "database level RW";
  }

  if (_systemAuthLevel == AuthLevel::RO) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION) << "_system level RO";
  }
  if (_systemAuthLevel == AuthLevel::RW) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION) << "_system level RW";
  }

  for (auto const& it : _collectionAccess) {
    if (it.second == AuthLevel::RO)
      LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION) << it.first << " RO";
    if (it.second == AuthLevel::RW)
      LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION) << it.first << " RW";
  }
}
