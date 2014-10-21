////////////////////////////////////////////////////////////////////////////////
/// @brief DB server job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ServerJob.h"
#include "Basics/MutexLocker.h"
#include "Basics/logging.h"
#include "Cluster/HeartbeatThread.h"
#include "V8Server/ApplicationV8.h"
#include "V8/v8-utils.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 class ServerJob
// -----------------------------------------------------------------------------

static triagens::basics::Mutex ExecutorLock;

using namespace triagens::arango;
using namespace triagens::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief general server job
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new db server job
////////////////////////////////////////////////////////////////////////////////

ServerJob::ServerJob (HeartbeatThread* heartbeat,
                      TRI_server_t* server,
                      ApplicationV8* applicationV8)
  : Job("HttpServerJob"),
    _heartbeat(heartbeat),
    _server(server),
    _applicationV8(applicationV8),
    _shutdown(0),
    _abandon(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a db server job
////////////////////////////////////////////////////////////////////////////////

ServerJob::~ServerJob () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::status_t ServerJob::work () {
  LOG_TRACE("starting plan update handler");

  if (_shutdown != 0) {
    return status_t(Job::JOB_DONE);
  }

  bool result = execute();
  _heartbeat->ready(true);

  if (result) {
    return status_t(Job::JOB_DONE);
  }

  return status_t(Job::JOB_FAILED);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ServerJob::cancel (bool running) {
  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute job
////////////////////////////////////////////////////////////////////////////////

bool ServerJob::execute () {
  // default to system database
  TRI_vocbase_t* vocbase = TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);

  if (vocbase == nullptr) {
    // database is gone
    return false;
  }

  // only one plan change at a time
  MUTEX_LOCKER(ExecutorLock);

  ApplicationV8::V8Context* context = _applicationV8->enterContext("STANDARD", vocbase, false, true);

  if (context == nullptr) {
    TRI_ReleaseDatabaseServer(_server, vocbase);
    return false;
  }

  try {
    v8::HandleScope scope;
    // execute script inside the context
    char const* file = "handle-plan-change";
    char const* content = "require('org/arangodb/cluster').handlePlanChange();";

    TRI_ExecuteJavaScriptString(v8::Context::GetCurrent(), v8::String::New(content, (int) strlen(content)), v8::String::New(file), false);
  }
  catch (...) {
  }


  // get the pointer to the last used vocbase
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(context->_isolate->GetData());
  void* orig = v8g->_vocbase;

  _applicationV8->exitContext(context);
  TRI_ReleaseDatabaseServer(_server, static_cast<TRI_vocbase_t*>(orig));

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
