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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include <v8.h>

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "v8-vocbaseprivate.h"

#include "v8-user-functions.h"

#include "VocBase/Methods/AqlUserFunctions.h"

using namespace arangodb;

static void JS_UnregisterAQLUserFunction(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("UNREGISTER_AQL_USER_FUNCTION(<name>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string functionName = TRI_ObjectToString(isolate, args[0]);
  Result rv = unregisterUserFunction(vocbase, functionName);

  if (rv.fail()) {
    TRI_V8_THROW_EXCEPTION(rv);
  }

  TRI_V8_RETURN(v8::Number::New(isolate, 1));
  TRI_V8_TRY_CATCH_END;
}

static void JS_UnregisterAQLUserFunctionsGroup(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "UNREGISTER_AQL_USER_FUNCTION_GROUP(<group string>)");
  }

  int deleteCount;
  auto& vocbase = GetContextVocBase(isolate);
  std::string functionFilterPrefix = TRI_ObjectToString(isolate, args[0]);
  Result rv = unregisterUserFunctionsGroup(vocbase, functionFilterPrefix, deleteCount);

  if (rv.fail()) {
    TRI_V8_THROW_EXCEPTION(rv);
  }

  TRI_V8_RETURN(v8::Number::New(isolate, deleteCount));
  TRI_V8_TRY_CATCH_END;
}

static void JS_RegisterAqlUserFunction(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "REGISTER_AQL_USER_FUNCTION(<name>, <functionbody> [, "
        "<isDeterministic>])");
  }

  TRI_normalize_V8_Obj(args, args[0]);

  auto& vocbase = GetContextVocBase(isolate);
  VPackBuilder builder;

  TRI_V8ToVPack(isolate, builder, args[0], false);

  bool replacedExisting = false;
  Result rv = registerUserFunction(vocbase, builder.slice(), replacedExisting);

  if (rv.fail()) {
    TRI_V8_THROW_EXCEPTION(rv);
  }

  TRI_V8_RETURN(replacedExisting);
  TRI_V8_TRY_CATCH_END;
}

static void JS_GetAqlUserFunctions(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "GET_AQL_USER_FUNCTIONS([<group-filter-string>])");
  }

  auto& vocbase = GetContextVocBase(isolate);
  VPackBuilder result;
  std::string functionFilterPrefix;

  if (args.Length() >= 1) {
    functionFilterPrefix = TRI_ObjectToString(isolate, args[0]);
  }

  Result rv = toArrayUserFunctions(vocbase, functionFilterPrefix, result);

  if (rv.fail()) {
    TRI_V8_THROW_EXCEPTION(rv);
  }

  v8::Handle<v8::Value> v8result = TRI_VPackToV8(isolate, result.slice());

  TRI_V8_RETURN(v8result);
  TRI_V8_TRY_CATCH_END;
}

void TRI_InitV8UserFunctions(v8::Isolate* isolate, v8::Handle<v8::Context>) {
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "UNREGISTER_AQL_USER_FUNCTION"),
      JS_UnregisterAQLUserFunction, true);

  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "UNREGISTER_AQL_USER_FUNCTION_GROUP"),
      JS_UnregisterAQLUserFunctionsGroup, true);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REGISTER_AQL_USER_FUNCTION"),
      JS_RegisterAqlUserFunction, true);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "GET_AQL_USER_FUNCTIONS"),
                               JS_GetAqlUserFunctions, true);
}
