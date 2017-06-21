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
#include "RestServer/FeatureCacheFeature.h"

#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

void StoreUser(v8::FunctionCallbackInfo<v8::Value> const& args, bool replace) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "save(username, password[, active, userData, changePassword])");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_USER_INVALID_NAME);
  }
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->systemAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }
  std::string username = TRI_ObjectToString(args[0]);
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
  bool changePassword = false;
  if (args.Length() >= 5) {
    changePassword = TRI_ObjectToBoolean(args[4]);
  }

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  Result r = authentication->authInfo()->storeUser(replace, username, pass,
                                                   active, changePassword);
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }
  if (!extras.isEmpty()) {
    authentication->authInfo()->setUserData(username, extras.slice());
  }

  VPackBuilder result = authentication->authInfo()->getUser(username);
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
        "update(username[, password, active, userData, changePassword])");
  }
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->systemAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  std::string username = TRI_ObjectToString(args[0]);
  VPackBuilder extras;
  if (args.Length() >= 4) {
    int r = TRI_V8ToVPackSimple(isolate, extras, args[3]);
    if (r != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(r);
    }
  }
  
  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  authentication->authInfo()->updateUser(username, [&](AuthUserEntry& entry) {
    if (args.Length() > 1 && args[1]->IsString()) {
      entry.updatePassword(TRI_ObjectToString(args[1]));
    }
    if (args.Length() > 2 && args[2]->IsBoolean()) {
      entry.setActive(TRI_ObjectToBoolean(args[2]));
    }
    if (args.Length() > 4 && args[4]->IsBoolean()) {
      entry.changePassword(TRI_ObjectToBoolean(args[4]));
    }
  });
  if (!extras.isEmpty()) {
    authentication->authInfo()->setUserData(username, extras.slice());
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_RemoveUser(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(username)");
  }
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->systemAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  std::string username = TRI_ObjectToString(args[0]);
  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  Result r = authentication->authInfo()->removeUser(username);
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->systemAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  std::string username = TRI_ObjectToString(args[0]);
  VPackBuilder result = authentication->authInfo()->getUser(username);
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
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->systemAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  authentication->authInfo()->outdate();

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GrantDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("grantDatabase(username, database, type)");
  }
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->systemAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  std::string username = TRI_ObjectToString(args[0]);
  std::string db = TRI_ObjectToString(args[1]);
  AuthLevel lvl = AuthLevel::RW;
  if (args.Length() >= 3) {
    std::string type = TRI_ObjectToString(args[2]);
    lvl = convertToAuthLevel(type);
  }

  Result r = authentication->authInfo()->updateUser(
      username, [&](AuthUserEntry& entry) { entry.grantDatabase(db, lvl); });
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->systemAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  std::string username = TRI_ObjectToString(args[0]);
  std::string db = TRI_ObjectToString(args[1]);
  Result r = authentication->authInfo()->updateUser(
      username,
      [&](AuthUserEntry& entry) { entry.grantDatabase(db, AuthLevel::NONE); });
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->databaseAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  std::string username = TRI_ObjectToString(args[0]);
  std::string db = TRI_ObjectToString(args[1]);
  std::string coll = TRI_ObjectToString(args[2]);

  AuthLevel lvl = AuthLevel::RW;
  if (args.Length() >= 4) {
    std::string type = TRI_ObjectToString(args[3]);
    lvl = convertToAuthLevel(type);
  }
  Result r = authentication->authInfo()->updateUser(
      username,
      [&](AuthUserEntry& entry) { entry.grantCollection(db, coll, lvl); });
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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
  if (ExecContext::CURRENT_EXECCONTEXT != nullptr) {
    AuthLevel level =
        ExecContext::CURRENT_EXECCONTEXT->authContext()->databaseAuthLevel();
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  std::string username = TRI_ObjectToString(args[0]);
  std::string db = TRI_ObjectToString(args[1]);
  std::string coll = TRI_ObjectToString(args[2]);

  Result r = authentication->authInfo()->updateUser(
      username, [&](AuthUserEntry& entry) {
        entry.grantCollection(db, coll, AuthLevel::NONE);
      });
  if (!r.ok()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  std::string username = TRI_ObjectToString(args[0]);
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
  VPackBuilder old = authentication->authInfo()->getConfigData(username);
  VPackBuilder updated =
      VelocyPackHelper::merge(old.slice(), merge.slice(), true, true);
  authentication->authInfo()->setConfigData(username, updated.slice());

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

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  std::string username = TRI_ObjectToString(args[0]);
  VPackBuilder config = authentication->authInfo()->getConfigData(username);
  if (!config.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, config.slice()));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetPermission(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (args.Length() < 1 || !args[0]->IsString() ||
      (args.Length() > 1 && !args[1]->IsString())) {
    TRI_V8_THROW_EXCEPTION_USAGE("permission(username[, key])");
  }

  auto authentication =
      FeatureCacheFeature::instance()->authenticationFeature();
  std::string username = TRI_ObjectToString(args[0]);

  if (args.Length() > 1) {
    std::string key = TRI_ObjectToString(args[1]);
    AuthLevel lvl = authentication->authInfo()->canUseDatabase(username, key);
    if (lvl == AuthLevel::RO) {
      TRI_V8_RETURN(TRI_V8_STRING("ro"));
    } else if (lvl == AuthLevel::RW) {
      TRI_V8_RETURN(TRI_V8_STRING("rw"));
    }
    TRI_V8_RETURN(TRI_V8_STRING("none"));
  } else {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    DatabaseFeature::DATABASE->enumerateDatabases([&](TRI_vocbase_t* vocbase) {
      AuthLevel lvl =
          authentication->authInfo()->canUseDatabase(username, vocbase->name());
      if (lvl != AuthLevel::NONE) {
        std::string str = AuthLevel::RO == lvl ? "ro" : "rw";
        result->ForceSet(TRI_V8_STD_STRING(vocbase->name()),
                         TRI_V8_STD_STRING(str));
      }
    });
    TRI_V8_RETURN(result);
  }
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Users(v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase,
                     TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoUsers"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("save"), JS_SaveUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("replace"),
                       JS_ReplaceUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("update"),
                       JS_UpdateUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("remove"),
                       JS_RemoveUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("document"),
                       JS_GetUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("reload"),
                       JS_ReloadAuthData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("grantDatabase"),
                       JS_GrantDatabase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("revokeDatabase"),
                       JS_RevokeDatabase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("grantCollection"),
                       JS_GrantCollection);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("revokeCollection"),
                       JS_RevokeCollection);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("updateConfigData"),
                       JS_UpdateConfigData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("configData"),
                       JS_GetConfigData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("permission"),
                       JS_GetPermission);

  v8g->UsersTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoUsersCtor"));
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("ArangoUsersCtor"),
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance();
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, TRI_V8_ASCII_STRING("ArangoUsers"),
                                 aa);
  }
}
