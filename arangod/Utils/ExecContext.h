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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_EXECCONTEXT_H
#define ARANGOD_UTILS_EXECCONTEXT_H 1

#include "Basics/Common.h"
#include "Rest/RequestContext.h"
#include "Utils/Authentication.h"

namespace arangodb {
namespace transaction {
class Methods;
}

/// Carries some information about the current
/// context in which this thread is executed.
/// We should strive to have it always accessible
/// from ExecContext::CURRENT. Inherits from request
/// context for convencience
class ExecContext : public RequestContext {
 protected:
  ExecContext(bool isInternal, std::string const& user,
              std::string const& database, AuthLevel systemLevel,
              AuthLevel dbLevel)
      : _internal(isInternal),
        _canceled(false),
        _user(user),
        _database(database),
        _systemDbAuthLevel(systemLevel),
        _databaseAuthLevel(dbLevel) {
    TRI_ASSERT(!_internal || _user.empty());
  }
  ExecContext(ExecContext const&) = delete;

 public:
  virtual ~ExecContext() {}

  /// shortcut helper to check the AuthenticationFeature
  static bool isAuthEnabled();
  
  /// @brief an internal superuser context, is
  ///        a singleton instance, deleting is an error
  static ExecContext const* superuser();
  
  /// @brief create user context, caller is responsible for deleting
  static ExecContext* create(std::string const& user, std::string const& db);
  
  /// @brief an internal user is none / ro / rw for all collections / dbs
  bool isInternal() const {
    return _internal;
  }
  
  /// @brief any internal operation is a superuser.
  bool isSuperuser() const { return _internal &&
    _systemDbAuthLevel == AuthLevel::RW &&
    _databaseAuthLevel == AuthLevel::RW;
  }
  
  /// @brief is this an internal read-only user
  bool isReadOnly() const {
    return _internal && _systemDbAuthLevel == AuthLevel::RO;
  }
  
  /// @brief is allowed to manage users, create databases, ...
  bool isAdminUser() const {
    // conflicts with read-only: TRI_ASSERT(!_internal || _systemDbAuthLevel == AuthLevel::RW);
    return _systemDbAuthLevel == AuthLevel::RW;
  }
  
  /// @brief should immediately cance this operation
  bool isCanceled() const {
    return _canceled;
  }
  
  void cancel() {
    _canceled = true;
  }

  /// @brief current user, may be empty for internal users
  std::string const& user() const { return _user; }

  // std::string const& database() const { return _database; }
  /// @brief authentication level on _system. Always RW for superuser
  AuthLevel systemAuthLevel() const { return _systemDbAuthLevel; };

  /// @brief Authentication level on database selected in the current
  ///        request scope. Should almost always contain something,
  ///        if this thread originated in v8 or from HTTP / VST
  AuthLevel databaseAuthLevel() const { return _databaseAuthLevel; };

  /// @brief returns true if auth level is above or equal `requested`
  bool canUseDatabase(AuthLevel requested) const {
    return canUseDatabase(_database, requested);
  }
  /// @brief returns true if auth level is above or equal `requested`
  bool canUseDatabase(std::string const& db, AuthLevel requested) const;

  /// @brief returns auth level for user
  AuthLevel collectionAuthLevel(std::string const& dbname,
                                std::string const& collection) const;

  /// @brief returns true if auth levels is above or equal `requested`
  bool canUseCollection(std::string const& collection,
                        AuthLevel requested) const {
    return canUseCollection(_database, collection, requested);
  }
  /// @brief returns true if auth level is above or equal `requested`
  bool canUseCollection(std::string const& db, std::string const& coll,
                        AuthLevel requested) const {
    return requested <= collectionAuthLevel(db, coll);
  }

 public:
  
  /// Should always contain a reference to current user context
  static thread_local ExecContext const* CURRENT;

 protected:
  
  /// Internal superuser or read-only user
  bool _internal;
  /// should be used to indicate a canceled request / thread
  bool _canceled;
  /// current user, may be empty for internal users
  std::string const _user;
  /// current database to use
  std::string const _database;
  /// level of system database
  AuthLevel _systemDbAuthLevel;
  /// level of current database
  AuthLevel _databaseAuthLevel;

  static ExecContext SUPERUSER;
};

/// @brief scope guard for the exec context
struct ExecContextScope {
  explicit ExecContextScope(ExecContext const* exe)
      : _old(ExecContext::CURRENT) {
    ExecContext::CURRENT = exe;
  }

  ~ExecContextScope() { ExecContext::CURRENT = _old; }

 private:
  ExecContext const* _old;
};
}

#endif
