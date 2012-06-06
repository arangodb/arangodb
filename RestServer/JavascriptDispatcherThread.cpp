////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread for JavaScript actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "JavascriptDispatcherThread.h"

#include "Actions/actions.h"
#include "Logger/Logger.h"
#include "V8/v8-actions.h"
#include "V8/v8-conv.h"
#include "V8/v8-query.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8/v8-vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      class ActionDispatcherThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new dispatcher thread
////////////////////////////////////////////////////////////////////////////////

JavascriptDispatcherThread::JavascriptDispatcherThread (rest::DispatcherQueue* queue,
                                                        TRI_vocbase_t* vocbase,
                                                        uint64_t gcInterval,
                                                        string const& actionQueue,
                                                        set<string> const& allowedContexts,
                                                        string startupModules,
                                                        JSLoader* startupLoader,
                                                        JSLoader* actionLoader)
  : ActionDispatcherThread(queue),
    _vocbase(vocbase),
    _gcInterval(gcInterval),
    _gc(0),
    _isolate(0),
    _context(),
    _actionQueue(actionQueue),
    _allowedContexts(allowedContexts),
    _startupModules(startupModules),
    _startupLoader(startupLoader),
    _actionLoader(actionLoader) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                    ActionDispatcherThread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void* JavascriptDispatcherThread::context () {
  return (void*) _isolate; // the isolate is the execution context
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                          DispatcherThread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void JavascriptDispatcherThread::reportStatus () {
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void JavascriptDispatcherThread::tick (bool idle) {
  _gc += (idle ? 10 : 1);

  if (_gc > _gcInterval) {
    LOGGER_TRACE << "collecting garbage...";

    while (! v8::V8::IdleNotification()) {
    }

    _gc = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void JavascriptDispatcherThread::run () {
  initialise();

  _isolate->Enter();
  _context->Enter();

  DispatcherThread::run();
  
  // free memory for this thread
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) _isolate->GetData();

  if (v8g) {
    delete v8g;
  }

  _context->Exit();
  _context.Dispose();
 
  _isolate->Exit();
  _isolate->Dispose();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the isolate and context
////////////////////////////////////////////////////////////////////////////////

void JavascriptDispatcherThread::initialise () {
  bool ok;
  char const* files[] = { "common/bootstrap/modules.js",
                          "common/bootstrap/print.js",
                          "common/bootstrap/errors.js",
                          "server/ahuacatl.js",
                          "server/server.js"
  };
  size_t i;

  // enter a new isolate
  _isolate = v8::Isolate::New();
  _isolate->Enter();

  // create the context
  _context = v8::Context::New(0);

  if (_context.IsEmpty()) {
    LOGGER_FATAL << "cannot initialize V8 engine";
    _isolate->Exit();
    TRI_FlushLogging();
    exit(EXIT_FAILURE);
  }

  _context->Enter();

  TRI_InitV8VocBridge(_context, _vocbase);
  TRI_InitV8Queries(_context);
  TRI_InitV8Actions(_context, _actionQueue, _allowedContexts);
  TRI_InitV8Conversions(_context);
  TRI_InitV8Utils(_context, _startupModules);
  TRI_InitV8Shell(_context);

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    ok = _startupLoader->loadScript(_context, files[i]);

    if (! ok) {
      LOGGER_FATAL << "cannot load json utilities from file '" << files[i] << "'";
      _context->Exit();
      _isolate->Exit();

      TRI_FlushLogging();
      exit(EXIT_FAILURE);
    }
  }

  // load all actions
  if (_actionLoader == 0) {
    LOGGER_WARNING << "no action loader has been defined";
  }
  else {
    ok = _actionLoader->executeAllScripts(_context);

    if (! ok) {
      LOGGER_FATAL << "cannot load actions from directory '" << _actionLoader->getDirectory() << "'";
    }
  }
  
  // and return from the context
  _context->Exit();
  _isolate->Exit();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
