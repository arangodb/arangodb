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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "VocbaseContext.h"

#include "Auth/UserManager.h"
#include "Auth/Rbac/RbacFeature.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "VocBase/vocbase.h"

using namespace arangodb::rest;

namespace arangodb {

VocbaseContext::VocbaseContext(ConstructorToken, AuthMode authMode,
                               GeneralRequest& req, TRI_vocbase_t& vocbase)
    : ExecContext(ExecContext::ConstructorToken{}, std::move(authMode)),
#ifdef USE_ENTERPRISE
      _request(std::move(req)),
#endif
      _vocbase(vocbase),
      _canceled(false) {
  // _vocbase has already been refcounted for us
  TRI_ASSERT(!_vocbase.isDangling());
}

VocbaseContext::~VocbaseContext() {
  TRI_ASSERT(!_vocbase.isDangling());
  _vocbase.release();
}

std::shared_ptr<VocbaseContext> VocbaseContext::create(
    AuthenticationFeature& authenticationFeature, RbacFeature& rbacFeature,
    GeneralRequest& req, TRI_vocbase_t& vocbase) {
  auto authMode = AuthMode{[&]() -> AuthMode::Any {
    bool isSuperUser = req.authenticated() && req.user().empty() &&
                       req.authenticationMethod() == AuthenticationMethod::JWT;
    if (isSuperUser) {
      return AuthMode::Superuser();
    }

    if (!authenticationFeature.isActive()) {
      return AuthMode::Disabled(req.user());
    }

    auto* userManager = authenticationFeature.userManager();
    // I assume that `userManager == nullptr` implies `!req.authenticated()`,
    // which, if true, makes the nullptr-check superfluous. But I don't have
    // the time to verify this just now.
    // In a Cluster, with authentication enabled, on DBServers and Agents,
    // there is no UserManager, but at least the SuperUser can be authenticated.
    // Maybe it'd be possible with a JWT token to create an authenticated
    // request, in which case this check would be necessary. We might want to
    // catch that earlier, though.
    if (!req.authenticated() || userManager == nullptr) {
      return AuthMode::Unauthenticated{req.user()};
    }

    if (auto* rbacService = rbacFeature.service(); rbacService != nullptr) {
      return AuthMode::Rbac(authenticationFeature, *rbacService, req.user(),
                            req.jwtToken());
    }

    ADB_PROD_ASSERT(userManager != nullptr);
    return AuthMode::Classic(*userManager, req.user());
  }()};

  return std::make_shared<VocbaseContext>(ConstructorToken{},
                                          std::move(authMode), req, vocbase);
}

// std::shared_ptr<VocbaseContext> VocbaseContext::create_old(
//    AuthenticationFeature& authenticationFeature, RbacFeature& rbacFeature,
//    GeneralRequest& req, TRI_vocbase_t& vocbase) {
//  // _vocbase has already been refcounted for us
//  TRI_ASSERT(!vocbase.isDangling());
//
//  // superusers will have an empty username. This MUST be invalid
//  // for users authenticating with name / password
//  bool isSuperUser = req.authenticated() && req.user().empty() &&
//                     req.authenticationMethod() == AuthenticationMethod::JWT;
//  if (isSuperUser) {
//    return std::make_shared<VocbaseContext>(
//        ConstructorToken{}, authenticationFeature, rbacFeature, req, vocbase,
//        ExecContext::Type::Internal,
//        /*sysLevel*/ auth::Level::RW,
//        /*dbLevel*/ auth::Level::RW, true);
//  }
//
//  if (!authenticationFeature.isActive()) {
//    if (ServerState::readOnly()) {
//      // special read-only case
//      return std::make_shared<VocbaseContext>(
//          ConstructorToken{}, authenticationFeature, rbacFeature, req,
//          vocbase, ExecContext::Type::Internal,
//          /*sysLevel*/ auth::Level::RO,
//          /*dbLevel*/ auth::Level::RO, true);
//    }
//    return std::make_shared<VocbaseContext>(
//        ConstructorToken{}, authenticationFeature, rbacFeature, req, vocbase,
//        req.user().empty() ? ExecContext::Type::Internal
//                           : ExecContext::Type::Default,
//        /*sysLevel*/ auth::Level::RW,
//        /*dbLevel*/ auth::Level::RW, true);
//  }
//
//  if (!req.authenticated()) {
//    return std::make_shared<VocbaseContext>(
//        ConstructorToken{}, authenticationFeature, rbacFeature, req, vocbase,
//        ExecContext::Type::Default,
//        /*sysLevel*/ auth::Level::NONE,
//        /*dbLevel*/ auth::Level::NONE, false);
//  }
//
//  if (req.user().empty()) {
//    std::string msg = "only jwt can be used to authenticate as superuser";
//    LOG_TOPIC("2d0f6", WARN, Logger::AUTHENTICATION) << msg;
//    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, std::move(msg));
//  }
//
//  auth::UserManager* um = authenticationFeature.userManager();
//  if (um == nullptr) {
//    TRI_ASSERT(!ServerState::instance()->isSingleServerOrCoordinator());
//    LOG_TOPIC("aae8a", WARN, Logger::AUTHENTICATION)
//        << "users are not supported on this server";
//    return nullptr;
//  }
//
//  auth::Level dbLvl =
//      um->databaseAuthLevel(req.user(), req.databaseName(), false);
//  auth::Level sysLvl = dbLvl;
//  if (req.databaseName() != StaticStrings::SystemDatabase) {
//    sysLvl =
//        um->databaseAuthLevel(req.user(), StaticStrings::SystemDatabase,
//        false);
//  }
//  bool isAdminUser = (sysLvl == auth::Level::RW);
//  if (!isAdminUser && ServerState::readOnly()) {
//    // in case we are in read-only mode, we need to re-check the original
//    // permissions
//    isAdminUser =
//        um->databaseAuthLevel(req.user(), StaticStrings::SystemDatabase,
//                              true) == auth::Level::RW;
//  }
//
//  return std::make_shared<VocbaseContext>(
//      ConstructorToken{}, authenticationFeature, rbacFeature, req, vocbase,
//      ExecContext::Type::Default,
//      /*sysLevel*/ sysLvl,
//      /*dbLevel*/ dbLvl, isAdminUser);
//}
//
//// TODO Try to rewrite ::create and make it more readable.
////      After that, try to consolidate the arguments to the VocbaseContext
////      constructor (and maybe the corresponding members):
////      It seems sensible to separate those that are related to internal
////      authorization, and possibly to separate authentication and
////      authorization. Auth on, off, or RBAC, might play a role as well. It
////      might also make sense to separate SuperUser (and possibly internal+ro)
////      more distinctly:
////      E.g. have some variant (SuperUser | Admin | AdminRo | User{...})
////      It might also simplify things to separate / move the
////      ServerState::readOnly check. Note that this is additionally done in
/// the /      UserManager when asking for an auth level, and reducing it to RO
/// if /      necessary.
// std::shared_ptr<VocbaseContext> VocbaseContext::create2(
//     AuthenticationFeature& authenticationFeature, RbacFeature& rbacFeature,
//     GeneralRequest& req, TRI_vocbase_t& vocbase) {
//   if (req.authenticated() && req.user().empty() &&
//       req.authenticationMethod() != AuthenticationMethod::JWT) {
//     std::string msg = "only jwt can be used to authenticate as superuser";
//     LOG_TOPIC("2d0f6", WARN, Logger::AUTHENTICATION) << msg;
//     THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, std::move(msg));
//   }
//
//   bool isSuperUser = req.authenticated() && req.user().empty() &&
//                      req.authenticationMethod() == AuthenticationMethod::JWT;
//
//   auto execContextType = ExecContext::Type{};
//   if (isSuperUser || (!authenticationFeature.isActive() &&
//                       (ServerState::readOnly() || req.user().empty()))) {
//     execContextType = ExecContext::Type::Internal;
//   } else {
//     execContextType = ExecContext::Type::Default;
//   }
//
//   auto* const um = authenticationFeature.userManager();
//   if (req.authenticated() && !req.user().empty() &&
//       authenticationFeature.isActive()) {
//     TRI_ASSERT((um != nullptr) ==
//                ServerState::instance()->isSingleServerOrCoordinator());
//     if (um == nullptr) {
//       LOG_TOPIC("aae8a", WARN, Logger::AUTHENTICATION)
//           << "users are not supported on this server";
//       return nullptr;
//     }
//   }
//
//   auto systemAuthLevel = auth::Level{};
//   auto dbAuthLevel = auth::Level{};
//   if (isSuperUser) {
//     systemAuthLevel = auth::Level::RW;
//     dbAuthLevel = auth::Level::RW;
//   } else if (!authenticationFeature.isActive()) {
//     auto level = ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
//     systemAuthLevel = level;
//     dbAuthLevel = level;
//   } else if (req.authenticated()) {
//     TRI_ASSERT(ServerState::instance()->isSingleServerOrCoordinator());
//     TRI_ASSERT(um != nullptr);
//     dbAuthLevel = um->databaseAuthLevel(req.user(), req.databaseName(),
//     false); systemAuthLevel =
//         um->databaseAuthLevel(req.user(), StaticStrings::SystemDatabase,
//         false);
//   } else {
//     systemAuthLevel = auth::Level::NONE;
//     dbAuthLevel = auth::Level::NONE;
//   }
//
//   auto isAdminUser = bool{};
//   isAdminUser = isSuperUser || !authenticationFeature.isActive() ||
//                 systemAuthLevel == auth::Level::RW;
//
//   return std::make_shared<VocbaseContext>(
//       ConstructorToken{}, authenticationFeature, rbacFeature, req, vocbase,
//       execContextType, systemAuthLevel, dbAuthLevel, isAdminUser);
// }

void VocbaseContext::forceSuperuser() {
  // TODO See if we can remove this method. It might be sufficient to use
  //      SuperUserExecContext, instead of modifying the current context.
  //      One contradictory thought: We might want to be able to track stuff
  //      about the original request, e.g. for auditing.
  //      But note that currently, this explicitly does not happen: Setting the
  //      _type to Internal causes the user to be ignored in AuditRequestInfo,
  //      and the request path to be ignored in EventsEE.cpp.
  //      If we remove it, we can remove the reset() method in AuthMode as well.
  _authMode.reset<AuthMode::Superuser>();
}

#ifdef USE_ENTERPRISE
std::string VocbaseContext::authMethod() const {
  switch (_request.authenticationMethod()) {
    case rest::AuthenticationMethod::BASIC:
      return "http basic";
    case rest::AuthenticationMethod::JWT:
      return "http jwt";
    case rest::AuthenticationMethod::NONE:
      break;
  }
  return "n/a";
}
#endif

}  // namespace arangodb
