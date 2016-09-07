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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DBServerAgencySync.h"

#include "Basics/MutexLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/HeartbeatThread.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::rest;

static arangodb::Mutex ExecutorLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new db server job
////////////////////////////////////////////////////////////////////////////////

DBServerAgencySync::DBServerAgencySync(HeartbeatThread* heartbeat)
    : Job("DBServerAgencySync"),
      _heartbeat(heartbeat),
      _shutdown(0),
      _abandon(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a db server job
////////////////////////////////////////////////////////////////////////////////

DBServerAgencySync::~DBServerAgencySync() {}

void DBServerAgencySync::work() {
  LOG_TOPIC(TRACE, Logger::CLUSTER) << "starting plan update handler";

  if (_shutdown != 0) {
    return;
  }

  _heartbeat->setReady();

  DBServerAgencySyncResult result;
  {
    // only one plan change at a time
    MUTEX_LOCKER(mutexLocker, ExecutorLock);

    result = execute();
  }

  _heartbeat->removeDispatchedJob(result);
}

bool DBServerAgencySync::cancel() { return false; }

void DBServerAgencySync::cleanup(DispatcherQueue* queue) {
  queue->removeJob(this);
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute job
////////////////////////////////////////////////////////////////////////////////

DBServerAgencySyncResult DBServerAgencySync::execute() {
  // default to system database

  DatabaseFeature* database = 
    ApplicationServer::getFeature<DatabaseFeature>("Database");

  TRI_vocbase_t* const vocbase = database->systemDatabase();

  DBServerAgencySyncResult result;
  if (vocbase == nullptr) {
    return result;
  }
  
  auto clusterInfo = ClusterInfo::instance();
  auto plan = clusterInfo->getPlan();
  auto current = clusterInfo->getCurrent();
  
  VocbaseGuard guard(vocbase);

  V8Context* context = V8DealerFeature::DEALER->enterContext(vocbase, true);

  if (context == nullptr) {
    return result;
  }

  TRI_DEFER(V8DealerFeature::DEALER->exitContext(context));

  auto isolate = context->_isolate;
  
  try {
    v8::HandleScope scope(isolate);

    // execute script inside the context
    auto file = TRI_V8_ASCII_STRING("handle-plan-change");
    auto content =
        TRI_V8_ASCII_STRING("require('@arangodb/cluster').handlePlanChange");
    
    v8::TryCatch tryCatch;
    v8::Handle<v8::Value> handlePlanChange = TRI_ExecuteJavaScriptString(
        isolate, isolate->GetCurrentContext(), content, file, false);
    
    if (tryCatch.HasCaught()) {
      TRI_LogV8Exception(isolate, &tryCatch);
      return result;
    }

    if (!handlePlanChange->IsFunction()) {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "handlePlanChange is not a function";
      return result;
    }

    v8::Handle<v8::Function> func =
      v8::Handle<v8::Function>::Cast(handlePlanChange);
    v8::Handle<v8::Value> args[2];
    // Keep the shared_ptr to the builder while we run TRI_VPackToV8 on the
    // slice(), just to be on the safe side:
    auto builder = clusterInfo->getPlan();
    args[0] = TRI_VPackToV8(isolate, builder->slice());
    builder = clusterInfo->getCurrent();
    args[1] = TRI_VPackToV8(isolate, builder->slice());
    
    v8::Handle<v8::Value> res =
      func->Call(isolate->GetCurrentContext()->Global(), 2, args);
    
    if (tryCatch.HasCaught()) {
      TRI_LogV8Exception(isolate, &tryCatch);
      return result;
    }
    
    result.success = true;  // unless overwritten by actual result

    if (res->IsObject()) {
      v8::Handle<v8::Object> o = res->ToObject();

      v8::Handle<v8::Array> names = o->GetOwnPropertyNames();
      uint32_t const n = names->Length();
      
      for (uint32_t i = 0; i < n; ++i) {
        v8::Handle<v8::Value> key = names->Get(i);
        v8::String::Utf8Value str(key);

        v8::Handle<v8::Value> value = o->Get(key);
        
        if (value->IsNumber()) {
          if (strcmp(*str, "plan") == 0) {
            result.planVersion =
              static_cast<uint64_t>(value->ToUint32()->Value());
          } else if (strcmp(*str, "current") == 0) {
            result.currentVersion =
              static_cast<uint64_t>(value->ToUint32()->Value());
          }
        } else if (value->IsBoolean() && strcmp(*str, "success")) {
          result.success = TRI_ObjectToBoolean(value);
        }
      }
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER)
        << "handlePlanChange returned a non-object";
      return result;
    }
    // invalidate our local cache, even if an error occurred
    clusterInfo->flush();
  } catch (...) {
  }

  return result;
}
