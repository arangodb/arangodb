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

#include "RubyDispatcherThread.h"

#include "Actions/actions.h"
#include "Logger/Logger.h"
#include "MRuby/mr-actions.h"
#include "MRuby/mr-utils.h"

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

RubyDispatcherThread::RubyDispatcherThread (rest::DispatcherQueue* queue,
                                            TRI_vocbase_t* vocbase,
                                            string const& actionQueue,
                                            set<string> const& allowedContexts,
                                            string startupModules,
                                            MRLoader* startupLoader,
                                            MRLoader* actionLoader)
  : ActionDispatcherThread(queue),
    _vocbase(vocbase),
    _mrs(0),
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

void* RubyDispatcherThread::context () {
  return (void*) _mrs;
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

void RubyDispatcherThread::reportStatus () {
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void RubyDispatcherThread::tick (bool idle) {
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

void RubyDispatcherThread::run () {
  initialise();

  DispatcherThread::run();
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

void RubyDispatcherThread::initialise () {
  bool ok;
  char const* files[] = { "common/bootstrap/error.rb",
                          "server/server.rb"
  };
  size_t i;

  // create a new ruby shell
  _mrs = MR_OpenShell();

  TRI_InitMRUtils(_mrs);
  TRI_InitMRActions(_mrs);

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    ok = _startupLoader->loadScript(&_mrs->_mrb, files[i]);

    if (! ok) {
      LOGGER_FATAL << "cannot load MRuby utilities from file '" << files[i] << "'";
      TRI_FlushLogging();
      exit(EXIT_FAILURE);
    }
  }

  // load all actions
  if (_actionLoader == 0) {
    LOGGER_WARNING << "no action loader has been defined";
  }
  else {
    ok = _actionLoader->executeAllScripts(&_mrs->_mrb);

    if (! ok) {
      LOGGER_FATAL << "cannot load MRuby actions from directory '" << _actionLoader->getDirectory() << "'";
    }
  }
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
