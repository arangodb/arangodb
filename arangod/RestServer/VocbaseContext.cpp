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
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

double VocbaseContext::ServerSessionTtl =
    60.0 * 60.0 * 24 * 60;  // 2 month session timeout

VocbaseContext* VocbaseContext::create(GeneralRequest* req, TRI_vocbase_t* vocbase) {
  // _vocbase has already been refcounted for us
  TRI_ASSERT(!vocbase->isDangling());

  AuthenticationFeature* auth = AuthenticationFeature::instance();
  TRI_ASSERT(auth != nullptr);
  if (auth == nullptr) {
    return nullptr;
  } else if (!auth->isActive()) {
    return new VocbaseContext(req, vocbase, /*isInternal*/ false,
                              /*sysLevel*/ auth::Level::RW,
                              /*dbLevel*/ auth::Level::RW);
  }

  if (!req->authenticated()) {
    return new VocbaseContext(req, vocbase, /*isInternal*/ false,
                              /*sysLevel*/ auth::Level::NONE,
                              /*dbLevel*/ auth::Level::NONE);
  }
  
  // superusers will have an empty username. This MUST be invalid
  // for users authenticating with name / password
  if (req->user().empty()) {
    if (req->authenticationMethod() != AuthenticationMethod::JWT) {
      std::string msg = "only jwt can be used to authenticate as superuser";
      LOG_TOPIC(WARN, Logger::AUTHENTICATION) << msg;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
    }
    return new VocbaseContext(req, vocbase, /*isInternal*/ true,
                              /*sysLevel*/ auth::Level::RW,
                              /*dbLevel*/ auth::Level::RW);
  }
  
  auth::UserManager* um = auth->userManager();
  if (um == nullptr) {
    LOG_TOPIC(WARN, Logger::AUTHENTICATION) << "users are not supported on this server";
    return nullptr;
  }
  
  auth::Level dbLvl = um->databaseAuthLevel(req->user(), req->databaseName());
  auth::Level sysLvl = dbLvl;
  if (req->databaseName() != TRI_VOC_SYSTEM_DATABASE) {
    sysLvl = um->databaseAuthLevel(req->user(), TRI_VOC_SYSTEM_DATABASE);
  }
  return new VocbaseContext(req, vocbase, /*isInternal*/ false,
                            /*sysLevel*/ sysLvl,
                            /*dbLevel*/ dbLvl);
}

VocbaseContext::VocbaseContext(GeneralRequest* req, TRI_vocbase_t* vocbase,
                               bool isInternal, auth::Level sys,
                               auth::Level dbl)
    : ExecContext(isInternal, req->user(), req->databaseName(), sys, dbl),
      _vocbase(vocbase) {
  TRI_ASSERT(_vocbase != nullptr);
  // _vocbase has already been refcounted for us
  TRI_ASSERT(!_vocbase->isDangling());
}

VocbaseContext::~VocbaseContext() {
  TRI_ASSERT(!_vocbase->isDangling());
  _vocbase->release();
}

void VocbaseContext::forceSuperuser() {
  TRI_ASSERT(!_internal || _user.empty());
  _internal = true;
  _systemDbAuthLevel = auth::Level::RW;
  _databaseAuthLevel = auth::Level::RW;
}

void VocbaseContext::forceReadOnly() {
  TRI_ASSERT(!_internal || _user.empty());
  _internal = true;
  _systemDbAuthLevel = auth::Level::RO;
  _databaseAuthLevel = auth::Level::RO;
}
