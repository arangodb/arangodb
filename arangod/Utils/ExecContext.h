////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_UTILS_EXECCONTEXT_H
#define ARANGOD_UTILS_EXECCONTEXT_H 1

#include "VocBase/AuthInfo.h"

namespace arangodb {

enum class AuthLevel;

class AuthContext {
  public:
    AuthContext(AuthLevel authLevel) : _databaseAccess(authLevel) {}
    AuthLevel getDatabaseAuthLevel() { return _databaseAccess; }

  protected:
    AuthLevel _databaseAccess;
    std::map<std::string, AuthLevel> _collectionAccess;
};

class ExecContext {
  public:
    ExecContext(std::string const& user, std::string const& database,
    std::shared_ptr<AuthContext> authContext)
      : _user(user), _database(database) {
        _auth = authContext;
      }

  protected:
    std::string _user;
    std::string _database;
    std::shared_ptr<AuthContext> _auth;
};
}

#endif
