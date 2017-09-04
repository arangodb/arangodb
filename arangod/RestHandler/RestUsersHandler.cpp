////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestUsersHandler::RestUsersHandler(GeneralRequest* request,
                                   GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RestUsersHandler::execute() {
  RequestType const type = _request->requestType();
  auto auth = FeatureCacheFeature::instance()->authenticationFeature();
  TRI_ASSERT(auth != nullptr);
  AuthInfo* authInfo = auth->authInfo();

  switch (type) {
    case RequestType::GET:
      return getRequest(authInfo);
    case RequestType::POST:
      return postRequest(authInfo);
    case RequestType::PUT:
      return putRequest(authInfo);
    case RequestType::PATCH:
      return patchRequest(authInfo);
    case RequestType::DELETE_REQ:
      return deleteRequest(authInfo);

    default:
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return RestStatus::DONE;
  }
}

bool RestUsersHandler::isSystemUser() const {
  if (_request->execContext() != nullptr) {
    return _request->execContext()->systemAuthLevel() == AuthLevel::RW;
  }
  // if authentication is deactivated authorize anyway
  return !FeatureCacheFeature::instance()->authenticationFeature()->isActive();
}

bool RestUsersHandler::canAccessUser(std::string const& user) const {
  if (_request->authorized() && user == _request->user()) {
    return true;
  }
  return isSystemUser();
}

/// helper to generate a compliant response for individual user requests
void RestUsersHandler::generateUserResult(rest::ResponseCode code,
                                          VPackBuilder const& doc) {
  VPackBuilder b;
  b(VPackValue(VPackValueType::Object))("error", VPackValue(false))(
      "code", VPackValue((int)code))();
  b = VPackCollection::merge(doc.slice(), b.slice(), false);
  generateResult(code, b.slice());
}

RestStatus RestUsersHandler::getRequest(AuthInfo* authInfo) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    if (isSystemUser()) {
      VPackBuilder users = authInfo->allUsers();
      generateSuccess(ResponseCode::OK, users.slice());
    } else {
      generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    }
  } else if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (canAccessUser(user)) {
      VPackBuilder doc = authInfo->serializeUser(user);
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
        generateDatabaseResult(authInfo, user, full);
      } else if (suffixes.size() == 3) {
        //_api/user/<user>/database/<dbname>
        // return specific database
        AuthLevel lvl = authInfo->canUseDatabase(user, suffixes[2]);
        VPackBuilder data;
        data.add(VPackValue(convertFromAuthLevel(lvl)));
        generateSuccess(ResponseCode::OK, data.slice());

      } else if (suffixes.size() == 4) {
        //_api/user/<user>/database/<dbname>/<collection>
        AuthLevel lvl =
            authInfo->canUseCollection(user, suffixes[2], suffixes[3]);
        VPackBuilder data;
        data.add(VPackValue(convertFromAuthLevel(lvl)));
        generateSuccess(ResponseCode::OK, data.slice());
      } else {
        generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
      }

    } else if (suffixes[1] == "config") {
      //_api/user/<user>//config
      VPackBuilder data = authInfo->getConfigData(user);
      VPackSlice resp = data.slice();
      if (suffixes.size() == 3) {
        resp = data.slice().get(suffixes[2]);
      }
      generateSuccess(ResponseCode::OK,
                      resp.isNone() ? VPackSlice::nullSlice() : resp);
    } else {
      generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    }
  }

  return RestStatus::DONE;
}

/// generate response for /_api/user/database?full=true/false
void RestUsersHandler::generateDatabaseResult(AuthInfo* authInfo,
                                              std::string const& user,
                                              bool full) {
  // return list of databases
  VPackBuilder data;
  data.openObject();
  Result res = authInfo->accessUser(user, [&](AuthUserEntry const& entry) {
    DatabaseFeature::DATABASE->enumerateDatabases([&](TRI_vocbase_t* vocbase) {

      AuthLevel lvl = entry.databaseAuthLevel(vocbase->name());
      std::string str = "undefined";
      if (entry.hasSpecificDatabase(vocbase->name())) {
        str = convertFromAuthLevel(lvl);
      }

      if (full) {
        VPackObjectBuilder b(&data, vocbase->name(), true);
        data.add("permission", VPackValue(str));
        VPackObjectBuilder b2(&data, "collections", true);
        methods::Collections::enumerateCollections(
            vocbase, [&](LogicalCollection* c) {
              if (entry.hasSpecificCollection(vocbase->name(), c->name())) {
                lvl = entry.collectionAuthLevel(vocbase->name(), c->name());
                data.add(c->name(), VPackValue(convertFromAuthLevel(lvl)));
              } else {
                data.add(c->name(), VPackValue("undefined"));
              }
            });
        lvl = authInfo->canUseCollection(user, vocbase->name(), "*");
        data.add("*", VPackValue(convertFromAuthLevel(lvl)));
      } else if (lvl != AuthLevel::NONE) {  // hide db's without access
        data.add(vocbase->name(), VPackValue(str));
      }
    });
    if (full) {
      AuthLevel lvl = authInfo->canUseDatabase(user, "*");
      data("*", VPackValue(VPackValueType::Object))(
          "permission", VPackValue(convertFromAuthLevel(lvl)))();
    }
  });
  data.close();
  if (res.ok()) {
    generateSuccess(ResponseCode::OK, data.slice());
  } else {
    generateError(res);
  }
}

/// helper to create(0), replace(1), update(2) a user
static Result StoreUser(AuthInfo* authInfo, int mode, std::string const& user,
                        VPackSlice json) {
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
    r = authInfo->storeUser(mode == 1, user, passwd, active);
  } else if (mode == 2) {
    r = authInfo->updateUser(user, [&](AuthUserEntry& entry) {
      if (json.isObject()) {
        if (json.get("passwd").isString()) {
          entry.updatePassword(passwd);
        }
        if (json.get("active").isBool()) {
          entry.setActive(active);
        }
      }
    });
  }
  if (r.ok() && extra.isObject() && !extra.isEmptyObject()) {
    r = authInfo->setUserData(user, extra);
  }

  return r;
}

RestStatus RestUsersHandler::postRequest(AuthInfo* authInfo) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();
  bool parseSuccess = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!parseSuccess || !parsedBody->slice().isObject()) {
    generateResult(rest::ResponseCode::OK, VPackSlice());
    return RestStatus::DONE;
  }

  if (suffixes.empty()) {
    // create a new user
    if (isSystemUser()) {
      VPackSlice s = parsedBody->slice().get("user");
      std::string user = s.isString() ? s.copyString() : "";
      // create user
      Result r = StoreUser(authInfo, 0, user, parsedBody->slice());
      if (r.ok()) {
        VPackBuilder doc = authInfo->serializeUser(user);
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
    VPackSlice s = parsedBody->slice().get("passwd");
    if (s.isString()) {
      password = s.copyString();
    }
    AuthResult result = authInfo->checkPassword(user, password);
    if (result._authorized) {
      generateSuccess(rest::ResponseCode::OK, VPackSlice::trueSlice());
    } else {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_USER_NOT_FOUND);
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  }
  return RestStatus::DONE;
}

RestStatus RestUsersHandler::putRequest(AuthInfo* authInfo) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();
  bool parseSuccess = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!parseSuccess) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  if (suffixes.size() == 1) {
    // replace existing user
    std::string const& user = suffixes[0];
    if (canAccessUser(user)) {
      Result r = StoreUser(authInfo, 1, user, parsedBody->slice());
      if (r.ok()) {
        VPackBuilder doc = authInfo->serializeUser(user);
        generateUserResult(ResponseCode::OK, doc);
      } else {
        generateError(r);
      }
    } else {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    }
  } else if (suffixes.size() == 3 || suffixes.size() == 4) {
    // update database / collection permissions
    std::string const& user = suffixes[0];
    if (suffixes[1] == "database") {
      // update a user's permissions
      std::string const& db = suffixes[2];
      std::string coll = suffixes.size() == 4 ? suffixes[3] : "";
      if (!isSystemUser()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
        return RestStatus::DONE;
      }

      if (!parsedBody->slice().isObject() ||
          !parsedBody->slice().get("grant").isString()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
        return RestStatus::DONE;
      }

      VPackSlice grant = parsedBody->slice().get("grant");
      AuthLevel lvl = convertToAuthLevel(grant);

      // contains response in case of success
      VPackBuilder b;
      b(VPackValue(VPackValueType::Object));
      Result r = authInfo->updateUser(user, [&](AuthUserEntry& entry) {
        if (coll.empty()) {
          entry.grantDatabase(db, lvl);
          b(db, VPackValue(convertFromAuthLevel(lvl)))();
        } else {
          entry.grantCollection(db, coll, lvl);
          b(db + "/" + coll, VPackValue(convertFromAuthLevel(lvl)))();
        }
      });
      if (r.ok()) {
        generateUserResult(ResponseCode::OK, b);
      } else {
        generateError(r);
      }

    } else if (suffixes[1] == "config") {
      // update internal config data, used in the admin dashboard
      if (canAccessUser(user)) {
        std::string const& key = suffixes[2];
        VPackBuilder conf = authInfo->getConfigData(user);
        // The API expects: { value : <toStore> }
        // If we get sth else than the above it is translated
        // to a remove of the config option.
        if (!parsedBody->isEmpty()) {
          VPackSlice newVal = parsedBody->slice();
          VPackSlice oldConf = conf.slice();
          if (!newVal.isObject() || !newVal.hasKey("value")) {
            if (!oldConf.isObject() || !oldConf.hasKey(key)) {
              // Nothing to do. We do not have a config yet.
              // so we do not create a new empty one.
              resetResponse(ResponseCode::OK);
              return RestStatus::DONE;
            }
            conf = VPackCollection::remove(
                oldConf, std::unordered_set<std::string>{key});
          } else {
            // We need to merge the new key into the config
            newVal = newVal.get("value");
            VPackBuilder b;
            b.openObject();
            b.add(key, newVal);
            b.close();
            conf = oldConf.isObject()
                       ? VPackCollection::merge(oldConf, b.slice(), false)
                       : b;
          }
        }

        Result r = authInfo->setConfigData(user, conf.slice());

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

RestStatus RestUsersHandler::patchRequest(AuthInfo* authInfo) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  bool parseSuccess = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!parseSuccess) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (canAccessUser(user)) {
      // update a user
      Result r = StoreUser(authInfo, 2, user, parsedBody->slice());
      if (r.ok()) {
        VPackBuilder doc = authInfo->serializeUser(user);
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

RestStatus RestUsersHandler::deleteRequest(AuthInfo* authInfo) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();

  if (suffixes.size() == 1) {
    if (!isSystemUser()) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
      return RestStatus::DONE;
    }

    std::string const& user = suffixes[0];
    Result r = authInfo->removeUser(user);
    if (r.ok()) {
      VPackBuilder b;
      b(VPackValue(VPackValueType::Object))("error", VPackValue(false))(
          "code", VPackValue(202))();
      generateResult(ResponseCode::ACCEPTED, b.slice());
    } else {
      generateError(r);
    }
  } else if (suffixes.size() == 2) {
    std::string const& user = suffixes[0];
    if (suffixes[1] == "config" && canAccessUser(user)) {
      Result r = authInfo->setConfigData(user, VPackSlice::emptyObjectSlice());
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
      if (!isSystemUser()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
        return RestStatus::DONE;
      }

      Result r = authInfo->updateUser(user, [&](AuthUserEntry& entry) {
        if (coll.empty()) {
          entry.removeDatabase(db);
        } else {
          entry.removeCollection(db, coll);
        }
      });
      if (r.ok()) {
        VPackBuilder b;
        b(VPackValue(VPackValueType::Object))("error", VPackValue(false))(
                     "code", VPackValue(202))();
        generateResult(ResponseCode::ACCEPTED, b.slice());
      } else {
        generateError(r);
      }
    } else if (suffixes[1] == "config") {
      // remove internal config data, used in the admin dashboard
      if (canAccessUser(user)) {
        std::string const& key = suffixes[2];
        VPackBuilder config = authInfo->getConfigData(user);

        Result r;
        VPackBuilder b;
        b(VPackValue(VPackValueType::Object))(key, VPackSlice::nullSlice())();
        if (!config.isEmpty()) {
          config =
              VPackCollection::merge(config.slice(), b.slice(), false, true);
          r = authInfo->setConfigData(user, config.slice());
        }
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
