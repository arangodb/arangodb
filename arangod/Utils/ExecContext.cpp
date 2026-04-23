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

#include "ExecContext.h"

#include "Basics/Result.h"

#include <ranges>
#include <variant>

using namespace arangodb;

thread_local std::shared_ptr<ExecContext const> ExecContext::CURRENT = nullptr;

std::shared_ptr<ExecContext const> const ExecContext::Superuser =
    std::make_shared<ExecContext const>(ConstructorToken{},
                                        AuthMode{AuthMode::Superuser{}});

/// Should always contain a reference to current user context
ExecContext const& ExecContext::current() {
  if (CURRENT != nullptr) {
    return *CURRENT;
  }
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

ExecContext::ExecContext(ConstructorToken, AuthMode authMode)
    : _authMode(std::move(authMode)) {}

bool ExecContext::isAuthEnabled() const {
  /// TODO Note there's a subtle change in the behavior by checking
  ///      _authMode instead of the AuthenticationFeature:
  ///     When authentication is disabled, but the request comes with
  ///     a valid JWT superuser token, this now returns true.
  ///     I'd like to get rid of this method completely; but if that
  ///     doesn't work out, we have to a) check that this doesn't
  ///     break any caller, and b) we should change the name.
  return !_authMode.isDisabled();
}

Result ExecContext::canUseAdminAction(rbac::Category::Any const& action) const {
  using namespace auth::perms;
  return can(Admin{.action{action}});
}

Result ExecContext::canUseHardenedAction(
    rbac::Category::Any const& action) const {
  using namespace auth::perms;
  return can(HardenedAdmin{.action{action}});
}

Result ExecContext::canSeeDatabase(std::string_view db) const {
  using namespace auth::perms;
  return can(SeeDatabase{.name{db}});
}

Result ExecContext::canCreateDatabase(std::string_view db) const {
  using namespace auth::perms;
  return can(CreateDatabase{.name{db}});
}

Result ExecContext::canDropDatabase(std::string_view db) const {
  using namespace auth::perms;
  return can(DropDatabase{.name{db}});
}

Result ExecContext::canUseDatabase(std::string_view db,
                                   DatabaseAccessLevel level) const {
  using namespace auth::perms;
  return can(UseDatabase{.name{db}, .level = level});
}

Result ExecContext::canSeeCollection(std::string_view db,
                                     std::string_view coll) const {
  using namespace auth::perms;
  return can(SeeCollection{.db{db}, .name{coll}});
}

Result ExecContext::canCreateCollection(std::string_view db,
                                        std::string_view coll) const {
  using namespace auth::perms;
  return can(CreateCollection{.db{db}, .name{coll}});
}

Result ExecContext::canDropCollection(std::string_view db,
                                      std::string_view coll) const {
  using namespace auth::perms;
  return can(DropCollection{.db{db}, .name{coll}});
}

Result ExecContext::canUseCollection(std::string_view db, std::string_view coll,
                                     CollectionAccessLevel level) const {
  using namespace auth::perms;
  return can(UseCollection{.db{db}, .name{coll}});
}

Result ExecContext::canCreateIndex(std::string_view db,
                                   std::string_view coll) const {
  using namespace auth::perms;
  return can(UseCollection{
      .db{db}, .name{coll}, .level = CollectionAccessLevel::WriteMeta});
}

Result ExecContext::canDropIndex(std::string_view db,
                                 std::string_view coll) const {
  using namespace auth::perms;
  return can(UseCollection{
      .db{db}, .name{coll}, .level = CollectionAccessLevel::WriteMeta});
}

Result ExecContext::canSeeView(std::string_view db,
                               std::string_view view) const {
  using namespace auth::perms;
  return can(SeeView{.db{db}, .name{view}});
}

Result ExecContext::canCreateView(std::string_view db,
                                  std::string_view view) const {
  using namespace auth::perms;
  return can(CreateView{.db{db}, .name{view}});
}

Result ExecContext::canDropView(std::string_view db, std::string_view view,
                                std::vector<std::string> collections) const {
  using namespace auth::perms;
  return can(DropView{
      .db{db}, .name{view}, .linkedCollections{std::move(collections)}});
}

Result ExecContext::canUseView(std::string_view db, std::string_view viewName,
                               ViewAccessLevel requested) const {
  using namespace auth::perms;
  return can(UseView{.db{db}, .name{viewName}, .level = requested});
}

Result ExecContext::canRenameView(std::string_view db,
                                  std::string_view oldViewName,
                                  std::string_view newViewName) const {
  using namespace auth::perms;
  return can(RenameView{});
}

Result ExecContext::canRenameView(std::string_view db,
                                  std::string_view oldViewName,
                                  std::string_view newViewName,
                                  std::vector<std::string> collections) const {
  using namespace auth::perms;
  return can(RenameView{.db{db},
                        .oldName{oldViewName},
                        .newName{newViewName},
                        .linkedCollections{std::move(collections)}});
}


Result ExecContext::canSeeAnalyzer(std::string_view db,
                                   std::string_view analyzer) const {
  using namespace auth::perms;
  return can(SeeAnalyzer{.db{db}, .name{analyzer}});
}

Result ExecContext::canCreateAnalyzer(std::string_view db,
                                      std::string_view analyzer) const {
  using namespace auth::perms;
  return can(CreateAnalyzer{.db{db}, .name{analyzer}});
}

Result ExecContext::canDropAnalyzer(std::string_view db,
                                    std::string_view analyzer) const {
  using namespace auth::perms;
  return can(DropAnalyzer{.db{db}, .name{analyzer}});
}

Result ExecContext::canUseAnalyzer(std::string_view db,
                                   std::string_view analyzer,
                                   AnalyzerAccessLevel level) const {
  using namespace auth::perms;
  return can(UseAnalyzer{.db{db}, .name{analyzer}, .level = level});
}

/// @brief returns true if the user can be read
bool ExecContext::canReadUser(std::string_view user) const {
  using namespace auth::perms;
  return can(ReadUser{.name{user}}).ok();
  // TODO
  // Pseudocode:
  // if superuser: true
  // if self: true
  // if rbac:
  //   return AdminReadUser(user)
  // else:
  //   return RW(_system)
  return true;
}

/// @brief returns true if the user can be modified, note that everybody
/// can modify themselves (if only to change the password).
bool ExecContext::canWriteUser(std::string_view user) const {
  using namespace auth::perms;
  return can(WriteUser{.name{user}}).ok();
  // TODO
  // Pseudocode:
  // if superuser: true
  // if self: true
  // if rbac:
  //   return AdminWriteUser(user)
  // else:
  //   return RW(_system)
  return true;
}

/// @brief returns true for each user which can be read
std::vector<bool> ExecContext::canReadUsers(
    std::vector<std::string> const& users) const {
  using namespace auth::perms;
  auto canRead = [this](auto&& user) { return can(ReadUser{.name = user}).ok(); };
  auto view = users | std::views::transform(canRead);
  return std::vector<bool>{view.begin(), view.end()};
}

ExecContextScope::ExecContextScope(std::shared_ptr<ExecContext const> exe)
    : _old(std::move(exe)) {
  std::swap(ExecContext::CURRENT, _old);
}

ExecContextScope::~ExecContextScope() { std::swap(ExecContext::CURRENT, _old); }
