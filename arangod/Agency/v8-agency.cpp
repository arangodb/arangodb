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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "v8-agency.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "Logger/LogMacros.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::consensus;

static void JS_StateAgent(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  Agent* agent = nullptr;
  try {
    AgencyFeature& feature = v8g->_server.getEnabledFeature<AgencyFeature>();
    agent = feature.agent();
    VPackBuilder builder;
    agent->state().toVelocyPack(builder);

    TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice()));
  } catch (std::exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("couldn't access agency feature: ") + e.what());
  }

  TRI_V8_TRY_CATCH_END
}

static void JS_EnabledAgent(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  TRI_V8_RETURN(v8::Boolean::New(isolate, v8g->_server.isEnabled<AgencyFeature>()));

  TRI_V8_TRY_CATCH_END
}

static void JS_LeadingAgent(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  Agent* agent = nullptr;
  try {
    AgencyFeature& feature = v8g->_server.getEnabledFeature<AgencyFeature>();
    agent = feature.agent();

  } catch (std::exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("couldn't access agency feature: ") + e.what());
  }

  v8::Handle<v8::Object> r = v8::Object::New(isolate);
  auto context = TRI_IGETC;
  
  r->Set(context, TRI_V8_ASCII_STRING(isolate, "leading"),
         v8::Boolean::New(isolate, agent->leading())).FromMaybe(false);

  TRI_V8_RETURN(r);
  TRI_V8_TRY_CATCH_END
}

static void JS_ReadAgent(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  Agent* agent = nullptr;
  try {
    AgencyFeature& feature = v8g->_server.getEnabledFeature<AgencyFeature>();
    agent = feature.agent();

  } catch (std::exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("couldn't access agency feature: ") + e.what());
  }

  query_t query = std::make_shared<Builder>();
  TRI_V8ToVPack(isolate, *query, args[0], false);

  read_ret_t ret = agent->read(query);

  if (ret.accepted) {  // Leading
    TRI_V8_RETURN(TRI_VPackToV8(isolate, ret.result->slice()));
  } else {  // Not leading
    TRI_V8_RETURN_FALSE();
  }

  TRI_V8_TRY_CATCH_END
}

static void JS_WriteAgent(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  Agent* agent = nullptr;
  try {
    AgencyFeature& feature = v8g->_server.getEnabledFeature<AgencyFeature>();
    agent = feature.agent();

  } catch (std::exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("couldn't access agency feature: ") + e.what());
  }

  query_t query = std::make_shared<Builder>();
  TRI_V8ToVPack(isolate, *query, args[0], false);

  write_ret_t ret = agent->write(query);

  if (ret.accepted) {  // Leading
    Builder body;
    body.openObject();
    body.add("results", VPackValue(VPackValueType::Array));
    for (auto const& index : ret.indices) {
      body.add(VPackValue(index));
    }
    body.close();
    body.close();

    // Wait for commit of highest except if it is 0?
    arangodb::consensus::index_t max_index = 0;
    try {
      max_index = *std::max_element(ret.indices.begin(), ret.indices.end());
    } catch (std::exception const& e) {
      LOG_TOPIC("bfcc6", WARN, Logger::AGENCY) << e.what();
    }

    if (max_index > 0) {
      agent->waitFor(max_index);
    }

    TRI_V8_RETURN(TRI_VPackToV8(isolate, body.slice()));
  } else {  // Not leading
    TRI_V8_RETURN_FALSE();
  }

  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Agency(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  TRI_ASSERT(v8g != nullptr);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  // ...........................................................................
  // generate the agency template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoAgent"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "enabled"), JS_EnabledAgent);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "leading"), JS_LeadingAgent);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "read"), JS_ReadAgent);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "write"), JS_WriteAgent);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "state"), JS_StateAgent);

  v8g->AgentTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoAgentCtor"));

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "ArangoAgentCtor"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate,
                                 TRI_V8_ASCII_STRING(isolate, "ArangoAgent"), aa);
  }
}
