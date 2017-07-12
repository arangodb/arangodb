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
#include "VocBase/vocbase.h"

using namespace arangodb;

thread_local ExecContext* ExecContext::CURRENT_EXECCONTEXT = nullptr;

AuthContext::AuthContext(
    std::string const& database, AuthLevel authLevel,
    std::unordered_map<std::string, AuthLevel>&& collectionAccess)
    : _isSystemDB(database == TRI_VOC_SYSTEM_DATABASE),
      _databaseAuthLevel(authLevel),
      _systemAuthLevel(AuthLevel::NONE),
      _collectionAccess(std::move(collectionAccess)) {}

AuthLevel AuthContext::collectionAuthLevel(
    std::string const& collectionName) const {
  // disallow access to _system/_users for everyone
  if (collectionName.empty()) {
    return AuthLevel::NONE;
  } else if (_isSystemDB && collectionName == TRI_COL_NAME_USERS) {
    return AuthLevel::NONE;
  }

  AuthLevel lvl = AuthLevel::NONE;
  auto const& it = _collectionAccess.find(collectionName);
  if (it != _collectionAccess.end()) {
    lvl = it->second;
  } else {
    auto const& it2 = _collectionAccess.find("*");
    if (it2 != _collectionAccess.end()) {
      lvl = it2->second;
    }
  }

  if (!collectionName.empty() && collectionName[0] == '_') {
    if (collectionName == "_frontend") {
      return AuthLevel::RW;
    } else if (lvl == AuthLevel::NONE) {
      lvl = AuthLevel::RO;  // at least ro for all system collections
    }
  }
  return lvl;
}

bool AuthContext::hasSpecificCollection(
    std::string const& collectionName) const {
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
