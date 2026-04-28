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
#include "Auth/Rbac/Actions.h"
#include "Auth/Permissions.h"
#include "Basics/Result.h"
#include "Rest/RequestContext.h"

#include <memory>
#include <span>
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
  ~ExecContext() override = default;

  /// Should always contain a reference to current user context
  static ExecContext const& current();
  /// Note that this intentionally returns CURRENT, even if it is a nullptr:
  /// This makes it suitable to set CURRENT in another thread.
  static std::shared_ptr<ExecContext const> currentAsShared();

  /// @brief an internal superuser context, is
  ///        a singleton instance, deleting is an error
  static ExecContext const& superuser();
  static std::shared_ptr<ExecContext const> superuserAsShared();

  [[nodiscard]] bool isSuperuser() const noexcept {
    // This will report `true` if authentication is disabled!
    return _authMode.isSuperuser() || _authMode.isDisabled();
  }

  [[nodiscard]] bool isSuperuserConsideringReadOnly() const noexcept {
    // This will report `true` if authentication is disabled!
    // FIXME: But it will return `false` if the deployment is in read-only mode.
    // We will see if we actually need this API call.
    return _authMode.isSuperuser() || _authMode.isDisabled();
  }

  /// @brief tells you if this execution was canceled
  // TODO I think it's strange to to have this in ExecContext. It's implemented
  //      in the VocbaseContext.
  //      It is set to true exclusively from RestVocbaseBaseHandler::cancel().
  //      The cancel() method on the RestHandler is used exclusively from the
  //      AsyncJobManager.
  //      It is read exclusively in transaction::Methods::commitInternal,
  //      to abort right before a commit.
  //      So its kind of used to abort transactions, but only those that happen
  //      to be committed under this ExecContext, and only at commit time.
  //      Except they are not aborted, just the commit fails; though that might
  //      lead to the transaction being aborted later, e.g. when the Methods
  //      object goes out of scope. Probably not for streaming transactions,
  //      though, which might or might not be desirable.
  virtual bool isCanceled() const { return false; }

  /// @brief current user, may be empty for internal users
  [[nodiscard]] std::string_view user() const {
    return _authMode.getIAuth().username();
  }

  // Unified permission-check entry point. Prefer this over the canXxx()
  // methods below for new code; eventually they are going to be removed.
  //
  // Since `auth::Permission` is a `std::variant`, it is implicitly
  // constructed from any of its alternatives in `auth::perms::...`. Typical
  // usage:
  //
  //   using namespace arangodb::auth::perms;
  //   if (auto r = ec.can(SeeCollection{.db = db, .name = coll});
  //       !r.ok()) { /* ... */ }
  [[nodiscard]] Result can(auth::Permission permission) const {
    return _authMode.getIAuth().check(std::move(permission));
  }

  // New Result-returning permission check methods:

  Result canUseAdminAction(
      arangodb::rbac::Category::Any const& rbacAction) const;

  Result canUseHardenedAction(rbac::Category::Any const& action) const;

  Result canSeeDatabase(std::string_view db) const;
  Result canCreateDatabase(std::string_view db) const;
  Result canDropDatabase(std::string_view db) const;
  Result canUseDatabase(std::string_view db, DatabaseAccessLevel level) const;

  Result canSeeCollection(std::string_view db, std::string_view coll) const;
  Result canCreateCollection(std::string_view db, std::string_view coll) const;
  Result canDropCollection(std::string_view db, std::string_view coll) const;
  Result canUseCollection(std::string_view db, std::string_view coll,
                          CollectionAccessLevel level) const;

  Result canCreateIndex(std::string_view db, std::string_view coll) const;
  Result canDropIndex(std::string_view db, std::string_view coll) const;

  Result canSeeView(std::string_view db, std::string_view view) const;
  Result canCreateView(std::string_view db, std::string_view view) const;
  // TODO Remove defaulting of the collections parameter, it's only
  //      there for now so everything compiles.
  Result canDropView(std::string_view db, std::string_view view,
                     std::vector<std::string> collections = {}) const;
  Result canUseView(std::string_view db, std::string_view view,
                    ViewAccessLevel level) const;

  // TODO These are not yet in path_permissions.md!
  // TODO We should need only the second, but it's additional work
  //      to refactor the code in that way.
  Result canRenameView(std::string_view db, std::string_view oldViewName,
                       std::string_view newViewName) const;
  Result canRenameView(std::string_view db, std::string_view oldViewName,
                       std::string_view newViewName,
                       std::vector<std::string> collections) const;

  Result canSeeAnalyzer(std::string_view db, std::string_view analyzer) const;
  Result canCreateAnalyzer(std::string_view db,
                           std::string_view analyzer) const;
  Result canDropAnalyzer(std::string_view db, std::string_view analyzer) const;
  Result canUseAnalyzer(std::string_view db, std::string_view analyzer,
                        AnalyzerAccessLevel level) const;

  /// @brief returns true if the user can be read
  Result canReadUser(std::string_view user) const;

  /// @brief returns true for each user which can be read
  // TODO Should this return a std::vector<Result>?
  // MAX: I do not think so, it is used only once to filter the visible
  // users. All we need is the bool.
  std::vector<bool> canReadUsers(std::span<std::string_view> users) const;

  /// @brief returns true if the user can be modified, note that everybody
  // can modify themselves (if only to change the password).
  Result canWriteUser(std::string_view user) const;

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
