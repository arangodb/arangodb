////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2019 ArangoDB GmbH, Cologne, Germany
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

#include "Auth/Common.h"
#include "Auth/AuthUser.h"
#include "Auth/DatabaseResource.h"
#include "Rest/RequestContext.h"

#include <memory>
#include <string>

namespace arangodb {
namespace auth {
class AuthUser;
class CollectionResource;
}

namespace transaction {
class Methods;
}

// Carries some information about the current context in which this
// thread is executed.  We should strive to have it always accessible
// from ExecContext::CURRENT. Inherits from request context for
// convencience

class ExecContext : public RequestContext {
  friend struct ExecContextScope;
  friend struct ExecContextSuperuserScope;

 public:
  static std::unique_ptr<ExecContext> create(auth::AuthUser const& user,
                                             auth::DatabaseResource&& database);

 protected:
  enum class Type { User, Internal };

  ExecContext(auth::AuthUser const& user, auth::DatabaseResource&& database,
              auth::Level systemLevel, auth::Level dbLevel);
  ExecContext(auth::Level systemLevel, auth::Level dbLevel);
  ExecContext(ExecContext const&);
  ExecContext(ExecContext&&) = delete;

 public:
  virtual ~ExecContext() {}

  // shortcut helper to check the AuthenticationFeature
  static bool isAuthEnabled();

  // Should always contain a reference to current user context
  static ExecContext const& current();

  // an internal superuser context; is a singleton instance; deleting is
  // an error
  static ExecContext const& superuser() { return ExecContext::Superuser; }

  // an internal user is none / ro / rw for all collections / dbs
  // mainly used to override further permission resolution
  inline bool isInternal() const { return _type == Type::Internal; }

  //  any internal user is a superuser if he has rw access
  bool isSuperuser() const {
    return isInternal() && _systemDbAuthLevel == auth::Level::RW &&
           _databaseAuthLevel == auth::Level::RW;
  }

  // is this an internal read-only user
  bool isReadOnly() const {
    return isInternal() && _systemDbAuthLevel == auth::Level::RO;
  }

  // is allowed to manage users, create databases, ...
  bool isAdminUser() const { return _systemDbAuthLevel == auth::Level::RW; }

  // should immediately cancel this operation
  bool isCanceled() const { return _canceled; }

  void cancel() { _canceled = true; }

  // current user, may be empty for internal users
  std::string const& user() const { return _user; }

  // std::string const& database() const { return _database; }

  // authentication level on _system. Always RW for superuser
  auth::Level systemAuthLevel() const { return _systemDbAuthLevel; };

  // Authentication level on database selected in the current
  // request scope. Should almost always contain something,
  // if this thread originated in v8 or from HTTP / VST
  auth::Level databaseAuthLevel() const { return _databaseAuthLevel; };

  // returns true if auth level is above or equal `requested`
  bool canUseDatabase(auth::Level requested) const {
    return requested <= _databaseAuthLevel;
  }

  // returns true if auth level is above or equal `requested`
  auth::Level authLevel(auth::DatabaseResource const&) const;

  // returns auth level for user
  auth::Level authLevel(auth::CollectionResource const&) const;

  // returns true if auth level is above or equal `requested`
  bool canUseCollection(auth::CollectionResource const& coll,
                        auth::Level requested) const {
    return requested <= authLevel(coll);
  }

 protected:
  Type _type;

  // current user, may be empty for internal users
  std::string const _user;

  // current database to use
  auth::DatabaseResource const _database;

  // should be used to indicate a canceled request / thread
  bool _canceled;

  // level of system database
  auth::Level _systemDbAuthLevel;

  // level of current database
  auth::Level _databaseAuthLevel;

 private:
  static ExecContext Superuser;
  static thread_local ExecContext const* CURRENT;
};

// scope guard for the exec context
struct ExecContextScope {
  explicit ExecContextScope(ExecContext const* exe)
      : _old(ExecContext::CURRENT) {
    ExecContext::CURRENT = exe;
  }

  ~ExecContextScope() { ExecContext::CURRENT = _old; }

 private:
  ExecContext const* _old;
};

struct ExecContextSuperuserScope {
  explicit ExecContextSuperuserScope() : _old(ExecContext::CURRENT) {
    ExecContext::CURRENT = &ExecContext::Superuser;
  }

  explicit ExecContextSuperuserScope(bool cond) : _old(ExecContext::CURRENT) {
    if (cond) {
      ExecContext::CURRENT = &ExecContext::Superuser;
    }
  }

  ~ExecContextSuperuserScope() { ExecContext::CURRENT = _old; }

 private:
  ExecContext const* _old;
};

}  // namespace arangodb

#endif
