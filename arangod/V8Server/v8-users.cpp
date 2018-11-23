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

#include "v8-users.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/ExecContext.h"

#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @return a collection exists in database or a wildcard was specified
////////////////////////////////////////////////////////////////////////////////
arangodb::Result existsCollection(
    std::string const& database, std::string const& collection
) {
  auto* databaseFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabaseFeature
  >("Database");

  if (!databaseFeature) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL, "failure to find feature 'Database'"
    );
  }

  static const std::string wildcard("*");

  if (wildcard == database) {
    return arangodb::Result(); // wildcard always matches
  }

  auto* vocbase = databaseFeature->lookupDatabase(database);

  if (!vocbase) {
    return arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (wildcard == collection) {
    return arangodb::Result(); // wildcard always matches
  }

  return !arangodb::CollectionNameResolver(*vocbase).getCollection(collection)
    ? arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)
    : arangodb::Result()
    ;
}

}

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

static bool IsAdminUser() {
  if (ExecContext::CURRENT != nullptr) {
    return ExecContext::CURRENT->isAdminUser();
  }
  return true;
}

/// check ExecContext if system use
static bool CanAccessUser(std::string const& user) {
  if (ExecContext::CURRENT != nullptr) {
    return IsAdminUser()|| user == ExecContext::CURRENT->user();
  }
  return true;
}

void StoreUser(v8::FunctionCallbackInfo<v8::Value> const& args, bool replace) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "save(username, password[, active, userData])");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_USER_INVALID_NAME);
  }
  std::string username = TRI_ObjectToString(args[0]);
  if (!CanAccessUser(username)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }
  std::string pass = args.Length() > 1 && args[1]->IsString()
                         ? TRI_ObjectToString(args[1])
                         : "";
  bool active = true;
  if (args.Length() >= 3 && args[2]->IsBoolean()) {
    active = TRI_ObjectToBoolean(args[2]);
  }

  VPackBuilder extras;
  if (args.Length() >= 4) {
    int r = TRI_V8ToVPackSimple(isolate, extras, args[3]);
    if (r != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(r);
    }
  }
  
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "users are not supported on this server");
  }
  
  Result r = um->storeUser(replace, username, pass, active, extras.slice());
  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION(r);
  }

  VPackBuilder result = um->serializeUser(username);
  if (!result.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_SaveUser(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StoreUser(args, false);
}

static void JS_ReplaceUser(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StoreUser(args, true);
}

static void JS_UpdateUser(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "update(username[, password, active, userData])");
  }
  std::string username = TRI_ObjectToString(args[0]);
  if (!CanAccessUser(username)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  VPackBuilder extras;
  if (args.Length() >= 4) {
    int r = TRI_V8ToVPackSimple(isolate, extras, args[3]);
    if (r != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(r);
    }
  }
  
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "users are not supported on this server");
  }
  um->updateUser(username, [&](auth::User& u) {
    if (args.Length() > 1 && args[1]->IsString()) {
      u.updatePassword(TRI_ObjectToString(args[1]));
    }
    if (args.Length() > 2 && args[2]->IsBoolean()) {
      u.setActive(TRI_ObjectToBoolean(args[2]));
    }
    if (!extras.isEmpty()) {
      u.setUserData(std::move(extras));
    }
    return TRI_ERROR_NO_ERROR;
  });

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_RemoveUser(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(username)");
  }
  if (!IsAdminUser()) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "users are not supported on this server");
  }
  
  Result r = um->removeUser(TRI_ObjectToString(args[0]));
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION(r);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetUser(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("document(username)");
  }

  std::string username = TRI_ObjectToString(args[0]);

  if (!CanAccessUser(username)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "user are not supported on this server");
  }
  
  VPackBuilder result = um->serializeUser(username);
  if (!result.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_ReloadAuthData(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() > 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("reload()");
  }
  if (!IsAdminUser()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }
  
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um != nullptr) {
    um->triggerLocalReload();
    um->triggerGlobalReload(); // noop except on coordinator
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GrantDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("grantDatabase(username, database, type)");
  }
  if (!IsAdminUser()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  std::string username = TRI_ObjectToString(args[0]);
  std::string db = TRI_ObjectToString(args[1]);
  auth::Level lvl = auth::Level::RW;
  if (args.Length() >= 3) {
    std::string type = TRI_ObjectToString(args[2]);
    lvl = auth::convertToAuthLevel(type);
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "user are not supported on this server");
  }
  Result r = um->updateUser(username,
          [&](auth::User& entry) {
            entry.grantDatabase(db, lvl);
            return TRI_ERROR_NO_ERROR;
          });
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION(r);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_RevokeDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("revokeDatabase(username,  database)");
  }
  if (!IsAdminUser()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "user are not supported on this server");
  }
  
  std::string username = TRI_ObjectToString(args[0]);
  std::string db = TRI_ObjectToString(args[1]);
  Result r = um->updateUser(username, [&](auth::User& entry) {
        entry.removeDatabase(db);
        return TRI_ERROR_NO_ERROR;
      });
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION(r);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GrantCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString() ||
      !args[2]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("grantCollection(username, db, coll[, type])");
  }

  if (!IsAdminUser()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "user are not supported on this server");
  }

  std::string username = TRI_ObjectToString(args[0]);
  std::string db = TRI_ObjectToString(args[1]);
  std::string coll = TRI_ObjectToString(args[2]);

  // validate that the collection is present
  {
    auto res = existsCollection(db, coll);

    if (!res.ok()) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  auth::Level lvl = auth::Level::RW;

  if (args.Length() >= 4) {
    std::string type = TRI_ObjectToString(args[3]);
    lvl = auth::convertToAuthLevel(type);
  }

  Result r = um->updateUser(username, [&](auth::User& entry) {
    entry.grantCollection(db, coll, lvl);
    return TRI_ERROR_NO_ERROR;
  });

  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION(r);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_RevokeCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString() ||
      !args[2]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("revokeCollection(username, db, coll)");
  }

  if (!IsAdminUser()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "user are not supported on this server");
  }

  std::string username = TRI_ObjectToString(args[0]);
  std::string db = TRI_ObjectToString(args[1]);
  std::string coll = TRI_ObjectToString(args[2]);

  // validate that the collection is present
  {
    auto res = existsCollection(db, coll);

    if (!res.ok()) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  Result r = um->updateUser(
      username, [&](auth::User& entry) {
        entry.removeCollection(db, coll);
        return TRI_ERROR_NO_ERROR;
      });

  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION(r);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

// create/update (value != null) or delete (value == null)
static void JS_UpdateConfigData(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("updateConfigData(username, key[, value])");
  }
  std::string username = TRI_ObjectToString(args[0]);
  if (!CanAccessUser(username)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  std::string key = TRI_ObjectToString(args[1]);
  VPackBuilder merge;
  if (args.Length() > 2) {
    VPackBuilder value;
    int res = TRI_V8ToVPackSimple(isolate, value, args[2]);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
    merge.add(key, value.slice());
  } else {
    merge.add(key, VPackSlice::nullSlice());
  }
  
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "user are not supported on this server");
  }
  
  Result r = um->updateUser(username, [&](auth::User& u) {
    VPackBuilder updated = VelocyPackHelper::merge(u.configData(),
                                                   merge.slice(), true, true);
    u.setConfigData(std::move(updated));
    return TRI_ERROR_NO_ERROR;
  });
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION(r);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetConfigData(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 1 || !args[0]->IsString() ||
      (args.Length() > 1 && !args[1]->IsString())) {
    TRI_V8_THROW_EXCEPTION_USAGE("configData(username[, key])");
  }
  std::string username = TRI_ObjectToString(args[0]);
  if (!CanAccessUser(username)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "user are not supported on this server");
  }
  
  v8::Handle<v8::Value> result;
  Result r = um->accessUser(username, [&](auth::User const& u) {
    if (u.configData().isObject()) {
      result = TRI_VPackToV8(isolate, u.configData());
    }
    return TRI_ERROR_NO_ERROR;
  });
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION(r);
  } else if (!result.IsEmpty()) {
    TRI_V8_RETURN(result);
  }
  
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetPermission(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() > 3 || args.Length() == 0 || !args[0]->IsString() ||
      (args.Length() > 1 && !args[1]->IsString()) ||
      (args.Length() > 2 && !args[2]->IsString())) {
    TRI_V8_THROW_EXCEPTION_USAGE("permission(username[, database, collection])");
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "user are not supported on this server");
  }
  
  std::string username = TRI_ObjectToString(isolate, args[0]);
  if (args.Length() > 1) {
    std::string dbname = TRI_ObjectToString(isolate, args[1]);
    auth::Level lvl;
    if (args.Length() == 3) {
      std::string collection = TRI_ObjectToString(isolate, args[2]);
      lvl = um->collectionAuthLevel(username, dbname, collection);
    } else {
      lvl = um->databaseAuthLevel(username, dbname);
    }
    
    if (lvl == auth::Level::RO) {
      TRI_V8_RETURN(TRI_V8_ASCII_STRING(isolate, "ro"));
    } else if (lvl == auth::Level::RW) {
      TRI_V8_RETURN(TRI_V8_ASCII_STRING(isolate, "rw"));
    }
    TRI_V8_RETURN(TRI_V8_ASCII_STRING(isolate, "none"));
  } else {
    // return the current database permissions
    v8::Handle<v8::Object> result = v8::Object::New(isolate);

    DatabaseFeature::DATABASE->enumerateDatabases(
      [&](TRI_vocbase_t& vocbase)->void {
        auto lvl = um->databaseAuthLevel(username, vocbase.name());

        if (lvl != auth::Level::NONE) { // hide non accessible collections
          result->ForceSet(
            TRI_V8_STD_STRING(isolate, vocbase.name()),
            TRI_V8_STD_STRING(isolate, auth::convertFromAuthLevel(lvl))
          );
        }
      }
    );
    TRI_V8_RETURN(result);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_AuthIsActive(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive()) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_TRY_CATCH_END
}

static void JS_CurrentUser(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() != 0 ) {
    TRI_V8_THROW_EXCEPTION_USAGE("currentUser()");
  }
  if (ExecContext::CURRENT != nullptr) {
    TRI_V8_RETURN(TRI_V8_STD_STRING(isolate, ExecContext::CURRENT->user()));
  }
  TRI_V8_RETURN_NULL();
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Users(v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase,
                     TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoUsersCtor"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(0);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "save"), JS_SaveUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "replace"),
                       JS_ReplaceUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "update"),
                       JS_UpdateUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "remove"),
                       JS_RemoveUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "document"),
                       JS_GetUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "reload"),
                       JS_ReloadAuthData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "grantDatabase"),
                       JS_GrantDatabase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "revokeDatabase"),
                       JS_RevokeDatabase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "grantCollection"),
                       JS_GrantCollection);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "revokeCollection"),
                       JS_RevokeCollection);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "updateConfigData"),
                       JS_UpdateConfigData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "configData"),
                       JS_GetConfigData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "permission"),
                       JS_GetPermission);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "currentUser"),
                       JS_CurrentUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "isAuthActive"),
                       JS_AuthIsActive);

  v8g->UsersTempl.Reset(isolate, rt);
  //ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoUsersCtor"));
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "ArangoUsersCtor"),
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance();
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "ArangoUsers"), aa);
  }
}
