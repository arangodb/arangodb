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
/// @author Frank Celler
/// @author Simon Grätzr
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_EXECCONTEXT_H
#define ARANGOD_UTILS_EXECCONTEXT_H 1

#include "Auth/AuthUser.h"
#include "Auth/Common.h"
#include "Auth/DatabaseResource.h"
#include "Rest/RequestContext.h"

#include <memory>
#include <string>

namespace arangodb {
namespace auth {
class AuthUser;
class CollectionResource;
}  // namespace auth

namespace transaction {
class Methods;
}

// Carries some information about the current context in which this
// thread is executed.  We should strive to have it always accessible
// from ExecContext::CURRENT. Inherits from request context for
// convencience

class ExecContext : public RequestContext {
 private:
  class ScopeBase {
   public:
    explicit ScopeBase(ExecContext const* exe) : _old(ExecContext::CURRENT) {
      ExecContext::CURRENT = exe;
    }

    ScopeBase(ExecContext const* exe, bool cond) : _old(ExecContext::CURRENT) {
      if (cond) {
        ExecContext::CURRENT = &ExecContext::Superuser;
      }
    }

    ~ScopeBase() { ExecContext::CURRENT = _old; }

   private:
    ExecContext const* _old;
  };

 public:
  class Scope : public ScopeBase {
   public:
    explicit Scope(ExecContext const* exe) : ScopeBase(exe) {}
  };

  // scope guard for the exec context
  class AdminScope : public ScopeBase {
   public:
    explicit AdminScope() : ScopeBase(&ExecContext::Admin) {}
  };

  class NobodyScope : public ScopeBase {
   public:
    explicit NobodyScope() : ScopeBase(&ExecContext::Nobody) {}
  };

  class SuperuserScope : public ScopeBase {
   public:
    SuperuserScope() : ScopeBase(&ExecContext::Superuser) {}
    explicit SuperuserScope(bool cond) : ScopeBase(&ExecContext::Superuser, cond) {}
  };

  class ReadOnlySuperuserScope : public ScopeBase {
   public:
    ReadOnlySuperuserScope() : ScopeBase(&ExecContext::ReadOnlySuperuser) {}
    explicit ReadOnlySuperuserScope(bool cond) : ScopeBase(&ExecContext::ReadOnlySuperuser, cond) {}
  };

 public:
  static std::unique_ptr<ExecContext> create(auth::AuthUser const& user,
                                             auth::DatabaseResource&& database);
  virtual ~ExecContext() {}

 protected:
  enum class Type { User, Internal };

  ExecContext(auth::AuthUser const& user, auth::DatabaseResource&& database,
              auth::Level systemLevel, auth::Level dbLevel);
  ExecContext(auth::Level dbLevel);
  ExecContext(ExecContext const&);
  ExecContext(ExecContext&&) = delete;

 public:
  std::string const& user() const { return _user.internalUsername(); }
  auth::DatabaseResource const& database() const { return _database; }

  // should immediately cancel this operation
  bool isCanceled() const { return _canceled; }
  void cancel() { _canceled = true; }

  // shortcut helper to check the AuthenticationFeature
  static bool isAuthEnabled();

  // Should always contain a reference to current user context
  static ExecContext const& current();

  // returns true if auth level is above or equal `requested`
  static bool currentHasAccess(auth::DatabaseResource const& database, auth::Level requested) {
    return current().hasAccess(database, requested);
  }

  bool hasAccess(auth::DatabaseResource const& database, auth::Level requested) const {
    return requested <= authLevel(database);
  }

  bool hasAccess(auth::CollectionResource const& coll, auth::Level requested) const {
    return requested <= authLevel(coll);
  }

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

 private:
  auth::Level authLevel(auth::DatabaseResource const&) const;
  auth::Level authLevel(auth::CollectionResource const&) const;

 protected:
  Type _type;

  // current user, may be empty for internal users
  auth::AuthUser const _user;

  // current database to use
  auth::DatabaseResource const _database;

  // should be used to indicate a canceled request / thread
  bool _canceled;

  // level of system database
  auth::Level _systemDbAuthLevel;

  // level of current database
  auth::Level _databaseAuthLevel;

 private:
  static thread_local ExecContext const* CURRENT;

  static ExecContext Admin;
  static ExecContext Nobody;
  static ExecContext ReadOnlySuperuser;
  static ExecContext Superuser;
};
}  // namespace arangodb

#endif
