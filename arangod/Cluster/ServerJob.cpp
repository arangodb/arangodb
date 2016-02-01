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

#include "ServerJob.h"

#include "Basics/MutexLocker.h"
#include "Basics/Logger.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ClusterInfo.h"
#include "Dispatcher/DispatcherQueue.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::rest;

static arangodb::Mutex ExecutorLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new db server job
////////////////////////////////////////////////////////////////////////////////

ServerJob::ServerJob(HeartbeatThread* heartbeat, TRI_server_t* server,
                     ApplicationV8* applicationV8)
    : Job("HttpServerJob"),
      _heartbeat(heartbeat),
      _server(server),
      _applicationV8(applicationV8),
      _shutdown(0),
      _abandon(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a db server job
////////////////////////////////////////////////////////////////////////////////

ServerJob::~ServerJob() {}

void ServerJob::work() {
  LOG(TRACE) << "starting plan update handler";

  if (_shutdown != 0) {
    return;
  }

  _heartbeat->setReady();

  bool result;

  {
    // only one plan change at a time
    MUTEX_LOCKER(mutexLocker, ExecutorLock);

    result = execute();
  }

  _heartbeat->removeDispatchedJob(result);
}

bool ServerJob::cancel() { return false; }

void ServerJob::cleanup(DispatcherQueue* queue) {
  queue->removeJob(this);
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute job
////////////////////////////////////////////////////////////////////////////////

bool ServerJob::execute() {
  // default to system database
  TRI_vocbase_t* vocbase =
      TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);

  if (vocbase == nullptr) {
    // database is gone
    return false;
  }

  ApplicationV8::V8Context* context =
      _applicationV8->enterContext(vocbase, true);

  if (context == nullptr) {
    TRI_ReleaseDatabaseServer(_server, vocbase);
    return false;
  }

  bool ok = true;
  auto isolate = context->isolate;
  try {
    v8::HandleScope scope(isolate);

    // execute script inside the context
    auto file = TRI_V8_ASCII_STRING("handle-plan-change");
    auto content =
        TRI_V8_ASCII_STRING("require('@arangodb/cluster').handlePlanChange();");
    v8::Handle<v8::Value> res = TRI_ExecuteJavaScriptString(
        isolate, isolate->GetCurrentContext(), content, file, false);
    if (res->IsBoolean() && res->IsTrue()) {
      LOG(ERR) << "An error occurred whilst executing the handlePlanChange in JavaScript.";
      ok = false;  // The heartbeat thread will notice this!
    }
    // invalidate our local cache, even if an error occurred
    ClusterInfo::instance()->flush();
  } catch (...) {
  }

  // get the pointer to the last used vocbase
  TRI_GET_GLOBALS();
  void* orig = v8g->_vocbase;

  _applicationV8->exitContext(context);
  TRI_ReleaseDatabaseServer(_server, static_cast<TRI_vocbase_t*>(orig));

  return ok;
}
