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

  /*return RestStatus::QUEUE
      .then([]() { LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "demo handler
     going to sleep"; })
      .then([]() { sleep(5); })
      .then([]() { LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "demo handler
     done sleeping"; })
      .then([this]() { doSomeMoreWork(); })
      .then([this]() { return evenMoreWork(); });*/
}

bool RestUsersHandler::isSystemUser() const {
  if (_request->execContext() != nullptr) {
    return _request->execContext()->authContext()->systemAuthLevel() ==
           AuthLevel::RW;
  }
  return false;
}

bool RestUsersHandler::canAccessUser(std::string const& user) const {
  return user == _request->user() ? true : isSystemUser();
}

RestStatus RestUsersHandler::getRequest(AuthInfo* authInfo) {
  std::vector<std::string> suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    if (isSystemUser()) {
      VPackBuilder users = authInfo->allUsers();
      
      VPackBuilder r;
      r(VPackValue(VPackValueType::Object))("result", users.slice());
      generateResult(ResponseCode::OK, r.slice());
    } else {
      generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    }
  } else if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (canAccessUser(user)) {
      VPackBuilder doc = authInfo->getUser(user);
      
      VPackBuilder r;
      r(VPackValue(VPackValueType::Object))("result", doc.slice());
      generateResult(ResponseCode::OK, r.slice());
    } else {
      generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    }
  } else if (suffixes.size() >= 2) {
    std::string const& user = suffixes[0];

    if (canAccessUser(user)) {
      VPackBuilder data;
      data.openObject();
      if (suffixes[1] == "database") {
        VPackObjectBuilder b(&data, "result", true);
        DatabaseFeature::DATABASE->enumerateDatabases(
            [&](TRI_vocbase_t* vocbase) {
              AuthLevel lvl = authInfo->canUseDatabase(user, vocbase->name());
              if (lvl != AuthLevel::NONE) {
                std::string str = AuthLevel::RO == lvl ? "ro" : "rw";
                data.add(vocbase->name(), VPackValue(str));
              }
            });
        
      } else if (suffixes[1] == "config") {
        if (suffixes.size() == 3) {
          data.add("result", authInfo->getConfigData(user).slice().get(suffixes[2]));
        } else {
          data.add("result", authInfo->getConfigData(user).slice());
        }
      }
      data.close();// openObject
      generateResult(ResponseCode::OK, data.slice());
    } else {
      generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    }
  }
  return RestStatus::DONE;
}

/// helper to create(0), replace(1), update(2) a user
static Result StoreUser(AuthInfo* authInfo, int mode, std::string const& user,
                        VPackSlice json) {
  VPackSlice s = json.get("passwd");
  std::string passwd = s.isString() ? s.copyString() : "";
  s = json.get("active");
  bool active = s.isBool() ? s.getBool() : true;
  VPackSlice extra = json.get("extra");
  s = json.get("changePassword");
  bool changePasswd = s.isBool() ? s.getBool() : false;

  Result r;
  if (mode == 0 || mode == 1) {
    r = authInfo->storeUser(mode == 1, user, passwd, active, changePasswd);
  } else if (mode == 2) {
    r = authInfo->updateUser(user, [&](AuthUserEntry& entry) {
      entry.updatePassword(passwd);
      entry.setActive(active);
      entry.changePassword(changePasswd);
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
  if (!parseSuccess) {
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
        VPackBuilder doc = authInfo->getUser(user);
        generateResult(ResponseCode::OK, doc.slice());
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
      VPackBuilder b;
      b(VPackValue(VPackValueType::Object))("result", VPackValue(true));
      generateResult(rest::ResponseCode::OK, b.slice());
    } else {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
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
    generateResult(rest::ResponseCode::OK, VPackSlice());
    return RestStatus::DONE;
  }

  if (suffixes.size() == 1) {
    // replace existing user
    std::string const& user = suffixes[0];
    if (canAccessUser(user)) {
      Result r = StoreUser(authInfo, 1, user, parsedBody->slice());
      if (r.ok()) {
        VPackBuilder doc = authInfo->getUser(user);
        generateResult(ResponseCode::OK, doc.slice());
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
      if (isSystemUser()) {
        std::string const& db = suffixes[2];
        std::string coll = suffixes.size() == 4 ? suffixes[3] : "";
        VPackSlice grant = parsedBody->slice().get("grant");
        AuthLevel lvl = convertToAuthLevel(grant);

        Result r = authInfo->updateUser(user, [&](AuthUserEntry& entry) {
          if (coll.empty()) {
            entry.grantDatabase(db, lvl);
          } else {
            entry.grantCollection(db, coll, lvl);
          }
        });
        if (r.ok()) {
          generateResult(ResponseCode::OK, VPackSlice());
        } else {
          generateError(r);
        }
      } else {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
      }

    } else if (suffixes[1] == "config") {
      // update internal config data, used in the admin dashboard
      if (canAccessUser(user)) {
        std::string const& key = suffixes[2];
        VPackBuilder config = authInfo->getConfigData(user);
        VPackBuilder b;
        b(VPackValue(VPackValueType::Object))(key, parsedBody->slice());

        config = VPackCollection::merge(config.slice(), b.slice(), false);
        Result r = authInfo->setConfigData(user, config.slice());
        if (r.ok()) {
          generateResult(ResponseCode::OK, VPackSlice());
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
  std::vector<std::string> const& suffixes = _request->suffixes();
  bool parseSuccess = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!parseSuccess) {
    generateResult(rest::ResponseCode::OK, VPackSlice());
    return RestStatus::DONE;
  }

  if (suffixes.size() == 1) {
    std::string const& user = suffixes[0];
    if (canAccessUser(user)) {
      // update a user
      Result r = StoreUser(authInfo, 2, user, parsedBody->slice());
      if (r.ok()) {
        VPackBuilder doc = authInfo->getUser(user);
        generateResult(ResponseCode::OK, doc.slice());
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
  bool parseSuccess = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!parseSuccess) {
    generateResult(rest::ResponseCode::OK, VPackSlice());
    return RestStatus::DONE;
  }

  if (suffixes.size() == 1) {
    if (isSystemUser()) {
      std::string const& user = suffixes[0];
      Result r = authInfo->removeUser(user);
      if (r.ok()) {
        generateResult(ResponseCode::ACCEPTED, VPackSlice());
      } else {
        generateError(r);
      }
    }
  } else if (suffixes.size() == 2) {
    if (suffixes[1] == "config") {
      std::string const& user = suffixes[0];
      Result r = authInfo->setConfigData(user, VPackSlice::nullSlice());
      if (r.ok()) {
        generateResult(ResponseCode::OK, VPackSlice());
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
      if (isSystemUser()) {
        std::string const& db = suffixes[2];
        std::string coll = suffixes.size() == 4 ? suffixes[3] : "";
        Result r = authInfo->updateUser(user, [&](AuthUserEntry& entry) {
          if (coll.empty()) {
            entry.grantDatabase(db, AuthLevel::NONE);
          } else {
            entry.grantCollection(db, coll, AuthLevel::NONE);
          }
        });
        if (r.ok()) {
          generateResult(ResponseCode::OK, VPackSlice());
        } else {
          generateError(r);
        }
      } else {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
      }

    } else if (suffixes[1] == "config") {
      // remove internal config data, used in the admin dashboard
      if (canAccessUser(user)) {
        std::string const& key = suffixes[2];
        VPackBuilder config = authInfo->getConfigData(user);
        VPackBuilder b;
        b(VPackValue(VPackValueType::Object))(key, VPackSlice::nullSlice());

        config = VPackCollection::merge(config.slice(), b.slice(), false, true);
        Result r = authInfo->setConfigData(user, config.slice());
        if (r.ok()) {
          generateResult(ResponseCode::OK, VPackSlice());
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

/*
RestStatus RestUsersHandler::evenMoreWork() {
  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "demo handler almost done";

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("server", VPackValue("arango"));
  result.add("version", VPackValue(ARANGODB_VERSION));
  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());

  return RestStatus::DONE
      .then([]() { LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "demo handler
keeps working"; })
      .then([]() { sleep(5); })
      .then(
          []() { LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "even if the result
has already been returned"; })
      .then([]() { LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "finally done";
});
}
*/
