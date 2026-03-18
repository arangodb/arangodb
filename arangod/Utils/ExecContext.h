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

#include "Auth/AuthMode.h"
#include "Auth/Common.h"
#include "Auth/Rbac/Actions.h"
#include "Rest/RequestContext.h"

#include <memory>
#include <string>

namespace arangodb {
namespace transaction {
class Methods;
}
class AuthenticationFeature;
class RbacFeature;

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
  ExecContext(ConstructorToken, AuthMode authMode);
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

  bool isInternal() const noexcept {
    std::abort();  // TODO remove this method
  }

  bool isSuperuser() const noexcept { return _authMode.isSuperuser(); }

  bool isReadOnly() const noexcept {
    std::abort();  // TODO remove this method
  }

  bool isAdminUser(
      arangodb::rbac::Category::Any const& rbacAction) const noexcept {
    std::abort();  // TODO remove this method
  }

  /// @brief tells you if this execution was canceled
  virtual bool isCanceled() const { return false; }

  /// @brief current user, may be empty for internal users
  std::string_view user() const { return _authMode.getIAuth().username(); }

  std::string const& database() const {
    std::abort();  // TODO remove this method
  }

  /// @brief authentication level on _system. Always RW for superuser
  auth::Level systemAuthLevel() const noexcept {
    std::abort();  // TODO remove this method
  }

  /// @brief Authentication level on database selected in the current
  ///        request scope. Should almost always contain something,
  ///        if this thread originated in v8 or from HTTP
  auth::Level databaseAuthLevel() const noexcept {
    std::abort();  // TODO remove this method
  }

  /// @brief returns true if auth level is above or equal `requested`
  bool canUseDatabase(std::string const& db, auth::Level requested) const;

  /// @brief returns auth level for user
  auth::Level collectionAuthLevel(std::string_view dbname,
                                  std::string_view collection) const;

  /// @brief returns true if auth level is above or equal `requested`
  bool canUseCollection(std::string_view db, std::string_view coll,
                        auth::Level requested) const {
    return _authMode.getIAuth().canUse(
        {Permission::DataSource{.database = std::string(db),
                                .name = std::string(coll),
                                .level = requested}});
  }

  static std::shared_ptr<ExecContext const> set(
      std::shared_ptr<ExecContext const> ctx) {
    std::swap(CURRENT, ctx);
    return ctx;
  }

#ifdef USE_ENTERPRISE
  [[nodiscard]] virtual std::string clientAddress() const { return ""; }
  [[nodiscard]] virtual std::string requestUrl() const { return ""; }
  [[nodiscard]] virtual std::string authMethod() const { return ""; }
#endif

 protected:
  AuthMode _authMode;

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
