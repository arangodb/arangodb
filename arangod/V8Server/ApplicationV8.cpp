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

using namespace triagens::arango;
using namespace std;

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
    _startupPathJS(),
    _startupModulesJS("js/modules"),
    _actionPathJS(),
    _gcIntervalJS(1000) {

  // .............................................................................
  // use relative system paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_SYSTEM

    _actionPathJS = binaryPath + "/../share/arango/js/actions/system";
    _startupModulesJS = binaryPath + "/../share/arango/js/server/modules"
                + ";" + binaryPath + "/../share/arango/js/common/modules";

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

#ifdef TRI_SYSTEM_ACTION_PATH
    _actionPathJS = TRI_SYSTEM_ACTION_PATH;
#else
    _actionPathJS = binaryPath + "/../js/actions/system";
#endif

#ifdef TRI_STARTUP_MODULES_PATH
    _startupModulesJS = TRI_STARTUP_MODULES_PATH;
#else
    _startupModulesJS = binaryPath + "/../js/server/modules"
                + ";" + binaryPath + "/../js/common/modules";
#endif

  // .............................................................................
  // use absolute paths
  // .............................................................................

#else

#ifdef _PKGDATADIR_
    _actionPathJS = string(_PKGDATADIR_) + "/js/actions/system";
    _startupModulesJS = string(_PKGDATADIR_) + "/js/server/modules"
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  options["JAVASCRIPT Options:help-admin"]
    ("xjavascript.gc-interval", &_gcIntervalJS, "JavaScript garbage collection interval (each x requests)")
  ;

  options["DIRECTORY Options:help-admin"]
    ("xjavascript.action-directory", &_actionPathJS, "path to the JavaScript action directory")
    ("xjavascript.modules-path", &_startupModulesJS, "one or more directories separated by (semi-) colons")
    ("xjavascript.startup-directory", &_startupPathJS, "path to the directory containing alternate JavaScript startup scripts")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::prepare () {
  for (size_t i = 0;  i < _nrInstances;  ++i) {
    prepareV8Instance(i);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::beginShutdown () {
  _shutdown = 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
