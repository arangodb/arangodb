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
// --SECTION--                                      class ActionDisptacherThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                           public static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief action path
////////////////////////////////////////////////////////////////////////////////

JSLoader* ActionDisptacherThread::_actionLoader = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief startup path
////////////////////////////////////////////////////////////////////////////////

JSLoader* ActionDisptacherThread::_startupLoader = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* ActionDisptacherThread::_vocbase = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief modules path
////////////////////////////////////////////////////////////////////////////////

string ActionDisptacherThread::_startupModules;

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

ActionDisptacherThread::ActionDisptacherThread (DispatcherQueue* queue, string const& userContext)
  : DispatcherThread(queue),
    _report(false),
    _isolate(0),
    _context(),
    _userContext(userContext) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a dispatcher thread
////////////////////////////////////////////////////////////////////////////////

ActionDisptacherThread::~ActionDisptacherThread () {
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

void ActionDisptacherThread::reportStatus () {
  _report = true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ActionDisptacherThread::tick () {
  while(!v8::V8::IdleNotification()) {
  }

  if (! _report || _isolate == 0) {
    return;
  }

  _report = false;

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) _isolate->GetData();

  if (v8g == 0) {
    return;
  }

  LOGGER_DEBUG << "active queries: " << v8g->JSQueries.size();
  LOGGER_DEBUG << "active result-sets: " << v8g->JSResultSets.size();
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

void ActionDisptacherThread::run () {
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

JSLoader* ActionDisptacherThread::actionLoader () {
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

void ActionDisptacherThread::initialise () {
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
  TRI_InitV8Actions(_context, _userContext.c_str());
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
    ok = actionLoader()->loadAllScripts(_context);

    if (! ok) {
      LOGGER_FATAL << "cannot load actions from directory '" << loader->getDirectory() << "'";
      cerr  << "cannot load actions from directory '" << loader->getDirectory() << "'\n";
      _context->Exit();
      _isolate->Exit();
      exit(EXIT_FAILURE);
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
