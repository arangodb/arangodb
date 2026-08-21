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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Assertions/ProdAssert.h"
#include "Auth/AuthMode.h"
#include "Auth/Permissions.h"
#include "Basics/Result.h"
#include "Utils/DatabaseGuard.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <string>

namespace arangodb {
struct Database;

namespace transaction {
class Methods;
}
class AuthenticationFeature;
class GeneralRequest;
class RbacFeature;
class ServerSecurityFeature;

/// Carries some information about the current
/// context in which this thread is executed.
/// We should strive to have it always accessible
/// from ExecContext::CURRENT.
class ExecContext {
  friend struct ExecContextScope;
  friend struct ExecContextSuperuserScope;

 protected:
  class ConstructorToken {};

 public:
  ExecContext(ConstructorToken, AuthMode authMode, bool isRestApiHardened,
              VocbasePtr vocbase, std::string clientAddress = {},
              std::string requestUrl = {}, std::string authMethod = "n/a",
              bool hasRequestInfo = false);
  ExecContext(ExecContext const&) = delete;
  ExecContext(ExecContext&&) = delete;

  /// @brief Create an ExecContext from an incoming request with a vocbase.
  /// This is the main factory for creating real ExecContexts.
  /// Superuser JWT requests create a dynamic Superuser context (not the static
  /// one) so that vocbase and request information is preserved.
  [[nodiscard]] static std::shared_ptr<ExecContext> create(
      AuthenticationFeature& authenticationFeature, RbacFeature& rbacFeature,
      ServerSecurityFeature const& securityFeature, GeneralRequest& req,
      VocbasePtr vocbase);

  /// Should always contain a reference to current user context
  static ExecContext const& current();
  /// Note that this intentionally returns CURRENT, even if it is a nullptr:
  /// This makes it suitable to set CURRENT in another thread.
  static std::shared_ptr<ExecContext const> currentAsShared();

  /// @brief an internal superuser context, is
  ///        a singleton instance, deleting is an error
  static ExecContext const& superuser();
  static std::shared_ptr<ExecContext const> superuserAsShared();

  [[nodiscard]] bool isDisabled() const noexcept {
    return _authMode.isDisabled();
  }

  [[nodiscard]] bool isSuperuserOrDisabled() const noexcept {
    // This will report `true` if authentication is disabled!
    return _authMode.isSuperuser() || _authMode.isDisabled();
  }

  [[nodiscard]] bool isSuperuser() const noexcept {
    return _authMode.isSuperuser();
  }
  [[nodiscard]] bool isClassic() const noexcept {
    return _authMode.isClassic();
  }

  /// @brief tells you if this execution was canceled
  bool isCanceled() const noexcept {
    return _canceled.load(std::memory_order_relaxed);
  }

  /// @brief cancel execution
  void cancel() noexcept { _canceled.store(true, std::memory_order_relaxed); }

  /// @brief returns the vocbase associated with this context, if any
  [[nodiscard]] std::optional<std::reference_wrapper<Database>> vocbase()
      const noexcept;

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
  //
  // Every question asked here is traced on `Logger::AUTHORIZATION` (TRACE);
  // see tests/js/client/server_permissions/authorization-questions.js.
  [[nodiscard]] Result can(auth::Permission permission) const;

  // The server-wide read-only gate, which every modifying operation has to
  // pass in addition to its permission question. It is consulted after that
  // question has been answered positively. Like `can()`, it is traced on
  // `Logger::AUTHORIZATION` (TRACE) as `AUTHZ-CHECK IsReadOnly` - the trace
  // is emitted whenever the gate is consulted, no matter how it answers, so
  // that the authorization-questions tests can see which operations are
  // gated by it.
  //
  // Note that `arangod/Auth/UserManagerBase.cpp` has read-only checks of its
  // own which are not routed through here and hence not traced.
  [[nodiscard]] Result checkNotReadOnly() const;

  // New Result-returning permission check methods:

  // Check an admin-class action. This is a thin wrapper around can(); it is
  // likely to be removed in favor of calling can() directly.
  Result canUseAdminAction(auth::perms::AnyAdmin auto action) const {
    return can(std::move(action));
  }

  // Like canUseAdminAction, but the check is only performed when the REST API
  // is hardened; otherwise the action is allowed. RBAC always implies a
  // hardened REST API.
  Result canUseHardenedAction(auth::perms::AnyAdmin auto action) const {
    ADB_PROD_ASSERT(!_authMode.isRbac() || _isRestApiHardened)
        << "RBAC is enabled, but REST API is not hardened: "
           "RBAC implies --server.harden=true ("
           "ServerSecurityFeatureOptions::hardenedRestApi = true).";
    if (!_isRestApiHardened) {
      return {};
    }
    Result r = can(std::move(action));
    if (r.ok()) {
      return r;
    }
    // Compatibility with 3.12.10:
    return {TRI_ERROR_FORBIDDEN, r.errorMessage()};
  }

  Result canSeeDatabase(std::string_view db) const;
  Result canCreateDatabase(std::string_view db) const;
  Result canDropDatabase(std::string_view db) const;
  Result canUseDatabase(std::string_view db, DatabaseAccessLevel level) const;

  Result canSeeCollection(std::string_view db, std::string_view coll) const;
  Result canCreateCollection(std::string_view db, std::string_view coll) const;
  Result canDropCollection(std::string_view db, std::string_view coll) const;
  Result canUseCollection(std::string_view db, std::string_view coll,
                          CollectionAccessLevel level) const;

  // May the current identity dump (via arangodump) this collection?
  // Behaves like `canUseCollection(db, coll, CollectionAccessLevel::Read)`,
  // except that in Classic Mode it is additionally granted to identities
  // with RW access to the `_system` database (i.e. "admins", equivalent to
  // `canUseAdminAction(rbac::Category::AdminDump{})`).
  Result canDumpCollection(std::string_view db, std::string_view coll) const;

  // May the current identity restore (via arangorestore) this collection?
  // Behaves like
  // `canUseCollection(db, coll, CollectionAccessLevel::WriteData)`, except
  // that in Classic Mode it is additionally granted to identities with RW
  // access to the `_system` database (i.e. "admins", equivalent to
  // `canUseAdminAction(rbac::Category::AdminRestore{})`).
  // The flag `overwrite` indicates if we need to drop and recreate the
  // collection in the "overwrite" case.
  Result canRestoreCollection(std::string_view db, std::string_view coll,
                              bool overwrite) const;

  Result canRestoreCreateIndex(std::string_view db,
                               std::string_view coll) const;
  Result canRestoreCreateView(std::string_view db, std::string_view viewName,
                              std::vector<std::string> view) const;
  Result canRestoreDropView(std::string_view db, std::string_view view) const;
  Result canRestoreWriteData(std::string_view db, std::string_view coll) const;

  Result canCreateIndex(std::string_view db, std::string_view coll) const;
  Result canDropIndex(std::string_view db, std::string_view coll) const;

  Result canSeeView(std::string_view db, std::string_view view) const;
  Result canCreateView(std::string_view db, std::string_view view,
                       std::vector<std::string> const& linkedCollections) const;
  Result canModifyView(std::string_view db, std::string_view view,
                       std::vector<std::string> const& linkedCollections) const;
  Result canDropView(std::string_view db, std::string_view view,
                     std::vector<std::string> const& linkedCollections) const;
  Result canUseView(std::string_view db, std::string_view view,
                    ViewAccessLevel level) const;

  Result canSeeGraph(std::string_view db, std::string_view graph) const;
  Result canCreateGraph(std::string_view db, std::string_view graph,
                        std::span<std::string> collectionNamesToCreate,
                        std::span<std::string> collectionNamesToRead) const;
  Result canDropGraph(std::string_view db, std::string_view graph,
                      std::span<std::string> collectionNames) const;
  Result canUseGraph(std::string_view db, std::string_view graph,
                     GraphAccessLevel const level) const;
  Result canRenameView(std::string_view db, std::string_view oldViewName,
                       std::string_view newViewName,
                       std::vector<std::string> const& linkedCollections) const;

  Result canSeeAnalyzer(std::string_view db, std::string_view analyzer) const;
  Result canCreateAnalyzer(std::string_view db,
                           std::string_view analyzer) const;
  Result canDropAnalyzer(std::string_view db, std::string_view analyzer) const;
  Result canUseAnalyzer(std::string_view db, std::string_view analyzer,
                        AnalyzerAccessLevel level) const;

  /// @brief returns true if the user can be read
  Result canReadUser(std::string_view userName) const;

  /// @brief returns true for each user which can be read
  // TODO Should this return a std::vector<Result>?
  // MAX: I do not think so, it is used only once to filter the visible
  // users. All we need is the bool.
  std::vector<bool> canReadUsers(std::span<std::string_view> users) const;

  /// @brief returns true if the given user may be created.
  Result canCreateUser(std::string_view userName) const;

  /// @brief returns true if the given user may be dropped.
  Result canDropUser(std::string_view userName) const;

  /// @brief returns true if the given user's own profile (password, active
  /// flag, config blob) may be modified. Note that everybody can modify
  /// their own profile (if only to change the password).
  Result canModifyUserProfile(std::string_view userName) const;

  /// @brief returns true if the given user's permissions on databases and
  /// collections may be granted/revoked.
  Result canGrantUserPermissions(std::string_view userName) const;

  /// @brief returns whether the given REST API version may be used, e.g. 1 for
  /// `/_arango/v1`. Note that this does not ask whether the version exists -
  /// that is decided by the handler factory.
  Result canUseApiVersion(uint32_t version) const;

  static std::shared_ptr<ExecContext const> set(
      std::shared_ptr<ExecContext const> ctx) {
    std::swap(CURRENT, ctx);
    return ctx;
  }

#ifdef USE_ENTERPRISE
  [[nodiscard]] std::string clientAddress() const;
  [[nodiscard]] std::string requestUrl() const;
  [[nodiscard]] std::string authMethod() const;
#endif

 private:
  AuthMode _authMode;
  bool const _isRestApiHardened;
  VocbasePtr _vocbase;

  // Client address, request URL and authentication method of the request
  // this context was created from, if any (see ExecContext::create()).
  // Preserved across forceSuperuser()/ExecContextSuperuserScope so that
  // auditing keeps working after a privilege upgrade.
  std::string _clientAddress;
  std::string _requestUrl;
  std::string _authMethod{"n/a"};
  bool _hasRequestInfo{false};

  // TODO (Tobias) this feels out of place. Look into it.
  /// should be used to indicate a canceled request / thread
  std::atomic<bool> _canceled{false};

  static std::shared_ptr<ExecContext const> const Superuser;
  static thread_local std::shared_ptr<ExecContext const> CURRENT;
};

/// @brief scope guard for the exec context
struct ExecContextScope {
  explicit ExecContextScope(std::shared_ptr<ExecContext const> exe) noexcept;

  ~ExecContextScope() noexcept;

 private:
  std::shared_ptr<ExecContext const> _old;
};

/// @brief scope guard installing a Superuser exec context
///
/// Use this to deliberately run an internal execution path with a clear
/// purpose -- e.g. the startup thread, a dedicated background thread, or
/// system-collection maintenance -- as Superuser.
///
/// Avoid using this to escalate privileges during request handling in order to
/// skip subsequent permission checks; authorization methods on ExecContext
/// should handle such cases properly on their own, whenever possible.
struct ExecContextSuperuserScope {
  explicit ExecContextSuperuserScope();

  explicit ExecContextSuperuserScope(bool cond);

  ~ExecContextSuperuserScope() { ExecContext::CURRENT = _old; }

 private:
  static auto getSuperuserContextFrom(ExecContext const* old)
      -> std::shared_ptr<ExecContext const>;

  std::shared_ptr<ExecContext const> _old;
};

}  // namespace arangodb
