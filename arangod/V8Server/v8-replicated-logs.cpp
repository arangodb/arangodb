////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <Futures/Future.h>

#include "v8-replicated-logs.h"
#include "V8/v8-helper.h"

#include "VocBase/vocbase.h"
#include "v8-externals.h"
#include "v8-vocbaseprivate.h"
#include "V8/v8-vpack.h"

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/Methods.h"

using namespace arangodb::replication2;

v8::Handle<v8::Object> WrapReplicatedLog(v8::Isolate* isolate, LogId id) {
  v8::EscapableHandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(VocbaseReplicatedLogTempl, v8::ObjectTemplate);
  auto& vocbase = GetContextVocBase(isolate);
  auto context = TRI_IGETC;
  v8::Handle<v8::Object> result =
      VocbaseReplicatedLogTempl->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());

  if (result.IsEmpty()) {
    return scope.Escape<v8::Object>(result);
  }

  result->SetInternalField(  // required for TRI_UnwrapClass(...)
      SLOT_CLASS_TYPE, v8::Integer::New(isolate, WRP_VOCBASE_REPLICATED_LOG_TYPE)  // args
  );
  result->SetInternalField(SLOT_CLASS, v8::BigInt::NewFromUnsigned(isolate, id.id()));

  TRI_GET_GLOBAL_STRING(_DbNameKey);
  result->Set(context, _DbNameKey, TRI_V8_STD_STRING(isolate, vocbase.name())).FromMaybe(false);

  return scope.Escape<v8::Object>(result);
}

static void JS_GetReplicatedLog(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_replicatedLog(<id>)");
  }
  auto arg = args[0]->ToUint32(TRI_IGETC);
  if (arg.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_replicatedLog(<id>) expects numerical identifier");
  }

  auto id = LogId{arg.ToLocalChecked()->Value()};
  std::ignore = vocbase.getReplicatedLogById(id);
  auto result = WrapReplicatedLog(isolate, id);
  TRI_V8_RETURN(result);

  TRI_V8_TRY_CATCH_END
}

static void JS_CreateReplicatedLog(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_createReplicatedLog(<spec>)");
  }

  auto spec = std::invoke([&] {
    VPackBuilder builder;
    TRI_V8ToVPack(isolate, builder, args[0], false, false);
    return agency::LogPlanSpecification(agency::from_velocypack, builder.slice());
  });

  auto res = ReplicatedLogMethods::createInstance(vocbase)->createReplicatedLog(spec).get();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  auto result = WrapReplicatedLog(isolate, spec.id);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_Id(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  // auto context = TRI_IGETC;

  // TRI_V8_RETURN(result);

  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8ReplicatedLogs(TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  auto db = v8::Local<v8::ObjectTemplate>::New(isolate, v8g->VocbaseTempl);
  TRI_AddMethodVocbase(isolate, db,
                       TRI_V8_ASCII_STRING(isolate, "_replicatedLog"), JS_GetReplicatedLog);
  TRI_AddMethodVocbase(isolate, db,
                       TRI_V8_ASCII_STRING(isolate, "_createReplicatedLog"),
                       JS_CreateReplicatedLog);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ReplicatedLog"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(1);  // SLOT_CLASS_TYPE + SLOT_LOG_ID

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "id"), JS_Id);

  v8g->VocbaseReplicatedLogTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "ReplicatedLog"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()));
}
