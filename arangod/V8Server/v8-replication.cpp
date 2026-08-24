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
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbaseprivate.h"

#include <velocypack/Builder.h>
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

void TRI_InitV8Replication(v8::Isolate* isolate,
                           v8::Handle<v8::Context> context,
                           TRI_vocbase_t* vocbase, size_t threadNumber,
                           TRI_v8_global_t* v8g) {
  // replication functions. not intended to be used by end users

  // logger functions
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_STATE"),
      JS_StateLoggerReplication, true);
}
