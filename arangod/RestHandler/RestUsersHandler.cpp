////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "RestUsersHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @return a collection exists in database or a wildcard was specified
////////////////////////////////////////////////////////////////////////////////
arangodb::Result existsCollection(arangodb::application_features::ApplicationServer& server,
                                  std::string const& database, std::string const& collection) {
  if (!server.hasFeature<arangodb::DatabaseFeature>()) {
    return arangodb::Result(TRI_ERROR_INTERNAL,
                            "failure to find feature 'Database'");
  }
  auto& databaseFeature = server.getFeature<arangodb::DatabaseFeature>();

  static const std::string wildcard("*");

  if (wildcard == database) {
    return arangodb::Result();  // wildcard always matches
  }

  auto* vocbase = databaseFeature.lookupDatabase(database);

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

RestUsersHandler::RestUsersHandler(application_features::ApplicationServer& server,
                                   GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestUsersHandler::execute() {
  RequestType const type = _request->requestType();
  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af == nullptr || af->userManager() == nullptr) {
    // nullptr may happens during shutdown, or on Agency
    generateError(ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED);
    return RestStatus::DONE;
  }

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

bool RestUsersHandler::isAdminUser() const {
  if (!ExecContext::isAuthEnabled()) {
    return true;
  }
  return ExecContext::current().isAdminUser();
}

bool RestUsersHandler::canAccessUser(std::string const& user) const {
  if (_request->authenticated() && user == _request->user()) {
    return true;
  }
  return isAdminUser();
}

/// helper to generate a compliant response for individual user requests
void RestUsersHandler::generateUserResult(rest::ResponseCode code, VPackBuilder const& doc) {
  VPackBuilder b;
  b(VPackValue(VPackValueType::Object))(StaticStrings::Error,
                                        VPackValue(false))(StaticStrings::Code,
                                                           VPackValue((int)code))();
  b = VPackCollection::merge(doc.slice(), b.slice(), false);
  generateResult(code, b.slice());
}

RestStatus RestUsersHandler::getRequest(auth::UserManager* um) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    if (isAdminUser()) {
      VPackBuilder users = um->allUsers();
      generateOk(ResponseCode::OK, users.slice());
    } else {
      generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    }
  } else if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (canAccessUser(user)) {
      VPackBuilder doc = um->serializeUser(user);
      generateUserResult(ResponseCode::OK, doc);
    } else {
      generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    }
  } else {
    std::string const& user = suffixes[0];
    if (!canAccessUser(user)) {
      generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
      return RestStatus::DONE;
    }

    if (suffixes[1] == "database") {
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
        generateOk(ResponseCode::OK, resp.isNone() ? VPackSlice::nullSlice() : resp);
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

/// generate response for /_api/user/database?full=true/false
void RestUsersHandler::generateDatabaseResult(auth::UserManager* um,
                                              std::string const& username, bool full) {
  // return list of databases
  VPackBuilder data;
  data.openObject();
  Result res = um->accessUser(username, [&, this](auth::User const& user) {
    server().getFeature<DatabaseFeature>().enumerateDatabases([&](TRI_vocbase_t& vocbase) -> void {
      if (full) {
        auto lvl = user.configuredDBAuthLevel(vocbase.name());
        std::string str = convertFromAuthLevel(lvl);
        velocypack::ObjectBuilder b(&data, vocbase.name(), true);

        data.add("permission", VPackValue(str));

        velocypack::ObjectBuilder b2(&data, "collections", true);

        methods::Collections::enumerate(&vocbase, [&](std::shared_ptr<LogicalCollection> const& c) -> void {
          TRI_ASSERT(c);
          lvl = user.configuredCollectionAuthLevel(vocbase.name(), c->name());
          data.add(c->name(), velocypack::Value(convertFromAuthLevel(lvl)));
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
      data("*", VPackValue(VPackValueType::Object))("permission",
                                                    VPackValue(convertFromAuthLevel(lvl)))();
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

/// helper to create(0), replace(1), update(2) a user
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
    r = um->updateUser(user, [&](auth::User& entry) {
      if (json.isObject()) {
        if (json.get("passwd").isString()) {
          entry.updatePassword(passwd);
        }
        if (json.get("active").isBool()) {
          entry.setActive(active);
        }
      }
      if (extra.isObject() && !extra.isEmptyObject()) {
        entry.setUserData(VPackBuilder(extra));
      }
      return TRI_ERROR_NO_ERROR;
    });
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
    if (isAdminUser()) {
      VPackSlice s = body.get("user");
      std::string user = s.isString() ? s.copyString() : "";
      // create user
      Result r = StoreUser(um, 0, user, body);
      if (r.ok()) {
        VPackBuilder doc = um->serializeUser(user);
        generateUserResult(ResponseCode::CREATED, doc);
      } else {
        generateError(r);
      }
    } else {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    }

  } else if (suffixes.size() == 1) {
    // validate username / password
    std::string const& user = suffixes[0];
    std::string password;
    VPackSlice s = body.get("passwd");
    if (s.isString()) {
      password = s.copyString();
    }
    if (um->checkPassword(user, password)) {
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

  if (suffixes.size() == 1) {
    // replace existing user
    std::string const& user = suffixes[0];
    if (!canAccessUser(user)) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
      return RestStatus::DONE;
    }

    Result r = StoreUser(um, 1, user, body);
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

      if (!isAdminUser()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);

        return RestStatus::DONE;
      }

      // validate that the collection is present
      if (suffixes.size() > 3) {
        auto res = existsCollection(server(), db, coll);

        if (!res.ok()) {
          generateError(res);

          return RestStatus::DONE;
        }
      }

      if (!body.isObject() || !body.get("grant").isString()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);

        return RestStatus::DONE;
      }

      VPackSlice grant = body.get("grant");
      auth::Level lvl = auth::convertToAuthLevel(grant);

      // contains response in case of success
      VPackBuilder b;

      b(VPackValue(VPackValueType::Object));

      Result r = um->updateUser(name, [&](auth::User& entry) {
        if (coll.empty()) {
          entry.grantDatabase(db, lvl);
          b(db, VPackValue(convertFromAuthLevel(lvl)))();
        } else {
          entry.grantCollection(db, coll, lvl);
          b(db + "/" + coll, VPackValue(convertFromAuthLevel(lvl)))();
        }

        return TRI_ERROR_NO_ERROR;
      });

      if (r.ok()) {
        generateUserResult(ResponseCode::OK, b);
      } else {
        generateError(r);
      }
    } else if (suffixes[1] == "config") {
      // update internal config data, used in the admin dashboard
      if (!canAccessUser(name)) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
        return RestStatus::DONE;
      }

      Result res;
      if (!body.isNone()) {
        std::string const& key = suffixes[2];
        // The API expects: { value : <toStore> }
        // If we get sth else than the above it is translated
        // to a remove of the config option.
        res = um->updateUser(name, [&](auth::User& u) {
          VPackSlice newVal = body;
          VPackSlice oldConf = u.userData();
          if (!newVal.isObject() || !newVal.hasKey("value")) {
            if (oldConf.isObject() && oldConf.hasKey(key)) {
              u.setUserData(
                  VPackCollection::remove(oldConf, std::unordered_set<std::string>{key}));
            }       // Nothing to do. We do not have a config yet.
          } else {  // We need to merge the new key into the config
            VPackBuilder b;
            b(VPackValue(VPackValueType::Object))(key, newVal.get("value"))();

            if (oldConf.isObject() && !oldConf.isEmptyObject()) {  // merge value in
              u.setUserData(VPackCollection::merge(oldConf, b.slice(), false));
            } else {
              u.setUserData(std::move(b));
            }
          }

          return TRI_ERROR_NO_ERROR;
        });
      }
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

  if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (canAccessUser(user)) {
      // update a user
      Result r = StoreUser(um, 2, user, body);
      if (r.ok()) {
        VPackBuilder doc = um->serializeUser(user);
        generateUserResult(ResponseCode::OK, doc);
      } else {
        generateError(r);
      }
    } else {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  }
  return RestStatus::DONE;
}

RestStatus RestUsersHandler::deleteRequest(auth::UserManager* um) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();

  if (suffixes.size() == 1) {
    if (!isAdminUser()) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
      return RestStatus::DONE;
    }

    std::string const& user = suffixes[0];
    Result r = um->removeUser(user);
    if (r.ok()) {
      VPackBuilder b;
      b(VPackValue(VPackValueType::Object))(StaticStrings::Error,
                                            VPackValue(false))(StaticStrings::Code,
                                                               VPackValue(202))();
      generateResult(ResponseCode::ACCEPTED, b.slice());
    } else {
      generateError(r);
    }
  } else if (suffixes.size() == 2) {
    std::string const& user = suffixes[0];
    if (suffixes[1] == "config" && canAccessUser(user)) {
      Result r = um->updateUser(user, [&](auth::User& u) {
        u.setConfigData(VPackBuilder());
        return TRI_ERROR_NO_ERROR;
      });
      if (r.ok()) {
        resetResponse(ResponseCode::OK);
      } else {
        generateError(r);
      }
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    }
  } else if (suffixes.size() == 3 || suffixes.size() == 4) {
    std::string const& user = suffixes[0];

    if (suffixes[1] == "database") {
      // revoke a user's permissions
      std::string const& db = suffixes[2];
      std::string coll = suffixes.size() == 4 ? suffixes[3] : "";

      if (!isAdminUser()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);

        return RestStatus::DONE;
      }

      // validate that the collection is present
      if (suffixes.size() > 3) {
        auto res = existsCollection(server(), db, coll);

        if (!res.ok()) {
          generateError(res);

          return RestStatus::DONE;
        }
      }

      Result r = um->updateUser(user, [&](auth::User& entry) {
        if (coll.empty()) {
          entry.removeDatabase(db);
        } else {
          entry.removeCollection(db, coll);
        }

        return TRI_ERROR_NO_ERROR;
      });

      if (r.ok()) {
        VPackBuilder b;
        b(VPackValue(VPackValueType::Object))(StaticStrings::Error,
                                              VPackValue(false))(StaticStrings::Code,
                                                                 VPackValue(202))();
        generateResult(ResponseCode::ACCEPTED, b.slice());
      } else {
        generateError(r);
      }
    } else if (suffixes[1] == "config") {
      // remove internal config data, used in the WebUI
      if (canAccessUser(user)) {
        std::string const& key = suffixes[2];
        Result r = um->updateUser(user, [&](auth::User& u) {
          VPackBuilder b;
          b(VPackValue(VPackValueType::Object))(key, VPackSlice::nullSlice())();
          if (!u.configData().isNone()) {
            u.setConfigData(VPackCollection::merge(u.configData(), b.slice(), false, true));
          }
          return TRI_ERROR_NO_ERROR;
        });

        if (r.ok()) {
          resetResponse(ResponseCode::OK);
        } else {
          generateError(r);
        }
      } else {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
      }
    }

  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  }
  return RestStatus::DONE;
}
