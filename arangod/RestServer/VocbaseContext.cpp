////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "VocbaseContext.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "VocBase/vocbase.h"

using namespace arangodb::rest;

namespace arangodb {

VocbaseContext::VocbaseContext(GeneralRequest& req, TRI_vocbase_t& vocbase,
                               ExecContext::Type type, auth::Level systemLevel,
                               auth::Level dbLevel, bool isAdminUser)
    : ExecContext(type, req.user(), req.databaseName(), systemLevel, dbLevel, isAdminUser),
      _vocbase(vocbase) {
  // _vocbase has already been refcounted for us
  TRI_ASSERT(!_vocbase.isDangling());
}

VocbaseContext::~VocbaseContext() {
  TRI_ASSERT(!_vocbase.isDangling());
  _vocbase.release();
}

VocbaseContext* VocbaseContext::create(GeneralRequest& req, TRI_vocbase_t& vocbase) {
  // _vocbase has already been refcounted for us
  TRI_ASSERT(!vocbase.isDangling());

  // superusers will have an empty username. This MUST be invalid
  // for users authenticating with name / password
  const bool isSuperUser = req.authenticated() && req.user().empty() &&
                           req.authenticationMethod() == AuthenticationMethod::JWT;
  if (isSuperUser) {
    return new VocbaseContext(req, vocbase, ExecContext::Type::Internal,
                              /*sysLevel*/ auth::Level::RW,
                              /*dbLevel*/ auth::Level::RW, true);
  }

  AuthenticationFeature* auth = AuthenticationFeature::instance();
  TRI_ASSERT(auth != nullptr);
  if (!auth->isActive()) {
    if (ServerState::readOnly()) {
      // special read-only case
      return new VocbaseContext(req, vocbase, ExecContext::Type::Internal,
                                /*sysLevel*/ auth::Level::RO,
                                /*dbLevel*/ auth::Level::RO, true);
    }
    return new VocbaseContext(req, vocbase, req.user().empty() ?
                              ExecContext::Type::Internal : ExecContext::Type::Default,
                              /*sysLevel*/ auth::Level::RW,
                              /*dbLevel*/ auth::Level::RW, true);
  }

  if (!req.authenticated()) {
    return new VocbaseContext(req, vocbase, ExecContext::Type::Default,
                              /*sysLevel*/ auth::Level::NONE,
                              /*dbLevel*/ auth::Level::NONE, false);
  } 
  
  if (req.user().empty()) {
    std::string msg = "only jwt can be used to authenticate as superuser";
    LOG_TOPIC("2d0f6", WARN, Logger::AUTHENTICATION) << msg;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }

  auth::UserManager* um = auth->userManager();
  if (um == nullptr) {
    LOG_TOPIC("aae8a", WARN, Logger::AUTHENTICATION)
        << "users are not supported on this server";
    return nullptr;
  }

  auth::Level dbLvl = um->databaseAuthLevel(req.user(), req.databaseName(), false);
  auth::Level sysLvl = dbLvl;
  if (req.databaseName() != StaticStrings::SystemDatabase) {
    sysLvl = um->databaseAuthLevel(req.user(), StaticStrings::SystemDatabase, false);
  }
  bool isAdminUser = (sysLvl == auth::Level::RW);
  if (!isAdminUser && ServerState::readOnly()) {
    // in case we are in read-only mode, we need to re-check the original permissions
    isAdminUser = um->databaseAuthLevel(req.user(), StaticStrings::SystemDatabase, true) ==
                  auth::Level::RW;
  }

  return new VocbaseContext(req, vocbase, ExecContext::Type::Default,
                            /*sysLevel*/ sysLvl,
                            /*dbLevel*/ dbLvl, isAdminUser);
}

void VocbaseContext::forceSuperuser() {
  TRI_ASSERT(_type != ExecContext::Type::Internal || _user.empty());
  if (ServerState::readOnly()) {
    forceReadOnly();
  } else {
    _type = ExecContext::Type::Internal;
    _systemDbAuthLevel = auth::Level::RW;
    _databaseAuthLevel = auth::Level::RW;
    _isAdminUser = true;
  }
}

void VocbaseContext::forceReadOnly() {
  TRI_ASSERT(_type != ExecContext::Type::Internal || _user.empty());
  _type = ExecContext::Type::Internal;
  _systemDbAuthLevel = auth::Level::RO;
  _databaseAuthLevel = auth::Level::RO;
}

}  // namespace arangodb
