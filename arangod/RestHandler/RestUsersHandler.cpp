////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include "RestUsersHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Auth/UserManager.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/LogSecurity.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>

#include <string>
#include <string_view>

namespace {

/// @brief whether security logging of account management events is done on
/// this server. this is only the case on single servers and coordinators,
/// which are the only servers on which users can be managed by clients.
bool shouldLogSecurityEvents() noexcept {
  return arangodb::ServerState::instance()->isSingleServerOrCoordinator();
}

/// @brief security log for an account management activity (creating,
/// replacing, updating or removing a user, granting or revoking permissions,
/// changing a user's configuration data). both successful and failed
/// attempts are logged. a denied attempt is in addition logged as an access
/// control violation by ExecContext, so it is not tagged as a violation here.
/// `details` must never contain secrets such as passwords.
void logAccountManagement(arangodb::GeneralRequest const& req,
                          std::string_view operation, std::string_view user,
                          arangodb::Result const& result,
                          std::string_view details = {}) {
  using namespace arangodb;
  if (result.ok()) {
    LOG_SECURITY_IF("b5bde", INFO, Logger::AUTHENTICATION,
                    Security::AccountManagement, shouldLogSecurityEvents())
        << "account management: " << operation << " for user '" << user
        << "' succeeded" << (details.empty() ? "" : ", ") << details
        << ", actor='" << req.user()
        << "', client=" << req.connectionInfo().fullClient()
        << ", request=" << GeneralRequest::translateMethod(req.requestType())
        << " " << req.fullUrl();
  } else {
    LOG_SECURITY_IF("ff4bc", INFO, Logger::AUTHENTICATION,
                    Security::AccountManagement, shouldLogSecurityEvents())
        << "account management: " << operation << " for user '" << user
        << "' failed: error=" << result.errorNumber() << ", reason='"
        << result.errorMessage() << "'" << (details.empty() ? "" : ", ")
        << details << ", actor='" << req.user()
        << "', client=" << req.connectionInfo().fullClient()
        << ", request=" << GeneralRequest::translateMethod(req.requestType())
        << " " << req.fullUrl();
  }
}

/// @brief describes which attributes of a user are set by a create, replace
/// or update request, without revealing the password.
std::string describeUserChange(arangodb::velocypack::Slice body) {
  std::string out = "changes={";
  bool first = true;
  auto add = [&](std::string_view what) {
    if (!first) {
      out.append(", ");
    }
    out.append(what);
    first = false;
  };
  if (body.isObject()) {
    if (body.get("passwd").isString()) {
      add("password");
    }
    if (auto active = body.get("active"); active.isBool()) {
      add(active.getBool() ? "active=true" : "active=false");
    }
    if (!body.get("extra").isNone()) {
      add("extra");
    }
  }
  out.push_back('}');
  return out;
}

/// @brief describes a database or collection permission target.
std::string describePermissionTarget(std::string_view db,
                                     std::string_view coll) {
  std::string out = "database='";
  out.append(db);
  out.push_back('\'');
  if (!coll.empty()) {
    out.append(", collection='");
    out.append(coll);
    out.push_back('\'');
  }
  return out;
}

// a collection exists in database or a wildcard was specified
arangodb::Result existsCollection(
    arangodb::application_features::ApplicationServer& server,
    std::string const& database, std::string const& collection) {
  if (!server.hasFeature<arangodb::DatabaseFeature>()) {
    return arangodb::Result(TRI_ERROR_INTERNAL,
                            "failure to find feature 'Database'");
  }

  static constexpr std::string_view wildcard("*");

  if (wildcard == database) {
    return arangodb::Result();  // wildcard always matches
  }

  auto& databaseFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto vocbase = databaseFeature.useDatabase(database);

  if (!vocbase) {
    return arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (wildcard == collection) {
    return arangodb::Result();  // wildcard always matches
  }

  return !arangodb::CollectionNameResolver(*vocbase).getCollection(collection)
             ? arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)
             : arangodb::Result();
}

}  // namespace

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestUsersHandler::RestUsersHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

// Mounted at /_api/user (prefix)
RestStatus RestUsersHandler::execute() {
  RequestType const type = _request->requestType();
  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af == nullptr || af->userManager() == nullptr) {
    // nullptr may happens during shutdown, or on Agency
    generateError(ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED);
    return RestStatus::DONE;
  }

  // Note that contrary to versions up to and including 3.12.9 writes are now
  // forbidden if the server is in read-only mode. This is intentional.
  switch (type) {
    case RequestType::GET:
      return getRequest(af->userManager());
    case RequestType::POST:
      return postRequest(af->userManager());
    case RequestType::PUT:
      return putRequest(af->userManager());
    case RequestType::PATCH:
      return patchRequest(af->userManager());
    case RequestType::DELETE_REQ:
      return deleteRequest(af->userManager());

    default:
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return RestStatus::DONE;
  }
}

async<Result> RestUsersHandler::checkDatabaseAccess() const {
  constexpr std::string_view pathPrefixApiUser("/_api/user/");

  auto const& path = _request->requestPath();

  if (_request->authenticated() && path.starts_with(pathPrefixApiUser)) {
    // No database access is required here (everybody may e.g. change their own
    // password)
    co_return Result{};
  }

  co_return co_await RestBaseHandler::checkDatabaseAccess();
}

/// helper to generate a compliant response for individual user requests
void RestUsersHandler::generateUserResult(rest::ResponseCode code,
                                          VPackBuilder const& doc) {
  VPackBuilder b;
  b(VPackValue(VPackValueType::Object))(
      StaticStrings::Error, VPackValue(false))(StaticStrings::Code,
                                               VPackValue((int)code))();
  b = VPackCollection::merge(doc.slice(), b.slice(), false);
  generateResult(code, b.slice());
}

RestStatus RestUsersHandler::getRequest(auth::UserManager* um) {
  auto& exec = ExecContext::current();

  std::vector<std::string> suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    if (auto r = exec.canUseAdminAction(auth::perms::AdminReadUsers{});
        r.fail()) {
      generateError(r);
      return RestStatus::DONE;
    }
    VPackBuilder usersArray = um->allUsers();
    TRI_ASSERT(usersArray.slice().isArray());
    std::vector<std::string_view> userList;
    userList.reserve(usersArray.slice().length());
    for (auto const& u : VPackArrayIterator(usersArray.slice())) {
      VPackSlice un = u.get("user");
      TRI_ASSERT(un.isString());
      userList.push_back(un.stringView());
    }
    // Now filter out those to which we have access:
    std::vector<bool> allowed = exec.canReadUsers(userList);
    TRI_ASSERT(allowed.size() == userList.size());
    VPackBuilder usersResult;
    {
      VPackArrayBuilder guard(&usersResult);
      for (size_t i = 0; i < allowed.size(); ++i) {
        if (allowed[i]) {
          usersResult.add(usersArray.slice()[i]);
        }
      }
    }
    generateOk(ResponseCode::OK, usersResult.slice());
  } else if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (auto r = exec.canReadUser(user); r.ok()) {
      VPackBuilder doc = um->serializeUser(user);
      generateUserResult(ResponseCode::OK, doc);
    } else {
      generateError(r);
    }
  } else {
    std::string const& user = suffixes[0];
    if (auto r = exec.canReadUser(user); r.fail()) {
      generateError(r);
      return RestStatus::DONE;
    }

    if (suffixes[1] == "database") {
      if (!(exec.isClassic() || exec.isSuperuserOrDisabled())) {
        generateError(Result{TRI_ERROR_FORBIDDEN, "Not allowed in RBAC mode."});
        return RestStatus::DONE;
      }
      if (suffixes.size() == 2) {
        //_api/user/<user>/database?full=<true/false>
        bool full = false;
        std::string const& param = _request->value("full", full);
        if (full) {
          full = StringUtils::boolean(param);
        }
        generateDatabaseResult(um, user, full);
      } else if (suffixes.size() == 3) {
        //_api/user/<user>/database/<dbname>
        // return specific database
        bool configured = _request->parsedValue("configured", false);
        auth::Level lvl = um->databaseAuthLevel(user, suffixes[2], configured);

        VPackBuilder data;
        data.add(VPackValue(convertFromAuthLevel(lvl)));
        generateOk(ResponseCode::OK, data.slice());
      } else if (suffixes.size() == 4) {
        bool configured = _request->parsedValue("configured", false);
        //_api/user/<user>/database/<dbname>/<collection>
        auth::Level lvl =
            um->collectionAuthLevel(user, suffixes[2], suffixes[3], configured);

        VPackBuilder data;
        data.add(VPackValue(convertFromAuthLevel(lvl)));
        generateOk(ResponseCode::OK, data.slice());
      } else {
        generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
      }
    } else if (suffixes[1] == "config") {
      //_api/user/<user>/config  (only used by WebUI)
      Result r;
      r = um->accessUser(user, [&](auth::User const& u) {
        VPackSlice resp = u.configData();
        if (suffixes.size() == 3 && resp.isObject()) {
          resp = resp.get(suffixes[2]);
        }
        generateOk(ResponseCode::OK,
                   resp.isNone() ? VPackSlice::nullSlice() : resp);
        return TRI_ERROR_NO_ERROR;
      });
      if (r.fail()) {
        generateError(r);
      }
    } else {
      generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    }
  }

  return RestStatus::DONE;
}

// generate response for /_api/user/database?full=true/false
void RestUsersHandler::generateDatabaseResult(auth::UserManager* um,
                                              std::string const& username,
                                              bool full) {
  // return list of databases
  VPackBuilder data;
  data.openObject();
  Result res = um->accessUser(username, [&, this](auth::User const& user) {
    server().getFeature<DatabaseFeature>().enumerateDatabases(
        [&](TRI_vocbase_t& vocbase) -> void {
          if (full) {
            auto lvl = user.configuredDBAuthLevel(vocbase.name());
            velocypack::ObjectBuilder b(&data, vocbase.name(), true);

            data.add("permission", VPackValue(convertFromAuthLevel(lvl)));

            velocypack::ObjectBuilder b2(&data, "collections", true);

            methods::Collections::enumerate(
                &vocbase,
                [&](std::shared_ptr<LogicalCollection> const& c) -> bool {
                  TRI_ASSERT(c);
                  lvl = user.configuredCollectionAuthLevel(vocbase.name(),
                                                           c->name());
                  data.add(c->name(),
                           velocypack::Value(convertFromAuthLevel(lvl)));
                  return true;
                });
            lvl = user.configuredCollectionAuthLevel(vocbase.name(), "*");
            data.add("*", velocypack::Value(convertFromAuthLevel(lvl)));
          } else {  // hide db's without access
            auto lvl = user.databaseAuthLevel(vocbase.name());

            if (lvl >= auth::Level::RO) {
              data.add(vocbase.name(), VPackValue(convertFromAuthLevel(lvl)));
            }
          }
        });

    if (full) {
      auth::Level lvl = user.databaseAuthLevel("*");
      data("*", VPackValue(VPackValueType::Object))(
          "permission", VPackValue(convertFromAuthLevel(lvl)))();
    }

    return TRI_ERROR_NO_ERROR;
  });
  data.close();
  if (res.ok()) {
    generateOk(ResponseCode::OK, data.slice());
  } else {
    generateError(res);
  }
}

// helper to create(0), replace(1), update(2) a user
static Result StoreUser(auth::UserManager* um, int mode,
                        std::string const& user, VPackSlice json) {
  std::string passwd;
  bool active = true;
  VPackSlice extra;
  if (json.isObject()) {
    VPackSlice s = json.get("passwd");
    passwd = s.isString() ? s.copyString() : "";
    s = json.get("active");
    active = s.isBool() ? s.getBool() : true;
    extra = json.get("extra");
  }

  Result r;
  if (mode == 0 || mode == 1) {
    r = um->storeUser(mode == 1, user, passwd, active, extra);
  } else if (mode == 2) {
    VPackBuilder doc = um->serializeUser(user);
    VPackSlice u = doc.slice();
    r = um->updateUser(
        user,
        [&](auth::User& entry) {
          if (json.isObject()) {
            if (json.get("passwd").isString()) {
              entry.updatePassword(passwd);
            }
            if (json.get("active").isBool()) {
              entry.setActive(active);
            }
          }

          VPackSlice oldExtra = u.get("extra");
          if (extra.isObject() && oldExtra.isObject()) {
            // Both `extra` and `oldExtra` are objects, so perform a deep
            // merge.
            entry.setUserData(
                VPackCollection::merge(oldExtra, extra, true, false));
          } else if (!extra.isNone()) {
            // `extra` or `oldExtra` is not an object, so a deep merge is not
            // possible. Just overwrite the old value.
            entry.setUserData(VPackBuilder(extra));
          }
          return TRI_ERROR_NO_ERROR;
        },
        auth::UserManager::RetryOnConflict::No);
  }

  return r;
}

RestStatus RestUsersHandler::postRequest(auth::UserManager* um) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();
  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess || !body.isObject()) {
    generateResult(rest::ResponseCode::OK, VPackSlice());
    return RestStatus::DONE;
  }

  if (suffixes.empty()) {
    // create a new user
    VPackSlice s = body.get("user");
    std::string user = s.isString() ? s.copyString() : "";
    auto& exec = ExecContext::current();
    if (auto r = exec.canCreateUser(user); r.ok()) {
      // create user
      auto r2 = StoreUser(um, 0, user, body);
      logAccountManagement(*_request, "create user", user, r2,
                           describeUserChange(body));
      if (r2.ok()) {
        VPackBuilder doc = um->serializeUser(user);
        generateUserResult(ResponseCode::CREATED, doc);
      } else {
        generateError(r2);
      }
    } else {
      logAccountManagement(*_request, "create user", user, r);
      generateError(r);
    }

  } else if (suffixes.size() == 1) {
    // validate username / password
    std::string const& user = suffixes[0];
    std::string password;
    VPackSlice s = body.get("passwd");
    if (s.isString()) {
      password = s.copyString();
    }
    std::string un;
    bool const valid = um->checkCredentials(user, password, un);
    // this is a password check, so it is an authentication event rather
    // than an account management activity. failed checks are in addition
    // security violations (e.g. password guessing).
    LOG_SECURITY_IF("c8b56", INFO, Logger::AUTHENTICATION,
                    valid ? Security::Authentication
                          : Security::Authentication | Security::Violations,
                    shouldLogSecurityEvents())
        << "credential validation for user '" << user
        << (valid ? "' succeeded" : "' failed") << ", actor='"
        << _request->user()
        << "', client=" << _request->connectionInfo().fullClient()
        << ", request="
        << GeneralRequest::translateMethod(_request->requestType()) << " "
        << _request->fullUrl();
    if (valid) {
      generateOk(rest::ResponseCode::OK, VPackSlice::trueSlice());
    } else {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_USER_NOT_FOUND);
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  }
  return RestStatus::DONE;
}

RestStatus RestUsersHandler::putRequest(auth::UserManager* um) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();
  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  auto& exec = ExecContext::current();
  if (suffixes.size() == 1) {
    // replace existing user
    std::string const& user = suffixes[0];
    if (auto r = exec.canModifyUserProfile(user); r.fail()) {
      logAccountManagement(*_request, "replace user", user, r);
      generateError(r);
      return RestStatus::DONE;
    }

    Result r = StoreUser(um, 1, user, body);
    logAccountManagement(*_request, "replace user", user, r,
                         describeUserChange(body));
    if (r.ok()) {
      VPackBuilder doc = um->serializeUser(user);
      generateUserResult(ResponseCode::OK, doc);
    } else {
      generateError(r);
    }
  } else if (suffixes.size() == 3 || suffixes.size() == 4) {
    // update database / collection permissions
    std::string const& name = suffixes[0];

    if (suffixes[1] == "database") {
      // update a user's permissions
      std::string const& db = suffixes[2];
      std::string coll = suffixes.size() == 4 ? suffixes[3] : "";
      std::string target = describePermissionTarget(db, coll);

      if (auto r = exec.canGrantUserPermissions(name); r.fail()) {
        logAccountManagement(*_request, "grant permission", name, r, target);
        generateError(r);
        return RestStatus::DONE;
      }

      // validate that the collection is present
      if (suffixes.size() > 3) {
        auto res = existsCollection(server(), db, coll);

        if (!res.ok()) {
          logAccountManagement(*_request, "grant permission", name, res,
                               target);
          generateError(res);

          return RestStatus::DONE;
        }
      }

      if (!body.isObject() || !body.get("grant").isString()) {
        logAccountManagement(*_request, "grant permission", name,
                             Result{TRI_ERROR_BAD_PARAMETER,
                                    "missing or invalid 'grant' attribute"},
                             target);
        generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);

        return RestStatus::DONE;
      }

      VPackSlice grant = body.get("grant");
      auth::Level lvl = auth::convertToAuthLevel(grant);
      target.append(", level='");
      target.append(convertFromAuthLevel(lvl));
      target.push_back('\'');

      // contains response in case of success
      VPackBuilder b;

      b(VPackValue(VPackValueType::Object));

      Result r = um->updateUser(
          name,
          [&](auth::User& entry) {
            if (coll.empty()) {
              entry.grantDatabase(db, lvl);
              b(db, VPackValue(convertFromAuthLevel(lvl)))();
            } else {
              entry.grantCollection(db, coll, lvl);
              b(db + "/" + coll, VPackValue(convertFromAuthLevel(lvl)))();
            }

            return TRI_ERROR_NO_ERROR;
          },
          auth::UserManager::RetryOnConflict::No);

      logAccountManagement(*_request, "grant permission", name, r, target);
      if (r.ok()) {
        generateUserResult(ResponseCode::OK, b);
      } else {
        generateError(r);
      }
    } else if (suffixes[1] == "config") {
      // update internal config data, used in the admin dashboard
      std::string const& key = suffixes[2];
      std::string const target = "config key='" + key + "'";
      if (auto r = exec.canModifyUserProfile(name); r.fail()) {
        logAccountManagement(*_request, "update user config", name, r, target);
        generateError(r);
        return RestStatus::DONE;
      }

      Result res;
      if (!body.isNone()) {
        // The API expects: { value : <toStore> }
        // If we get sth else than the above it is translated
        // to a remove of the config option.
        res = um->updateUser(
            name,
            [&](auth::User& u) {
              VPackSlice newVal = body;
              VPackSlice oldConf = u.configData();
              if (!newVal.isObject() || !newVal.hasKey("value")) {
                if (oldConf.isObject() && oldConf.hasKey(key)) {
                  u.setConfigData(VPackCollection::remove(
                      oldConf, std::unordered_set<std::string>{key}));
                }  // Nothing to do. We do not have a config yet.
              } else {  // We need to merge the new key into the config
                VPackBuilder b;
                b(VPackValue(VPackValueType::Object))(key,
                                                      newVal.get("value"))();

                if (oldConf.isObject() &&
                    !oldConf.isEmptyObject()) {  // merge value in
                  u.setConfigData(
                      VPackCollection::merge(oldConf, b.slice(), false));
                } else {
                  u.setConfigData(std::move(b));
                }
              }

              return TRI_ERROR_NO_ERROR;
            },
            auth::UserManager::RetryOnConflict::No);
      }
      logAccountManagement(*_request, "update user config", name, res, target);
      if (res.ok()) {
        resetResponse(ResponseCode::OK);
      } else {
        generateError(res);
      }
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  }
  return RestStatus::DONE;
}

RestStatus RestUsersHandler::patchRequest(auth::UserManager* um) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  auto& exec = ExecContext::current();
  if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (auto r = exec.canModifyUserProfile(user); r.ok()) {
      // update a user
      auto r2 = StoreUser(um, 2, user, body);
      logAccountManagement(*_request, "update user", user, r2,
                           describeUserChange(body));
      if (r2.ok()) {
        VPackBuilder doc = um->serializeUser(user);
        generateUserResult(ResponseCode::OK, doc);
      } else {
        generateError(r2);
      }
    } else {
      logAccountManagement(*_request, "update user", user, r);
      generateError(r);
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  }
  return RestStatus::DONE;
}

RestStatus RestUsersHandler::deleteRequest(auth::UserManager* um) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();

  auto& exec = ExecContext::current();
  if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (auto r = exec.canDropUser(user); r.fail()) {
      logAccountManagement(*_request, "remove user", user, r);
      generateError(r);
      return RestStatus::DONE;
    }

    Result r = um->removeUser(user);
    logAccountManagement(*_request, "remove user", user, r);
    if (r.ok()) {
      VPackBuilder b;
      b(VPackValue(VPackValueType::Object))(
          StaticStrings::Error, VPackValue(false))(StaticStrings::Code,
                                                   VPackValue(202))();
      generateResult(ResponseCode::ACCEPTED, b.slice());
    } else {
      generateError(r);
    }
  } else if (suffixes.size() == 2) {
    std::string const& user = suffixes[0];
    if (suffixes[1] == "config") {
      if (auto r2 = exec.canModifyUserProfile(user); r2.ok()) {
        Result r = um->updateUser(
            user,
            [&](auth::User& u) {
              u.setConfigData(VPackBuilder());
              return TRI_ERROR_NO_ERROR;
            },
            auth::UserManager::RetryOnConflict::No);
        logAccountManagement(*_request, "remove user config", user, r);
        if (r.ok()) {
          resetResponse(ResponseCode::OK);
        } else {
          generateError(r);
        }
      } else {
        logAccountManagement(*_request, "remove user config", user, r2);
        generateError(r2);
      }
    } else {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    }
  } else if (suffixes.size() == 3 || suffixes.size() == 4) {
    std::string const& user = suffixes[0];

    if (suffixes[1] == "database") {
      // revoke a user's permissions
      std::string const& db = suffixes[2];
      std::string coll = suffixes.size() == 4 ? suffixes[3] : "";
      std::string const target = describePermissionTarget(db, coll);

      if (auto r = exec.canGrantUserPermissions(user); r.fail()) {
        logAccountManagement(*_request, "revoke permission", user, r, target);
        generateError(r);
        return RestStatus::DONE;
      }

      // validate that the collection is present
      if (suffixes.size() > 3) {
        auto res = existsCollection(server(), db, coll);

        if (!res.ok()) {
          logAccountManagement(*_request, "revoke permission", user, res,
                               target);
          generateError(res);

          return RestStatus::DONE;
        }
      }

      Result r = um->updateUser(
          user,
          [&](auth::User& entry) {
            if (coll.empty()) {
              entry.removeDatabase(db);
            } else {
              entry.removeCollection(db, coll);
            }

            return TRI_ERROR_NO_ERROR;
          },
          auth::UserManager::RetryOnConflict::Yes);

      logAccountManagement(*_request, "revoke permission", user, r, target);
      if (r.ok()) {
        VPackBuilder b;
        b(VPackValue(VPackValueType::Object))(
            StaticStrings::Error, VPackValue(false))(StaticStrings::Code,
                                                     VPackValue(202))();
        generateResult(ResponseCode::ACCEPTED, b.slice());
      } else {
        generateError(r);
      }
    } else if (suffixes[1] == "config") {
      // remove internal config data, used in the WebUI
      std::string const& key = suffixes[2];
      std::string const target = "config key='" + key + "'";
      if (auto r1 = exec.canModifyUserProfile(user); r1.ok()) {
        Result r = um->updateUser(
            user,
            [&](auth::User& u) {
              VPackBuilder b;
              b(VPackValue(VPackValueType::Object))(key,
                                                    VPackSlice::nullSlice())();
              if (!u.configData().isNone()) {
                u.setConfigData(VPackCollection::merge(u.configData(),
                                                       b.slice(), false, true));
              }
              return TRI_ERROR_NO_ERROR;
            },
            auth::UserManager::RetryOnConflict::No);

        logAccountManagement(*_request, "remove user config", user, r, target);
        if (r.ok()) {
          resetResponse(ResponseCode::OK);
        } else {
          generateError(r);
        }
      } else {
        logAccountManagement(*_request, "remove user config", user, r1, target);
        generateError(r1);
      }
    }

  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  }
  return RestStatus::DONE;
}
