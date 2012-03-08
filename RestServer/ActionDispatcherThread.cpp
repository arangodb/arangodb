////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread for actions
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

#include "ActionDispatcherThread.h"

#include "BasicsC/files.h"
#include "BasicsC/strings.h"
#include "Logger/Logger.h"
#include "Rest/Initialise.h"
#include "V8/v8-actions.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8/v8-vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::avocado;

// -----------------------------------------------------------------------------
// --SECTION--                                      class ActionDispatcherThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                           public static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief startup path
////////////////////////////////////////////////////////////////////////////////

JSLoader* ActionDispatcherThread::_startupLoader = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* ActionDispatcherThread::_vocbase = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief modules path
////////////////////////////////////////////////////////////////////////////////

string ActionDispatcherThread::_startupModules;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new dispatcher thread
////////////////////////////////////////////////////////////////////////////////

ActionDispatcherThread::ActionDispatcherThread (DispatcherQueue* queue,
                                                string const& actionQueue,
                                                JSLoader* actionLoader)
  : DispatcherThread(queue),
    _isolate(0),
    _context(),
    _actionQueue(actionQueue),
    _actionLoader(actionLoader),
    _gc(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a dispatcher thread
////////////////////////////////////////////////////////////////////////////////

ActionDispatcherThread::~ActionDispatcherThread () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                          DispatcherThread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ActionDispatcherThread::reportStatus () {
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ActionDispatcherThread::tick (bool idle) {
  static uint64_t const INTERVALL = 1000;

  _gc += (idle ? 10 : 1);

  if (_gc > INTERVALL) {
    while(!v8::V8::IdleNotification()) {
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
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ActionDispatcherThread::run () {
  initialise();

  _isolate->Enter();
  _context->Enter();

  DispatcherThread::run();

  _context->Exit();
  _context.Dispose();

  _isolate->Exit();
  _isolate->Dispose();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// returns the action loader
////////////////////////////////////////////////////////////////////////////////

JSLoader* ActionDispatcherThread::actionLoader () {
  return _actionLoader;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the isolate and context
////////////////////////////////////////////////////////////////////////////////

void ActionDispatcherThread::initialise () {
  bool ok;
  char const* files[] = { "bootstrap/modules.js",
                          "bootstrap/print.js",
                          "server/modules.js",
                          "server/json.js",
                          "server/aql.js"
  };
  size_t i;

  // enter a new isolate
  _isolate = v8::Isolate::New();
  _isolate->Enter();

  // create the context
  _context = v8::Context::New(0);

  if (_context.IsEmpty()) {
    LOGGER_FATAL << "cannot initialize V8 engine";
    cerr << "cannot initialize V8 engine\n";
    _isolate->Exit();
    exit(EXIT_FAILURE);
  }

  _context->Enter();

  TRI_InitV8VocBridge(_context, _vocbase);
  TRI_InitV8Actions(_context, _actionQueue.c_str());
  TRI_InitV8Conversions(_context);
  TRI_InitV8Utils(_context, _startupModules);
  TRI_InitV8Shell(_context);

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    ok = _startupLoader->loadScript(_context, files[i]);

    if (! ok) {
      LOGGER_FATAL << "cannot load json utilities from file '" << files[i] << "'";
      cerr << "cannot load json utilities from file '" << files[i] << "'\n";
      _context->Exit();
      _isolate->Exit();
      exit(EXIT_FAILURE);
    }
  }

  // load all actions
  JSLoader* loader = actionLoader();

  if (loader == 0) {
    LOGGER_WARNING << "no action loader has been defined";
  }
  else {
    ok = actionLoader()->executeAllScripts(_context);

    if (! ok) {
      LOGGER_FATAL << "cannot load actions from directory '" << loader->getDirectory() << "'";
    }
  }

  // and return from the context
  _context->Exit();
  _isolate->Exit();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
