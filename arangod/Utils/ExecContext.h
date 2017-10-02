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
namespace transaction {
class Methods;
}

class ExecContext {
  ExecContext(bool isSuper, std::string const& user, std::string const& database,
              AuthLevel systemLevel, AuthLevel dbLevel)
      : _isSuperUser(isSuper),
        _user(user),
        _database(database),
        _systemDbAuthLevel(systemLevel),
        _databaseAuthLevel(dbLevel) {
          TRI_ASSERT(!_isSuperUser || _user.empty());
        }
  ExecContext(ExecContext const&) = delete;

 public:
  /// Should always contain a reference to current user context
  static thread_local ExecContext const* CURRENT;

  /// @brief a reference to a user with NONE for everything
  static ExecContext* createUnauthorized(std::string const& user,
                                         std::string const& db);
  /// @brief an internal system user
  static ExecContext* createSystem(std::string const& db);
  static ExecContext* create(std::string const& user, std::string const& db);

  /// @brief should always be owned externally, so copy it here
  // ExecContext* copy() const;

  std::string const& user() const { return _user; }
  std::string const& database() const { return _database; }
  AuthLevel systemAuthLevel() const { return _systemDbAuthLevel; };
  AuthLevel databaseAuthLevel() const { return _databaseAuthLevel; };

  bool isSuperUser() const { return _isSuperUser; }
  bool isSystemUser() const {
    return _isSuperUser || _systemDbAuthLevel == AuthLevel::RW;
  }

  bool canUseDatabase(AuthLevel requested) const {
    return canUseDatabase(_database, requested);
  }
  bool canUseDatabase(std::string const& db, AuthLevel requested) const;
  bool canUseCollection(std::string const& c, AuthLevel requested) const {
    return canUseCollection(_database, c, requested);
  }
  bool canUseCollection(std::string const& db, std::string const& c,
                        AuthLevel requested) const;

 private:
  bool _isSuperUser = false;
  std::string const _user;
  std::string const _database;
  AuthLevel const _systemDbAuthLevel;
  AuthLevel const _databaseAuthLevel;
};

/// @brief scope guard for the exec context
struct ExecContextScope {
  ExecContextScope() : _old(ExecContext::CURRENT) {
    ExecContext::CURRENT = nullptr;
  }

  explicit ExecContextScope(ExecContext const* exe)
      : _old(ExecContext::CURRENT) {
    TRI_ASSERT(ExecContext::CURRENT == nullptr);
    ExecContext::CURRENT = exe;
  }

  ~ExecContextScope() { ExecContext::CURRENT = _old; }

 private:
  ExecContext const* _old;
};
}

#endif
