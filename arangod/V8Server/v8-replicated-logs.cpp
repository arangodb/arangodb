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
#include "v8-replicated-logs.h"

#include "V8/v8-vpack.h"
#include "VocBase/vocbase.h"
#include "v8-externals.h"
#include "v8-vocbaseprivate.h"

#include "velocypack/Iterator.h"

#include "Replication2/Methods.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/Utilities.h"

using namespace arangodb::replication2;

v8::Handle<v8::Object> WrapReplicatedLog(v8::Isolate* isolate, LogId id) {
  v8::EscapableHandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(VocbaseReplicatedLogTempl, v8::ObjectTemplate);
  auto& vocbase = GetContextVocBase(isolate);
  auto context = TRI_IGETC;
  v8::Handle<v8::Object> result =
      VocbaseReplicatedLogTempl->NewInstance(TRI_IGETC).FromMaybe(
          v8::Local<v8::Object>());

  if (result.IsEmpty()) {
    return scope.Escape<v8::Object>(result);
  }

  result->SetInternalField(  // required for TRI_UnwrapClass(...)
      SLOT_CLASS_TYPE,
      v8::Integer::New(isolate, WRP_VOCBASE_REPLICATED_LOG_TYPE)  // args
  );
  result->SetInternalField(
      SLOT_CLASS,
      v8::Uint32::NewFromUnsigned(isolate, static_cast<uint32_t>(id.id())));

  TRI_GET_GLOBAL_STRING(_DbNameKey);
  result->Set(context, _DbNameKey, TRI_V8_STD_STRING(isolate, vocbase.name()))
      .FromMaybe(false);

  return scope.Escape<v8::Object>(result);
}

static LogId UnwrapReplicatedLog(v8::Isolate* isolate,
                                 v8::Local<v8::Object> const& obj) {
  if (obj->InternalFieldCount() <= SLOT_CLASS) {
    return LogId{0};
  }
  auto slot = obj->GetInternalField(SLOT_CLASS_TYPE);
  if (slot->Int32Value(TRI_IGETC).ToChecked() !=
      WRP_VOCBASE_REPLICATED_LOG_TYPE) {
    return LogId{0};
  }

  return LogId{
      obj->GetInternalField(SLOT_CLASS)->Uint32Value(TRI_IGETC).ToChecked()};
}

static void JS_GetReplicatedLog(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  std::ignore =
      ReplicatedLogMethods::createInstance(vocbase)->getStatus(id).get();
  auto result = WrapReplicatedLog(isolate, id);
  TRI_V8_RETURN(result);

  TRI_V8_TRY_CATCH_END
}

static void JS_CreateReplicatedLog(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN, std::string("Creating replicated log forbidden"));
  }

  auto& vocbase = GetContextVocBase(isolate);
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_createReplicatedLog(<spec>)");
  }

  auto spec = std::invoke([&] {
    VPackBuilder builder;
    TRI_V8ToVPack(isolate, builder, args[0], false, false);
    return agency::LogPlanSpecification(agency::from_velocypack,
                                        builder.slice());
  });

  auto res = ReplicatedLogMethods::createInstance(vocbase)
                 ->createReplicatedLog(spec)
                 .get();
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

  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  TRI_V8_RETURN(v8::Uint32::NewFromUnsigned(
      isolate, static_cast<std::uint32_t>(id.id())));

  TRI_V8_TRY_CATCH_END
}

static void JS_Drop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  if (auto res = ReplicatedLogMethods::createInstance(vocbase)
                     ->deleteReplicatedLog(id)
                     .get();
      !res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_TRY_CATCH_END
}

static void JS_Insert(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("insert(<payload>)");
  }

  VPackBufferUInt8 payload;
  VPackBuilder builder(payload);
  TRI_V8ToVPack(isolate, builder, args[0], false, false);

  auto result = ReplicatedLogMethods::createInstance(vocbase)
                    ->insert(id, LogPayload{payload})
                    .get();
  VPackBuilder response;
  {
    VPackObjectBuilder ob(&response);
    response.add("index", VPackValue(result.first));
    response.add(VPackValue("result"));
    result.second.toVelocyPack(response);
  }

  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_MultiInsert(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("multiInsert(<payload>)");
  }

  VPackBufferUInt8 payload;
  VPackBuilder builder(payload);
  TRI_V8ToVPack(isolate, builder, args[0], false, false);
  auto slice = builder.slice();
  if (!slice.isArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("multiInsert(<payload>) expects array");
  }

  replicated_log::VPackArrayToLogPayloadIterator iter{slice};
  auto result =
      ReplicatedLogMethods::createInstance(vocbase)->insert(id, iter).get();

  VPackBuilder response;
  {
    VPackObjectBuilder ob(&response);
    {
      VPackArrayBuilder ab{&response, "indexes"};
      for (auto const logIndex : result.first) {
        response.add(VPackValue(logIndex));
      }
    }
    response.add(VPackValue("result"));
    result.second.toVelocyPack(response);
  }

  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_Status(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  auto result =
      ReplicatedLogMethods::createInstance(vocbase)->getStatus(id).get();
  auto response = std::visit(
      [](auto&& r) {
        VPackBuilder response;
        r.toVelocyPack(response);
        return std::move(response);
      },
      std::move(result));
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_LocalStatus(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  auto result =
      ReplicatedLogMethods::createInstance(vocbase)->getLocalStatus(id).get();
  VPackBuilder response;
  result.toVelocyPack(response);
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_GlobalStatus(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  auto result =
      ReplicatedLogMethods::createInstance(vocbase)->getGlobalStatus(id).get();
  VPackBuilder response;
  result.toVelocyPack(response);
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_Head(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  std::size_t length;
  if (args.Length() == 0) {
    length = ReplicatedLogMethods::kDefaultLimit;
  } else if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("head(<limit = 10>)");
  } else {
    length = args[0]->ToUint32(TRI_IGETC).ToLocalChecked()->Value();
  }

  auto iter =
      ReplicatedLogMethods::createInstance(vocbase)->head(id, length).get();
  VPackBuilder response;
  {
    VPackArrayBuilder ab(&response);
    while (auto entry = iter->next()) {
      entry->toVelocyPack(response);
    }
  }
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_Tail(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  std::size_t length;
  if (args.Length() == 0) {
    length = ReplicatedLogMethods::kDefaultLimit;
  } else if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("tail(<limit = 10>)");
  } else {
    length = args[0]->ToUint32(TRI_IGETC).ToLocalChecked()->Value();
  }

  auto iter =
      ReplicatedLogMethods::createInstance(vocbase)->tail(id, length).get();
  VPackBuilder response;
  {
    VPackArrayBuilder ab(&response);
    while (auto entry = iter->next()) {
      entry->toVelocyPack(response);
    }
  }
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_Slice(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("slice(<start>, <stop>)");
  }
  auto [start, stop] = std::invoke([&]() -> std::pair<LogIndex, LogIndex> {
    auto startIdx = args[0]->ToUint32(TRI_IGETC).ToLocalChecked()->Value();
    if (args.Length() > 1) {
      auto stopIdx = args[1]->ToUint32(TRI_IGETC).ToLocalChecked()->Value();
      return std::make_pair(LogIndex{startIdx}, LogIndex{stopIdx});
    }
    return std::make_pair(
        LogIndex{startIdx},
        LogIndex{startIdx + ReplicatedLogMethods::kDefaultLimit + 1});
  });
  auto iter = ReplicatedLogMethods::createInstance(vocbase)
                  ->slice(id, start, stop)
                  .get();
  VPackBuilder response;
  {
    VPackArrayBuilder ab(&response);
    while (auto entry = iter->next()) {
      entry->toVelocyPack(response);
    }
  }
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_Poll(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("poll(<first = 0, limit = 10>)");
  }
  auto [first, limit] = std::invoke([&]() -> std::pair<LogIndex, std::size_t> {
    auto firstIdx = args[0]->ToUint32(TRI_IGETC).ToLocalChecked()->Value();
    if (args.Length() > 1) {
      auto limitValue = args[1]->ToUint32(TRI_IGETC).ToLocalChecked()->Value();
      return std::make_pair(LogIndex{firstIdx}, limitValue);
    }
    return std::make_pair(LogIndex{firstIdx},
                          ReplicatedLogMethods::kDefaultLimit);
  });

  auto iter = ReplicatedLogMethods::createInstance(vocbase)
                  ->poll(id, first, limit)
                  .get();
  VPackBuilder response;
  {
    VPackArrayBuilder ab(&response);
    while (auto entry = iter->next()) {
      entry->toVelocyPack(response);
    }
  }
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_At(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  LogIndex index;
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("at(<index>)");
  } else {
    index = LogIndex{args[0]->ToUint32(TRI_IGETC).ToLocalChecked()->Value()};
  }

  auto entry = ReplicatedLogMethods::createInstance(vocbase)
                   ->getLogEntryByIndex(id, index)
                   .get();
  VPackBuilder response;
  entry->toVelocyPack(response);
  TRI_V8_RETURN(TRI_VPackToV8(isolate, response.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_Release(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto id = UnwrapReplicatedLog(isolate, args.Holder());
  if (!arangodb::ExecContext::current().isAdminUser()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        std::string("No access to replicated log '") + to_string(id) + "'");
  }

  LogIndex index;
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("release(<index>)");
  } else {
    index = LogIndex{args[0]->ToUint32(TRI_IGETC).ToLocalChecked()->Value()};
  }

  auto result =
      ReplicatedLogMethods::createInstance(vocbase)->release(id, index).get();
  if (result.fail()) {
    THROW_ARANGO_EXCEPTION(result);
  }
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8ReplicatedLogs(TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  auto db = v8::Local<v8::ObjectTemplate>::New(isolate, v8g->VocbaseTempl);
  TRI_AddMethodVocbase(isolate, db,
                       TRI_V8_ASCII_STRING(isolate, "_replicatedLog"),
                       JS_GetReplicatedLog);
  TRI_AddMethodVocbase(isolate, db,
                       TRI_V8_ASCII_STRING(isolate, "_createReplicatedLog"),
                       JS_CreateReplicatedLog);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoReplicatedLog"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);  // SLOT_CLASS_TYPE + SLOT_LOG_ID

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "id"), JS_Id);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "drop"),
                       JS_Drop);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "insert"),
                       JS_Insert);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "multiInsert"),
                       JS_MultiInsert);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "status"),
                       JS_Status);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "localStatus"),
                       JS_LocalStatus);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "globalStatus"),
                       JS_GlobalStatus);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "head"),
                       JS_Head);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "tail"),
                       JS_Tail);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "slice"),
                       JS_Slice);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "at"), JS_At);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "release"),
                       JS_Release);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "poll"),
                       JS_Poll);

  v8g->VocbaseReplicatedLogTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "ReplicatedLog"),
      ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()));
}
