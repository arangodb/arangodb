////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread for JavaScript actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-actions.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-vocbase.h"

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

#if 0
void* JavascriptDispatcherThread::context () {
  return (void*) _isolate; // the isolate is the execution context
}
#endif

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

#if 0
void JavascriptDispatcherThread::reportStatus () {
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

#if 0
void JavascriptDispatcherThread::tick (bool idle) {
  _gc += (idle ? 10 : 1);

  if (_gc > _gcInterval) {
    LOGGER_TRACE << "collecting garbage...";

    while (! v8::V8::IdleNotification()) {
    }

    _gc = 0;
  }
}
#endif

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

#if 0
void JavascriptDispatcherThread::run () {
  initialise();

  _isolate->Enter();
  _context->Enter();

  DispatcherThread::run();

  _context->Exit();
  _context.Dispose();

  while (!v8::V8::IdleNotification()) {
  }
 
  // free memory for this thread
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) _isolate->GetData();

  if (v8g) {
    delete v8g;
  }

  _isolate->Exit();
  _isolate->Dispose();
}
#endif

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
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
