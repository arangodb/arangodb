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

#ifndef ARANGOD_UTILS_EXECCONTEXT_H
#define ARANGOD_UTILS_EXECCONTEXT_H 1

#include <string>
#include "Utils/Authentication.h"

namespace arangodb {

enum class AuthLevel;

class ExecContext {
 public:
  ExecContext(std::string const& user, std::string const& database,
              AuthLevel systemLevel, AuthLevel dbLevel)
      : _user(user),
        _database(database),
        _systemAuthLevel(systemLevel),
        _databaseAuthLevel(dbLevel) {}

  ExecContext(ExecContext const&) = delete;

  static thread_local ExecContext* CURRENT;

  std::string const& user() const { return _user; }
  std::string const& database() const { return _database; }
  AuthLevel systemAuthLevel() const { return _systemAuthLevel; };
  AuthLevel databaseAuthLevel() const { return _databaseAuthLevel; };
  ExecContext* copy() const;
  
 private:
  std::string const _user;
  std::string _database;
  AuthLevel const _systemAuthLevel;
  AuthLevel const _databaseAuthLevel;
};
}

#endif
