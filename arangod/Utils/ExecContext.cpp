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

#include "ExecContext.h"

#include "Assertions/ProdAssert.h"
#include "Auth/Rbac/RbacFeature.h"
#include "Basics/Result.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/LogMacros.h"
#include "Rest/GeneralRequest.h"
#include "VocBase/vocbase.h"

#include <ranges>

using namespace arangodb;

thread_local std::shared_ptr<ExecContext const> ExecContext::CURRENT = nullptr;

std::shared_ptr<ExecContext const> const ExecContext::Superuser =
    std::make_shared<ExecContext const>(ConstructorToken{},
                                        AuthMode{AuthMode::Superuser{}}, true,
                                        VocbasePtr{nullptr});

/// Should always contain a reference to current user context
ExecContext const& ExecContext::current() {
  // Since COR-811 every execution path is expected to install an ExecContext
  // explicitly (request handling, dedicated threads, scheduler work items,
  // coroutine resumption, tasks). Reaching this function without one means
  // either an infrastructure thread -- which deliberately has no context and
  // must never run authorization-relevant code -- or a missed propagation
  // path; both are bugs.
  TRI_ASSERT(CURRENT != nullptr);
  if (CURRENT != nullptr) {
    return *CURRENT;
  }
  // in production builds, fail towards the historical behavior, for now.
  // change this into a prod assert later.
  return *Superuser;
}

/// Note that this intentionally returns CURRENT, even if it is a nullptr: This
/// makes it suitable to set CURRENT in another thread.
std::shared_ptr<ExecContext const> ExecContext::currentAsShared() {
  return CURRENT;
}

/// @brief an internal superuser context, is
///        a singleton instance, deleting is an error
ExecContext const& ExecContext::superuser() { return *Superuser; }
std::shared_ptr<ExecContext const> ExecContext::superuserAsShared() {
  return Superuser;
}

ExecContext::ExecContext(ConstructorToken, AuthMode authMode,
                         bool isRestApiHardened, VocbasePtr vocbase)
    : _authMode(std::move(authMode)),
      _isRestApiHardened(isRestApiHardened),
      _vocbase(std::move(vocbase)) {}

/*static*/ std::shared_ptr<ExecContext> ExecContext::create(
    AuthenticationFeature& authenticationFeature, RbacFeature& rbacFeature,
    ServerSecurityFeature const& securityFeature, GeneralRequest& req,
    VocbasePtr vocbase) {
  // Extract raw pointer for AuthMode construction
  // The VocbasePtr will be moved into ExecContext, which will own the
  // reference
  TRI_vocbase_t* vocbasePtr = vocbase.get();
  TRI_ASSERT(vocbasePtr && !vocbasePtr->isDangling());

  auto authMode = AuthMode{[&]() -> AuthMode::Any {
    bool isSuperUser =
        req.authenticated() && req.user().empty() &&
        req.authenticationMethod() == rest::AuthenticationMethod::JWT;
    if (isSuperUser) {
      // For a superuser JWT request, create a dynamic Superuser context that
      // preserves the request reference (for auditing etc.).
      return AuthMode::Superuser(req);
    }

    if (!authenticationFeature.isActive()) {
      return AuthMode::Disabled(req.user(), req);
    }

    auto* userManager = authenticationFeature.userManager();
    // In a Cluster, with authentication enabled, on DBServers and Agents,
    // there is no UserManager, but at least the SuperUser can be
    // authenticated. In that case we treat the request as unauthenticated.
    if (!req.authenticated() || userManager == nullptr) {
      return AuthMode::Unauthenticated(req.user(), req);
    }

    if (auto* rbacService = rbacFeature.service(); rbacService != nullptr) {
      return AuthMode::Rbac(*rbacService, req.user(), req.jwtToken(), req);
    }

    ADB_PROD_ASSERT(userManager != nullptr);
    return AuthMode::Classic(*userManager, req.user(), req);
  }()};

  return std::make_shared<ExecContext>(ConstructorToken{}, std::move(authMode),
                                       securityFeature.isRestApiHardened(),
                                       std::move(vocbase));
}

void ExecContext::forceSuperuser() {
  // Preserve any existing request/vocbase references in the new Superuser
  // mode so that the destructor can still release the vocbase correctly,
  // and auditing information remains accessible.
  auto req = _authMode.getIAuth().request();
  if (_vocbase && req.has_value()) {
    _authMode.reset<AuthMode::Superuser>(req->get());
  } else {
    _authMode.reset<AuthMode::Superuser>();
  }
}

std::optional<std::reference_wrapper<TRI_vocbase_t>> ExecContext::vocbase()
    const noexcept {
  if (_vocbase) {
    return *_vocbase;
  }
  return std::nullopt;
}

#ifdef USE_ENTERPRISE
std::string ExecContext::clientAddress() const {
  if (auto req = request(); req.has_value()) {
    return req->get().connectionInfo().fullClient();
  }
  return {};
}

std::string ExecContext::requestUrl() const {
  if (auto req = request(); req.has_value()) {
    return req->get().fullUrl();
  }
  return {};
}

std::string ExecContext::authMethod() const {
  if (auto req = request(); req.has_value()) {
    switch (req->get().authenticationMethod()) {
      case rest::AuthenticationMethod::BASIC:
        return "http basic";
      case rest::AuthenticationMethod::JWT:
        return "http jwt";
      case rest::AuthenticationMethod::NONE:
        break;
    }
  }
  return "n/a";
}
#endif

Result ExecContext::can(auth::Permission permission) const {
  // Note that the log message is built before the check, because `check()`
  // consumes `permission`.
  LOG_TOPIC("7e3f1", TRACE, Logger::AUTHORIZATION)
      << "AUTHZ-CHECK " << permission;
  return _authMode.getIAuth().check(std::move(permission));
}

Result ExecContext::checkNotReadOnly() const {
  // Note that this is logged unconditionally, i.e. also when the gate lets
  // the operation pass: the trace documents that the question was asked, in
  // the same way `can()` does.
  LOG_TOPIC("5f9c2", TRACE, Logger::AUTHORIZATION) << "AUTHZ-CHECK IsReadOnly";
  if (!isSuperuser() && ServerState::readOnly()) {
    return {TRI_ERROR_ARANGO_READ_ONLY, "Server is in read-only mode."};
  }
  return {};
}

Result ExecContext::canSeeDatabase(std::string_view db) const {
  using namespace auth::perms;
  return can(SeeDatabase{.name{db}});
}

Result ExecContext::canCreateDatabase(std::string_view db) const {
  using namespace auth::perms;
  if (auto r = can(CreateDatabase{.name{db}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return {TRI_ERROR_FORBIDDEN, "Server is in read-only mode."};
  }
  return {};
}

Result ExecContext::canDropDatabase(std::string_view db) const {
  using namespace auth::perms;
  if (auto r = can(DropDatabase{.name{db}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canUseDatabase(std::string_view db,
                                   DatabaseAccessLevel level) const {
  using namespace auth::perms;
  if (auto r = can(UseDatabase{.name{db}, .level = level}); r.fail()) {
    return r;
  }
  if (level >= DatabaseAccessLevel::Write) {
    if (auto r = checkNotReadOnly(); r.fail()) {
      return r;
    }
  }
  return {};
}

Result ExecContext::canSeeCollection(std::string_view db,
                                     std::string_view coll) const {
  using namespace auth::perms;
  return can(SeeCollection{.db{db}, .name{coll}});
}

Result ExecContext::canCreateCollection(std::string_view db,
                                        std::string_view coll) const {
  using namespace auth::perms;
  if (auto r = can(CreateCollection{.db{db}, .name{coll}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canDropCollection(std::string_view db,
                                      std::string_view coll) const {
  using namespace auth::perms;
  if (auto r = can(DropCollection{.db{db}, .name{coll}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canUseCollection(std::string_view db, std::string_view coll,
                                     CollectionAccessLevel level) const {
  using namespace auth::perms;
  if (auto r = can(UseCollection{.db{db}, .name{coll}, .level = level});
      r.fail()) {
    return r;
  }
  if (level >= CollectionAccessLevel::WriteData) {
    if (auto r = checkNotReadOnly(); r.fail()) {
      return r;
    }
  }
  return {};
}

Result ExecContext::canDumpCollection(std::string_view db,
                                      std::string_view coll) const {
  using namespace auth::perms;
  return can(DumpCollection{.db{db}, .name{coll}});
}

Result ExecContext::canRestoreCollection(std::string_view db,
                                         std::string_view coll,
                                         bool overwrite) const {
  using namespace auth::perms;
  if (auto r =
          can(RestoreCollection{.db{db}, .name{coll}, .overwrite = overwrite});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canRestoreCreateIndex(std::string_view db,
                                          std::string_view coll) const {
  using namespace auth::perms;
  if (auto r = can(RestoreCreateIndex{.db{db}, .collName{coll}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canRestoreCreateView(
    std::string_view db, std::string_view viewName,
    std::vector<std::string> linkedCollNames) const {
  using namespace auth::perms;
  if (auto r =
          can(RestoreCreateView{.db{db},
                                .viewName{viewName},
                                .linkedCollNames{std::move(linkedCollNames)}});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canRestoreDropView(std::string_view db,
                                       std::string_view view) const {
  using namespace auth::perms;
  if (auto r = can(RestoreDropView{.db{db}, .viewName{view}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canRestoreWriteData(std::string_view db,
                                        std::string_view coll) const {
  using namespace auth::perms;
  if (auto r = can(RestoreWriteData{.db{db}, .collName{coll}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canCreateIndex(std::string_view db,
                                   std::string_view coll) const {
  using namespace auth::perms;
  if (auto r = can(UseCollection{
          .db{db}, .name{coll}, .level = CollectionAccessLevel::WriteMeta});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canDropIndex(std::string_view db,
                                 std::string_view coll) const {
  using namespace auth::perms;
  if (auto r = can(UseCollection{
          .db{db}, .name{coll}, .level = CollectionAccessLevel::WriteMeta});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canSeeView(std::string_view db,
                               std::string_view view) const {
  using namespace auth::perms;
  return can(SeeView{.db{db}, .name{view}});
}

Result ExecContext::canCreateView(
    std::string_view db, std::string_view view,
    std::vector<std::string> const& linkedCollections) const {
  using namespace auth::perms;
  if (auto r = can(CreateView{
          .db{db}, .name{view}, .linkedCollections{linkedCollections}});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canModifyView(
    std::string_view db, std::string_view view,
    std::vector<std::string> const& linkedCollections) const {
  using namespace auth::perms;
  if (auto r = can(ModifyView{
          .db{db}, .name{view}, .linkedCollections{linkedCollections}});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canDropView(
    std::string_view db, std::string_view view,
    std::vector<std::string> const& linkedCollections) const {
  using namespace auth::perms;
  if (auto r = can(DropView{
          .db{db}, .name{view}, .linkedCollections{linkedCollections}});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canUseView(std::string_view db, std::string_view viewName,
                               ViewAccessLevel requested) const {
  using namespace auth::perms;
  if (auto r = can(UseView{.db{db}, .name{viewName}, .level = requested});
      r.fail()) {
    return r;
  }
  if (requested == ViewAccessLevel::Modify) {
    if (auto r = checkNotReadOnly(); r.fail()) {
      return r;
    }
  }
  return {};
}

Result ExecContext::canRenameView(std::string_view db,
                                  std::string_view oldViewName,
                                  std::string_view newViewName) const {
  using namespace auth::perms;
  if (auto r = can(
          RenameView{.db{db}, .oldName{oldViewName}, .newName{newViewName}});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canSeeAnalyzer(std::string_view db,
                                   std::string_view analyzer) const {
  using namespace auth::perms;
  return can(SeeAnalyzer{.db{db}, .name{analyzer}});
}

Result ExecContext::canCreateAnalyzer(std::string_view db,
                                      std::string_view analyzer) const {
  using namespace auth::perms;
  if (auto r = can(CreateAnalyzer{.db{db}, .name{analyzer}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canDropAnalyzer(std::string_view db,
                                    std::string_view analyzer) const {
  using namespace auth::perms;
  if (auto r = can(DropAnalyzer{.db{db}, .name{analyzer}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canUseAnalyzer(std::string_view db,
                                   std::string_view analyzer,
                                   AnalyzerAccessLevel level) const {
  using namespace auth::perms;
  if (auto r = can(UseAnalyzer{.db{db}, .name{analyzer}, .level = level});
      r.fail()) {
    return r;
  }
  if (level == AnalyzerAccessLevel::Modify) {
    if (auto r = checkNotReadOnly(); r.fail()) {
      return r;
    }
  }
  return {};
}

Result ExecContext::canSeeGraph(std::string_view db,
                                std::string_view graph) const {
  using namespace auth::perms;
  return can(SeeGraph{.db{db}, .name{graph}});
}

Result ExecContext::canCreateGraph(
    std::string_view db, std::string_view graph,
    std::span<std::string> collectionNamesToCreate,
    std::span<std::string> collectionNamesToRead) const {
  using namespace auth::perms;
  if (auto r =
          can(CreateGraph{.db{db},
                          .name{graph},
                          .collectionNamesToCreate{collectionNamesToCreate},
                          .collectionNamesToRead{collectionNamesToRead}});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canDropGraph(std::string_view db, std::string_view graph,
                                 std::span<std::string> collectionNames) const {
  using namespace auth::perms;
  if (auto r = can(
          DropGraph{.db{db}, .name{graph}, .collectionNames{collectionNames}});
      r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

Result ExecContext::canUseGraph(std::string_view db, std::string_view graph,
                                GraphAccessLevel const level) const {
  using namespace auth::perms;
  if (auto r = can(UseGraph{.db{db}, .name{graph}, .level = level}); r.fail()) {
    return r;
  }
  if (level == GraphAccessLevel::Modify) {
    if (auto r = checkNotReadOnly(); r.fail()) {
      return r;
    }
  }
  return {};
}

/// @brief returns true if the user can be read
Result ExecContext::canReadUser(std::string_view userName) const {
  using namespace auth::perms;
  // We implement one exception here: A user can read itself, we forbid
  // this, though, if the request was not authenticated, just to be safe:
  // We do this distinction here such that we do not have to implement
  // it separately for Classic and RBAC.
  if (!_authMode.isUnauthenticated() && userName == user()) {
    return {};
  }
  return can(ReadUser{.name{userName}});
}

/// @brief returns true if the given user may be created.
Result ExecContext::canCreateUser(std::string_view userName) const {
  using namespace auth::perms;
  if (auto r = can(CreateUser{.name{userName}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

/// @brief returns true if the given user may be dropped.
Result ExecContext::canDropUser(std::string_view userName) const {
  using namespace auth::perms;
  if (auto r = can(DropUser{.name{userName}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

/// @brief returns true if the given user's own profile (password, active
/// flag, config blob) may be modified. Note that everybody can modify
/// their own profile (if only to change the password).
Result ExecContext::canModifyUserProfile(std::string_view userName) const {
  using namespace auth::perms;
  Result r = {};
  // We implement one exception here: A user can read itself, we forbid
  // this, though, if the request was not authenticated, just to be safe:
  // We do this distinction here such that we do not have to implement
  // it separately for Classic and RBAC.
  if (_authMode.isUnauthenticated() || userName != user()) {
    r = can(ModifyUserProfile{.name{userName}});
  }
  if (r.fail()) {
    return r;
  }
  if (r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

/// @brief returns true if the given user's permissions on databases and
/// collections may be granted/revoked.
Result ExecContext::canGrantUserPermissions(std::string_view userName) const {
  using namespace auth::perms;
  if (auto r = can(GrantUserPermissions{.name{userName}}); r.fail()) {
    return r;
  }
  if (auto r = checkNotReadOnly(); r.fail()) {
    return r;
  }
  return {};
}

/// @brief returns true for each user which can be read
std::vector<bool> ExecContext::canReadUsers(
    std::span<std::string_view> users) const {
  using namespace auth::perms;
  auto canRead = [this](auto&& user) {
    // FIXME: Here is a morally unnecessary copy:
    return can(ReadUser{.name = std::string{user}}).ok();
  };
  auto view = users | std::views::transform(canRead);
  return std::vector<bool>{view.begin(), view.end()};
}

ExecContextScope::ExecContextScope(
    std::shared_ptr<ExecContext const> exe) noexcept
    : _old(std::move(exe)) {
  std::swap(ExecContext::CURRENT, _old);
}

ExecContextScope::~ExecContextScope() noexcept {
  std::swap(ExecContext::CURRENT, _old);
}

ExecContextSuperuserScope::ExecContextSuperuserScope()
    : _old(ExecContext::CURRENT) {
  ExecContext::CURRENT = getSuperuserContextFrom(_old.get());
}

ExecContextSuperuserScope::ExecContextSuperuserScope(bool cond)
    : _old(ExecContext::CURRENT) {
  if (cond) {
    ExecContext::CURRENT = getSuperuserContextFrom(_old.get());
  }
}

auto ExecContextSuperuserScope::getSuperuserContextFrom(
    ExecContext const* const old) -> std::shared_ptr<ExecContext const> {
  // save the original request for audit logging, if there is one
  if (old != nullptr && old->request().has_value()) {
    // NOTE we could store the vocbase as well, but I'm unsure if that's
    // helpful
    //      (note that an exec context contains a request iff it
    //      contains a vocbase)
    return std::make_shared<ExecContext>(
        ExecContext::ConstructorToken{},
        AuthMode{AuthMode::Superuser{*old->request()}}, old->_isRestApiHardened,
        nullptr);
  } else {
    return ExecContext::Superuser;
  }
}
