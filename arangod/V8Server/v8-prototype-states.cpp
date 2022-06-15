////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <Futures/Future.h>

#include "V8/v8-helper.h"
#include "v8-prototype-state.h"

#include "V8/v8-vpack.h"
#include "VocBase/vocbase.h"
#include "v8-externals.h"
#include "v8-vocbaseprivate.h"

#include "velocypack/Iterator.h"
#include "Inspection/VPack.h"

#include "Basics/StaticStrings.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Prototype/PrototypeLeaderState.h"
#include "Replication2/StateMachines/Prototype/PrototypeFollowerState.h"
#include "Replication2/StateMachines/Prototype/PrototypeStateMethods.h"

using namespace arangodb::replication2;

v8::Handle<v8::Object> WrapPrototypeState(v8::Isolate* isolate, LogId id) {
  v8::EscapableHandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(VocbasePrototypeStateTempl, v8::ObjectTemplate);
  auto& vocbase = GetContextVocBase(isolate);
  auto context = TRI_IGETC;
  v8::Handle<v8::Object> result =
      VocbasePrototypeStateTempl->NewInstance(TRI_IGETC).FromMaybe(
          v8::Local<v8::Object>());

  if (result.IsEmpty()) {
    return scope.Escape<v8::Object>(result);
  }

  result->SetInternalField(  // required for TRI_UnwrapClass(...)
      SLOT_CLASS_TYPE,
      v8::Integer::New(isolate, WRP_VOCBASE_PROTOTYPE_STATE_TYPE)  // args
  );
  result->SetInternalField(
      SLOT_CLASS,
      v8::Uint32::NewFromUnsigned(isolate, static_cast<uint32_t>(id.id())));

  TRI_GET_GLOBAL_STRING(_DbNameKey);
  result->Set(context, _DbNameKey, TRI_V8_STD_STRING(isolate, vocbase.name()))
      .FromMaybe(false);

  return scope.Escape<v8::Object>(result);
}

static LogId UnwrapPrototypeState(v8::Isolate* isolate,
                                  v8::Local<v8::Object> const& obj) {
  if (obj->InternalFieldCount() <= SLOT_CLASS) {
    return LogId{0};
  }
  auto slot = obj->GetInternalField(SLOT_CLASS_TYPE);
  if (slot->Int32Value(TRI_IGETC).ToChecked() !=
      WRP_VOCBASE_PROTOTYPE_STATE_TYPE) {
    return LogId{0};
  }

  return LogId{
      obj->GetInternalField(SLOT_CLASS)->Uint32Value(TRI_IGETC).ToChecked()};
}

static void JS_GetPrototypeState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_prototypeState(<id>)");
  }
  auto arg = args[0]->ToUint32(TRI_IGETC);
  if (arg.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_prototypeState(<id>) expects numerical identifier");
  }

  auto id = LogId{arg.ToLocalChecked()->Value()};
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to prototype state '") + to_string(id) + "'");
  }

  auto res = PrototypeStateMethods::createInstance(vocbase)->status(id).get();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res.result());
  }
  auto result = WrapPrototypeState(isolate, id);
  TRI_V8_RETURN(result);

  TRI_V8_TRY_CATCH_END
}

static void JS_CreatePrototypeState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN, std::string("Creating prototype state forbidden"));
  }

  auto& vocbase = GetContextVocBase(isolate);
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_createPrototypeState(<spec>)");
  }

  auto params = std::invoke([&] {
    VPackBuilder builder;
    TRI_V8ToVPack(isolate, builder, args[0], false, false);
    return arangodb::velocypack::deserialize<
        PrototypeStateMethods::CreateOptions>(builder.slice());
  });

  auto res =
      PrototypeStateMethods::createInstance(vocbase)->createState(params).get();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res.result());
  }
  auto result = WrapPrototypeState(isolate, res->id);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_Id(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto id = UnwrapPrototypeState(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to prototype state '") + to_string(id) + "'");
  }

  TRI_V8_RETURN(v8::Uint32::NewFromUnsigned(
      isolate, static_cast<std::uint32_t>(id.id())));

  TRI_V8_TRY_CATCH_END
}

static void JS_WriteInternal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapPrototypeState(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to prototype state '") + to_string(id) + "'");
  }

  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_writeInternal(kv, [options])");
  }

  std::unordered_map<std::string, std::string> kvs;
  {
    VPackBuilder builder;
    TRI_V8ToVPack(isolate, builder, args[0], false, false);
    for (auto const& [k, v] : VPackObjectIterator(builder.slice())) {
      kvs[k.copyString()] = v.copyString();
    }
  }

  auto options = PrototypeStateMethods::PrototypeWriteOptions{};
  if (args.Length() > 1) {
    VPackBuilder builder;
    builder.clear();
    TRI_V8ToVPack(isolate, builder, args[1], false, false);
    options = arangodb::velocypack::deserialize<
        PrototypeStateMethods::PrototypeWriteOptions>(builder.slice());
  }

  auto const logIndex = PrototypeStateMethods::createInstance(vocbase)
                            ->insert(id, kvs, options)
                            .get();

  VPackBuilder response;
  response.add(VPackValue(logIndex));
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));

  TRI_V8_TRY_CATCH_END
}

static void JS_Drop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapPrototypeState(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to prototype state '") + to_string(id) + "'");
  }

  auto const result =
      PrototypeStateMethods::createInstance(vocbase)->drop(id).get();
  if (result.fail()) {
    TRI_V8_THROW_EXCEPTION(result);
  }
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetSnapshot(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapPrototypeState(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to prototype state '") + to_string(id) + "'");
  }

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getSnapshot([waitForIndex])");
  }

  auto waitForIndex = LogIndex{0};
  if (args.Length() > 0) {
    auto arg = args[0]->ToUint32(TRI_IGETC);
    if (arg.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_USAGE(
          "getSnapshot(<idx>) expects numerical identifier");
    }
    waitForIndex = LogIndex{arg.ToLocalChecked()->Value()};
  }

  auto const result = PrototypeStateMethods::createInstance(vocbase)
                          ->getSnapshot(id, waitForIndex)
                          .get();
  if (result.fail()) {
    TRI_V8_THROW_EXCEPTION(result.result());
  }
  VPackBuilder response;
  {
    VPackObjectBuilder ob(&response);
    for (auto const& [k, v] : result.get()) {
      response.add(k, v);
    }
  }
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));

  TRI_V8_TRY_CATCH_END
}

static void JS_WaitFor(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapPrototypeState(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to prototype state '") + to_string(id) + "'");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("waitForApplied(<waitForIndex>)");
  }

  auto arg = args[0]->ToUint32(TRI_IGETC);
  if (arg.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "waitForApplied(<idx>) expects numerical identifier");
  }
  auto waitForIndex = LogIndex{arg.ToLocalChecked()->Value()};

  auto const result = PrototypeStateMethods::createInstance(vocbase)
                          ->waitForApplied(id, waitForIndex)
                          .get();
  if (result.fail()) {
    TRI_V8_THROW_EXCEPTION(result);
  }
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_ReadInternal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapPrototypeState(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to prototype state '") + to_string(id) + "'");
  }

  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_readInternal(ks [, options])");
  }

  std::vector<std::string> keys;
  VPackBuilder builder;
  TRI_V8ToVPack(isolate, builder, args[0], false, false);
  for (auto const& key : VPackArrayIterator(builder.slice())) {
    keys.emplace_back(key.copyString());
  }

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("get([waitForApplied])");
  }

  PrototypeStateMethods::ReadOptions readOptions;
  if (args.Length() > 1) {
    builder.clear();
    TRI_V8ToVPack(isolate, builder, args[1], false, false);
    auto const options = builder.slice();
    if (auto slice = options.get("waitForApplied"); !slice.isNone()) {
      readOptions.waitForApplied = slice.extract<LogIndex>();
    }
    if (auto slice = options.get("readFrom"); !slice.isNone()) {
      readOptions.readFrom = slice.extract<ParticipantId>();
    }
  }

  auto const result = PrototypeStateMethods::createInstance(vocbase)
                          ->get(id, keys, readOptions)
                          .get();
  if (result.fail()) {
    TRI_V8_THROW_EXCEPTION(result.result());
  }

  VPackBuilder response;
  {
    VPackObjectBuilder ob(&response);
    for (auto const& [k, v] : result.get()) {
      response.add(k, v);
    }
  }
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));

  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8PrototypeStates(TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  auto db = v8::Local<v8::ObjectTemplate>::New(isolate, v8g->VocbaseTempl);
  TRI_AddMethodVocbase(isolate, db,
                       TRI_V8_ASCII_STRING(isolate, "_prototypeState"),
                       JS_GetPrototypeState);
  TRI_AddMethodVocbase(isolate, db,
                       TRI_V8_ASCII_STRING(isolate, "_createPrototypeState"),
                       JS_CreatePrototypeState);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoPrototypeState"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);  // SLOT_CLASS_TYPE + SLOT_LOG_ID

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "id"), JS_Id);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_writeInternal"),
                       JS_WriteInternal);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_readInternal"),
                       JS_ReadInternal);
  TRI_AddMethodVocbase(
      isolate, rt, TRI_V8_ASCII_STRING(isolate, "waitForApplied"), JS_WaitFor);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "getSnapshot"),
                       JS_GetSnapshot);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "drop"),
                       JS_Drop);

  v8g->VocbasePrototypeStateTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "ArangoPrototypeState"),
      ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()));
}
