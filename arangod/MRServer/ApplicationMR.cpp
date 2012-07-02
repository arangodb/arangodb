////////////////////////////////////////////////////////////////////////////////
/// @brief MR enigne configuration
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

#include "ApplicationMR.h"

#include "Basics/ConditionLocker.h"
#include "Logger/Logger.h"
#include "MRServer/mr-actions.h"
#include "VocBase/vocbase.h"

using namespace triagens::basics;
using namespace triagens::arango;
using namespace std;

#include "mr/common/bootstrap/mr-error.h"
#include "mr/server/mr-server.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  class MRGcThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage collector
////////////////////////////////////////////////////////////////////////////////

  class MRGcThread : public Thread {
    public:
      MRGcThread (ApplicationMR* applicationMR) 
        : Thread("mr-gc"),
          _applicationMR(applicationMR) {
      }

    public:
      void run () {
        _applicationMR->collectGarbage();
      }

    private:
      ApplicationMR* _applicationMR;
  };

}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               class ApplicationMR
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

ApplicationMR::ApplicationMR (string const& binaryPath)
  : ApplicationFeature("MRuby"),
    _startupPath(),
    _startupModules("mr/modules"),
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

    _actionPath = binaryPath + "/../share/arango/mr/actions/system";
    _startupModules = binaryPath + "/../share/arango/mr/server/modules"
              + ";" + binaryPath + "/../share/arango/mr/common/modules";

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

    _actionPath = binaryPath + "/../mr/actions/system";
    _startupModules = binaryPath + "/../mr/server/modules"
              + ";" + binaryPath + "/../mr/common/modules";

  // .............................................................................
  // use absolute paths
  // .............................................................................

#else

#ifdef _PKGDATADIR_
    _actionPath = string(_PKGDATADIR_) + "/mr/actions/system";
    _startupModules = string(_PKGDATADIR_) + "/mr/server/modules"
              + ";" + string(_PKGDATADIR_) + "/mr/common/modules";

#endif

#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationMR::~ApplicationMR () {
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

void ApplicationMR::setConcurrency (size_t n) {
  _nrInstances = n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the database
////////////////////////////////////////////////////////////////////////////////

void ApplicationMR::setVocbase (TRI_vocbase_t* vocbase) {
  _vocbase = vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enters an context
////////////////////////////////////////////////////////////////////////////////

ApplicationMR::MRContext* ApplicationMR::enterContext () {
  CONDITION_LOCKER(guard, _contextCondition);

  while (_freeContexts.empty()) {
    LOGGER_DEBUG << "waiting for unused MRuby context";
    guard.wait();
  }

  LOGGER_TRACE << "found unused MRuby context";

  MRContext* context = _freeContexts.back();
  _freeContexts.pop_back();

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exists an context
////////////////////////////////////////////////////////////////////////////////

void ApplicationMR::exitContext (MRContext* context) {
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

  LOGGER_TRACE << "returned dirty MR context";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the garbage collection
////////////////////////////////////////////////////////////////////////////////

void ApplicationMR::collectGarbage () {
  while (_stopping == 0) {
    MRContext* context = 0;

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
      LOGGER_TRACE << "collecting MR garbage";

      mrb_garbage_collect(context->_mrb);

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

void ApplicationMR::disableActions () {
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

void ApplicationMR::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  options["RUBY Options:help-admin"]
    ("ruby.gc-interval", &_gcInterval, "Ruby garbage collection interval (each x requests)")
  ;

  options["DIRECTORY Options:help-admin"]
    ("ruby.action-directory", &_actionPath, "path to the Ruby action directory")
    ("ruby.modules-path", &_startupModules, "one or more directories separated by (semi-) colons")
    ("ruby.startup-directory", &_startupPath, "path to the directory containing alternate Ruby startup scripts")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationMR::prepare () {
  LOGGER_INFO << "using Ruby modules path '" << _startupModules << "'";

  // set up the startup loader
  if (_startupPath.empty()) {
    LOGGER_INFO << "using built-in Ruby startup files";

    _startupLoader.defineScript("common/bootstrap/error.rb", MR_common_bootstrap_error);
    _startupLoader.defineScript("server/server.rb", MR_server_server);
  }
  else {
    LOGGER_INFO << "using Ruby startup files at '" << _startupPath << "'";

    _startupLoader.setDirectory(_startupPath);
  }

  // set up action loader
  if (! _actionPath.empty()) {
    LOGGER_INFO << "using Ruby action files at '" << _actionPath << "'";

    _actionLoader.setDirectory(_actionPath);
  }

  // setup instances
  _contexts = new MRContext*[_nrInstances];

  for (size_t i = 0;  i < _nrInstances;  ++i) {
    bool ok = prepareMRInstance(i);

    if (! ok) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationMR::isStartable () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationMR::start () {
  _gcThread = new MRGcThread(this);
  _gcThread->start();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationMR::isStarted () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationMR::beginShutdown () {
  _stopping = 1;
  _contextCondition.broadcast();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationMR::shutdown () {
  _gcThread->stop();

  for (size_t i = 0;  i < _nrInstances;  ++i) {
    shutdownMRInstance(i);
  }
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
/// @brief prepares a MR instance
////////////////////////////////////////////////////////////////////////////////

bool ApplicationMR::prepareMRInstance (size_t i) {
  static char const* files[] = { "common/bootstrap/error.rb",
                                 "server/server.rb"
  };

  LOGGER_TRACE << "initialising MR context #" << i;

  MRContext* context = _contexts[i] = new MRContext();

  // create a new shell
  mrb_state* mrb = context->_mrb = MR_OpenShell();

  TRI_InitMRUtils(mrb);

  if (! _actionPath.empty()) {
    TRI_InitMRActions(mrb, this);
  }

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    bool ok = _startupLoader.loadScript(mrb, files[i]);

    if (! ok) {
      LOGGER_FATAL << "cannot load Ruby utilities from file '" << files[i] << "'";

      return false;
    }
  }

  // load all actions
  if (! _actionPath.empty()) {
    bool ok = _actionLoader.executeAllScripts(mrb);

    if (! ok) {
      LOGGER_FATAL << "cannot load Ruby actions from directory '" << _actionLoader.getDirectory() << "'";

      return false;
    }
  }

  // and return from the context
  LOGGER_TRACE << "initialised MR context #" << i;

  _freeContexts.push_back(context);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs a MR instances
////////////////////////////////////////////////////////////////////////////////

void ApplicationMR::shutdownMRInstance (size_t i) {
  LOGGER_TRACE << "shutting down MR context #" << i;

  MRContext* context = _contexts[i];
  mrb_state* mrb = context->_mrb;

  mrb_garbage_collect(mrb);

  MR_CloseShell(mrb);

  LOGGER_TRACE << "closed MR context #" << i;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
