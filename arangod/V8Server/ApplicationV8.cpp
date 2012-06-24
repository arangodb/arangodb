////////////////////////////////////////////////////////////////////////////////
/// @brief V8 enigne configuration
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

#include "ApplicationV8.h"

#include "Basics/ConditionLocker.h"
#include "Logger/Logger.h"
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-actions.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-vocbase.h"

using namespace triagens::basics;
using namespace triagens::arango;
using namespace std;

#include "js/common/bootstrap/js-modules.h"
#include "js/common/bootstrap/js-print.h"
#include "js/common/bootstrap/js-errors.h"
#include "js/server/js-ahuacatl.h"
#include "js/server/js-server.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  class V8GcThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage collector
////////////////////////////////////////////////////////////////////////////////

  class V8GcThread : public Thread {
    public:
      V8GcThread (ApplicationV8* applicationV8) 
        : Thread("v8-gc"),
          _applicationV8(applicationV8) {
      }

    public:
      void run () {
        _applicationV8->collectGarbage();
      }

    private:
      ApplicationV8* _applicationV8;
  };

}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               class ApplicationV8
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationV8::ApplicationV8 (string const& binaryPath)
  : ApplicationFeature("V8"),
    _startupPath(),
    _startupModules("js/modules"),
    _actionPath(),
    _gcInterval(1000),
    _startupLoader(),
    _actionLoader(),
    _vocbase(0),
    _nrInstances(0),
    _contexts(0),
    _contextCondition(),
    _freeContexts(),
    _dirtyContexts(),
    _stopping(0) {

  // .............................................................................
  // use relative system paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_SYSTEM

    _actionPath = binaryPath + "/../share/arango/js/actions/system";
    _startupModules = binaryPath + "/../share/arango/js/server/modules"
                + ";" + binaryPath + "/../share/arango/js/common/modules";

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

#ifdef TRI_SYSTEM_ACTION_PATH
    _actionPath = TRI_SYSTEM_ACTION_PATH;
#else
    _actionPath = binaryPath + "/../js/actions/system";
#endif

#ifdef TRI_STARTUP_MODULES_PATH
    _startupModules = TRI_STARTUP_MODULES_PATH;
#else
    _startupModules = binaryPath + "/../js/server/modules"
                + ";" + binaryPath + "/../js/common/modules";
#endif

  // .............................................................................
  // use absolute paths
  // .............................................................................

#else

#ifdef _PKGDATADIR_
    _actionPath = string(_PKGDATADIR_) + "/js/actions/system";
    _startupModules = string(_PKGDATADIR_) + "/js/server/modules"
                + ";" + string(_PKGDATADIR_) + "/js/common/modules";

#endif

#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationV8::~ApplicationV8 () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the concurrency
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::setConcurrency (size_t n) {
  _nrInstances = n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the database
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::setVocbase (TRI_vocbase_t* vocbase) {
  _vocbase = vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enters an context
////////////////////////////////////////////////////////////////////////////////

ApplicationV8::V8Context* ApplicationV8::enterContext () {
  CONDITION_LOCKER(guard, _contextCondition);

  while (_freeContexts.empty()) {
    LOGGER_DEBUG << "waiting for unused V8 context";
    guard.wait();
  }

  LOGGER_TRACE << "found unused V8 context";

  V8Context* context = _freeContexts.back();
  _freeContexts.pop_back();

  context->_locker = new v8::Locker(context->_isolate);
  context->_isolate->Enter();
  context->_context->Enter();

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exists an context
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::exitContext (V8Context* context) {
  context->_context->Exit();
  context->_isolate->Exit();
  delete context->_locker;

  ++context->_dirt;

  {
    CONDITION_LOCKER(guard, _contextCondition);

    if (context->_dirt < _gcInterval) {
      _freeContexts.push_back(context);
    }
    else {
      _dirtyContexts.push_back(context);
    }

    guard.broadcast();
  }

  LOGGER_TRACE << "returned dirty V8 context";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the garbage collection
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::collectGarbage () {
  while (_stopping == 0) {
    V8Context* context = 0;

    {
      CONDITION_LOCKER(guard, _contextCondition);

      if (_dirtyContexts.empty()) {
        guard.wait();
      }

      if (! _dirtyContexts.empty()) {
        context = _dirtyContexts.back();
        _dirtyContexts.pop_back();
      }
    }

    if (context != 0) {
      LOGGER_TRACE << "collecting V8 garbage";

      context->_locker = new v8::Locker(context->_isolate);
      context->_isolate->Enter();
      context->_context->Enter();

      while (!v8::V8::IdleNotification()) {
      }

      context->_context->Exit();
      context->_isolate->Exit();
      delete context->_locker;

      context->_dirt = 0;

      {
        CONDITION_LOCKER(guard, _contextCondition);

        _freeContexts.push_back(context);
        guard.broadcast();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disables actions
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::disableActions () {
  _actionPath.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  options["JAVASCRIPT Options:help-admin"]
    ("javascript.gc-interval", &_gcInterval, "JavaScript garbage collection interval (each x requests)")
  ;

  options["DIRECTORY Options:help-admin"]
    ("javascript.action-directory", &_actionPath, "path to the JavaScript action directory")
    ("javascript.modules-path", &_startupModules, "one or more directories separated by (semi-) colons")
    ("javascript.startup-directory", &_startupPath, "path to the directory containing alternate JavaScript startup scripts")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::prepare () {
  LOGGER_INFO << "using JavaScript modules path '" << _startupModules << "'";

  // set up the startup loader
  if (_startupPath.empty()) {
    LOGGER_INFO << "using built-in JavaScript startup files";

    _startupLoader.defineScript("common/bootstrap/modules.js", JS_common_bootstrap_modules);
    _startupLoader.defineScript("common/bootstrap/print.js", JS_common_bootstrap_print);
    _startupLoader.defineScript("common/bootstrap/errors.js", JS_common_bootstrap_errors);
    _startupLoader.defineScript("server/ahuacatl.js", JS_server_ahuacatl);
    _startupLoader.defineScript("server/server.js", JS_server_server);
  }
  else {
    LOGGER_INFO << "using JavaScript startup files at '" << _startupPath << "'";

    _startupLoader.setDirectory(_startupPath);
  }

  // set up action loader
  if (! _actionPath.empty()) {
    LOGGER_INFO << "using JavaScript action files at '" << _actionPath << "'";

    _actionLoader.setDirectory(_actionPath);
  }

  // setup instances
  _contexts = new V8Context*[_nrInstances];

  for (size_t i = 0;  i < _nrInstances;  ++i) {
    bool ok = prepareV8Instance(i);

    if (! ok) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::isStartable () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::start () {
  _gcThread = new V8GcThread(this);
  _gcThread->start();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::isStarted () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::beginShutdown () {
  _stopping = 1;
  _contextCondition.broadcast();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::shutdown () {
  _contextCondition.broadcast();
  usleep(1000);
  _gcThread->stop();
  _gcThread->join();
  delete _gcThread;

  for (size_t i = 0;  i < _nrInstances;  ++i) {
    shutdownV8Instance(i);
  }

  delete[] _contexts;
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
/// @brief prepares a V8 instance
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::prepareV8Instance (size_t i) {
  static char const* files[] = { "common/bootstrap/modules.js",
                                 "common/bootstrap/print.js",
                                 "common/bootstrap/errors.js",
                                 "server/ahuacatl.js",
                                 "server/server.js"
  };

  LOGGER_TRACE << "initialising V8 context #" << i;

  V8Context* context = _contexts[i] = new V8Context();

  // enter a new isolate
  context->_isolate = v8::Isolate::New();
  context->_locker = new v8::Locker(context->_isolate);
  context->_isolate->Enter();

  // create the context
  context->_context = v8::Context::New();

  if (context->_context.IsEmpty()) {
    LOGGER_FATAL << "cannot initialize V8 engine";

    context->_isolate->Exit();
    delete context->_locker;

    return false;
  }

  context->_context->Enter();

  TRI_InitV8VocBridge(context->_context, _vocbase);
  TRI_InitV8Queries(context->_context);

  if (! _actionPath.empty()) {
    TRI_InitV8Actions(context->_context, this);
  }

  TRI_InitV8Conversions(context->_context);
  TRI_InitV8Utils(context->_context, _startupModules);
  TRI_InitV8Shell(context->_context);

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    bool ok = _startupLoader.loadScript(context->_context, files[i]);

    if (! ok) {
      LOGGER_FATAL << "cannot load JavaScript utilities from file '" << files[i] << "'";

      context->_context->Exit();
      context->_isolate->Exit();
      delete context->_locker;

      return false;
    }
  }

  // load all actions
  if (! _actionPath.empty()) {
    bool ok = _actionLoader.executeAllScripts(context->_context);

    if (! ok) {
      LOGGER_FATAL << "cannot load JavaScript actions from directory '" << _actionLoader.getDirectory() << "'";

      context->_context->Exit();
      context->_isolate->Exit();
      delete context->_locker;

      return false;
    }
  }

  // and return from the context
  context->_context->Exit();
  context->_isolate->Exit();
  delete context->_locker;
  
  LOGGER_TRACE << "initialised V8 context #" << i;

  _freeContexts.push_back(context);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs a V8 instances
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::shutdownV8Instance (size_t i) {
  LOGGER_TRACE << "shutting down V8 context #" << i;

  V8Context* context = _contexts[i];

  context->_locker = new v8::Locker(context->_isolate);
  context->_isolate->Enter();
  context->_context->Enter();

  while (!v8::V8::IdleNotification()) {
  }
 
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) context->_isolate->GetData();

  if (v8g) {
    delete v8g;
  }

  context->_context->Exit();
  context->_context.Dispose();

  context->_isolate->Exit();
  delete context->_locker;

  context->_isolate->Dispose();

  delete context;

  LOGGER_TRACE << "closed V8 context #" << i;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
