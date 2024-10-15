////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Auth/Common.h"
#include "Rest/RequestContext.h"

#include <memory>
#include <string>

namespace arangodb {
namespace transaction {
class Methods;
}

/// Carries some information about the current
/// context in which this thread is executed.
/// We should strive to have it always accessible
/// from ExecContext::CURRENT. Inherits from request
/// context for convenience
class ExecContext : public RequestContext {
  friend struct ExecContextScope;
  friend struct ExecContextSuperuserScope;

 protected:
  enum class Type { Default, Internal };
  class ConstructorToken {};

 public:
  ExecContext(ConstructorToken, ExecContext::Type type, std::string const& user,
              std::string const& database, auth::Level systemLevel,
              auth::Level dbLevel, bool isAdminUser);
  ExecContext(ExecContext const&) = delete;
  ExecContext(ExecContext&&) = delete;

 public:
  virtual ~ExecContext() = default;

  /// shortcut helper to check the AuthenticationFeature
  static bool isAuthEnabled();

  /// Should always contain a reference to current user context
  static ExecContext const& current();
  /// Note that this intentionally returns CURRENT, even if it is a nullptr:
  /// This makes it suitable to set CURRENT in another thread.
  static std::shared_ptr<ExecContext const> currentAsShared();

  /// @brief an internal superuser context, is
  ///        a singleton instance, deleting is an error
  static ExecContext const& superuser();
  static std::shared_ptr<ExecContext const> superuserAsShared();

  /// @brief create user context
  static std::shared_ptr<ExecContext> create(std::string const& user,
                                             std::string const& db);

  /// @brief an internal user is none / ro / rw for all collections / dbs
  /// mainly used to override further permission resolution
  bool isInternal() const noexcept { return _type == Type::Internal; }

  /// @brief any internal operation is a superuser.
  bool isSuperuser() const noexcept {
    return isInternal() && _systemDbAuthLevel == auth::Level::RW &&
           _databaseAuthLevel == auth::Level::RW;
  }

  /// @brief is this an internal read-only user
  bool isReadOnly() const noexcept {
    return isInternal() && _systemDbAuthLevel == auth::Level::RO;
  }

  /// @brief is allowed to manage users, create databases, ...
  bool isAdminUser() const noexcept { return _isAdminUser; }

  /// @brief tells you if this execution was canceled
  virtual bool isCanceled() const { return false; }

  /// @brief current user, may be empty for internal users
  std::string const& user() const { return _user; }

  /// @brief current database
  std::string const& database() const { return _database; }

  // std::string const& database() const { return _database; }
  /// @brief authentication level on _system. Always RW for superuser
  auth::Level systemAuthLevel() const noexcept { return _systemDbAuthLevel; }

  /// @brief Authentication level on database selected in the current
  ///        request scope. Should almost always contain something,
  ///        if this thread originated in v8 or from HTTP
  auth::Level databaseAuthLevel() const noexcept { return _databaseAuthLevel; }

  /// @brief returns true if auth level is above or equal `requested`
  bool canUseDatabase(auth::Level requested) const noexcept {
    return requested <= _databaseAuthLevel;
  }

  /// @brief returns true if auth level is above or equal `requested`
  bool canUseDatabase(std::string const& db, auth::Level requested) const;

  /// @brief returns auth level for user
  auth::Level collectionAuthLevel(std::string const& dbname,
                                  std::string_view collection) const;

  /// @brief returns true if auth levels is above or equal `requested`
  bool canUseCollection(std::string const& collection,
                        auth::Level requested) const {
    return canUseCollection(_database, collection, requested);
  }
  /// @brief returns true if auth level is above or equal `requested`
  bool canUseCollection(std::string const& db, std::string const& coll,
                        auth::Level requested) const {
    return requested <= collectionAuthLevel(db, coll);
  }

  static std::shared_ptr<ExecContext const> set(
      std::shared_ptr<ExecContext const> ctx) {
    auto tmp = CURRENT;
    CURRENT = ctx;
    return tmp;
  }

#ifdef USE_ENTERPRISE
  virtual std::string clientAddress() const { return ""; }
  virtual std::string requestUrl() const { return ""; }
  virtual std::string authMethod() const { return ""; }
#endif

 protected:
  /// current user, may be empty for internal users
  std::string const _user;
  /// current database to use, superuser db is empty
  std::string const _database;

  Type _type;
  /// Flag if admin user access (not regarding cluster RO mode)
  bool _isAdminUser;
  /// level of system database
  auth::Level _systemDbAuthLevel;
  /// level of current database
  auth::Level _databaseAuthLevel;

 private:
  static std::shared_ptr<ExecContext const> const Superuser;
  static thread_local std::shared_ptr<ExecContext const> CURRENT;
};

/// @brief scope guard for the exec context
struct ExecContextScope {
  explicit ExecContextScope(std::shared_ptr<ExecContext const> exe);

  ~ExecContextScope();

 private:
  std::shared_ptr<ExecContext const> _old;
};

struct ExecContextSuperuserScope {
  explicit ExecContextSuperuserScope() : _old(ExecContext::CURRENT) {
    ExecContext::CURRENT = ExecContext::Superuser;
  }

  explicit ExecContextSuperuserScope(bool cond) : _old(ExecContext::CURRENT) {
    if (cond) {
      ExecContext::CURRENT = ExecContext::Superuser;
    }
  }

  ~ExecContextSuperuserScope() { ExecContext::CURRENT = _old; }

 private:
  std::shared_ptr<ExecContext const> _old;
};

}  // namespace arangodb
