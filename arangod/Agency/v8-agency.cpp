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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "v8-agency.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::consensus;

static void JS_EnabledAgent(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_V8_RETURN(v8::Boolean::New(
      isolate, ApplicationServer::server->isEnabled("Agency")));

  TRI_V8_TRY_CATCH_END
}

static void JS_LeadingAgent(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  Agent* agent = nullptr;
  try {
    AgencyFeature* feature =
        ApplicationServer::getEnabledFeature<AgencyFeature>("Agency");
    agent = feature->agent();

  } catch (std::exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("couldn't access agency feature: ") + e.what());
  }

  v8::Handle<v8::Object> r = v8::Object::New(isolate);

  r->Set(TRI_V8_ASCII_STRING("leading"),
         v8::Boolean::New(isolate, agent->leading()));

  TRI_V8_RETURN(r);
  TRI_V8_TRY_CATCH_END
}

static void JS_ReadAgent(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  Agent* agent = nullptr;
  try {
    AgencyFeature* feature =
        ApplicationServer::getEnabledFeature<AgencyFeature>("Agency");
    agent = feature->agent();

  } catch (std::exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("couldn't access agency feature: ") + e.what());
  }

  query_t query = std::make_shared<Builder>();
  int res = TRI_V8ToVPack(isolate, *query, args[0], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

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

  Agent* agent = nullptr;
  try {
    AgencyFeature* feature =
        ApplicationServer::getEnabledFeature<AgencyFeature>("Agency");
    agent = feature->agent();

  } catch (std::exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("couldn't access agency feature: ") + e.what());
  }

  query_t query = std::make_shared<Builder>();
  int res = TRI_V8ToVPack(isolate, *query, args[0], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  write_ret_t ret = agent->write(query);

  if (ret.accepted) {  // Leading

    size_t errors = 0;
    Builder body;
    body.openObject();
    body.add("results", VPackValue(VPackValueType::Array));
    for (auto const& index : ret.indices) {
      body.add(VPackValue(index));
      if (index == 0) {
        errors++;
      }
    }
    body.close();
    body.close();

    // Wait for commit of highest except if it is 0?
    arangodb::consensus::index_t max_index = 0;
    try {
      max_index = *std::max_element(ret.indices.begin(), ret.indices.end());
    } catch (std::exception const& e) {
      LOG_TOPIC(WARN, Logger::AGENCY) << e.what() << " " << __FILE__
                                      << __LINE__;
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
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoAgent"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("enabled"),
                       JS_EnabledAgent);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("leading"),
                       JS_LeadingAgent);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("read"), JS_ReadAgent);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("write"),
                       JS_WriteAgent);

  v8g->AgentTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoAgentCtor"));

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ArangoAgentCtor"),
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance();
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("ArangoAgent"), aa);
  }
}
