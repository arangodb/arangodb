////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "v8-replication.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/V8Context.h"
#include "Utils/DatabaseGuard.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbaseprivate.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_StateLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  // FIXME: use code in RestReplicationHandler and get rid of storage-engine
  //        dependent code here
  //
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  StorageEngine& engine = v8g->server().getFeature<DatabaseFeature>().engine();
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  TRI_vocbase_t& vocbase = GetContextVocBase(isolate);

  VPackBuilder builder;
  auto res = engine.createLoggerState(&vocbase, builder);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  v8::Handle<v8::Value> resultValue = TRI_VPackToV8(isolate, builder.slice());
  result = v8::Handle<v8::Object>::Cast(resultValue);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the tick ranges that can be provided by the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_TickRangesLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  v8::Handle<v8::Array> result;

  VPackBuilder builder;
  TRI_GET_GLOBALS();
  StorageEngine& engine = v8g->server().getFeature<DatabaseFeature>().engine();
  Result res = engine.createTickRanges(builder);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> resultValue = TRI_VPackToV8(isolate, builder.slice());
  result = v8::Handle<v8::Array>::Cast(resultValue);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the first tick that can be provided by the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_FirstTickLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_voc_tick_t tick = UINT64_MAX;
  TRI_GET_GLOBALS();
  StorageEngine& engine = v8g->server().getFeature<DatabaseFeature>().engine();
  Result res = engine.firstTick(tick);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (tick == UINT64_MAX) {
    TRI_V8_RETURN(v8::Null(isolate));
  }

  TRI_V8_RETURN(TRI_V8UInt64String<TRI_voc_tick_t>(isolate, tick));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last WAL entries
////////////////////////////////////////////////////////////////////////////////

static void JS_LastLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "REPLICATION_LOGGER_LAST(<fromTick>, <toTick>)");
  }

  TRI_voc_tick_t tickStart = TRI_ObjectToUInt64(isolate, args[0], true);
  TRI_voc_tick_t tickEnd = TRI_ObjectToUInt64(isolate, args[1], true);
  if (tickEnd <= tickStart) {
    TRI_V8_THROW_EXCEPTION_USAGE("tickStart < tickEnd");
  }

  auto origin =
      transaction::OperationOriginREST{"returning last documents from WAL"};
  auto transactionContext =
      transaction::V8Context::create(vocbase, origin, true);
  VPackBuilder builder(transactionContext->getVPackOptions());
  TRI_GET_GLOBALS();
  StorageEngine& engine = v8g->server().getFeature<DatabaseFeature>().engine();
  Result res = engine.lastLogger(vocbase, tickStart, tickEnd, builder);
  v8::Handle<v8::Value> result;

  if (res.fail()) {
    result = v8::Null(isolate);
    TRI_V8_THROW_EXCEPTION(res);
  }

  result = TRI_VPackToV8(isolate, builder.slice(),
                         transactionContext->getVPackOptions());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server's id
////////////////////////////////////////////////////////////////////////////////

static void JS_ServerIdReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::string const serverId = StringUtils::itoa(ServerIdFeature::getId().id());
  TRI_V8_RETURN_STD_STRING(serverId);
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Replication(v8::Isolate* isolate,
                           v8::Handle<v8::Context> context,
                           TRI_vocbase_t* vocbase, size_t threadNumber,
                           TRI_v8_global_t* v8g) {
  // replication functions. not intended to be used by end users

  // logger functions
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_STATE"),
      JS_StateLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_LAST"),
      JS_LastLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_TICK_RANGES"),
      JS_TickRangesLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_FIRST_TICK"),
      JS_FirstTickLoggerReplication, true);

  // other functions
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_SERVER_ID"),
      JS_ServerIdReplication, true);
}
