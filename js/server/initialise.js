'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief basic initialisation
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                load files required during startup
// -----------------------------------------------------------------------------

(function () {
  var startupPath = global.STARTUP_PATH;
  var load = global.SYS_LOAD;

  if (startupPath === "") {
    startupPath = ".";
  }

  load(startupPath + "/common/bootstrap/modules.js");
  load(startupPath + "/common/bootstrap/module-internal.js");
  load(startupPath + "/common/bootstrap/module-fs.js");
  load(startupPath + "/common/bootstrap/module-console.js");
  load(startupPath + "/common/bootstrap/errors.js");
  load(startupPath + "/common/bootstrap/monkeypatches.js");
  load(startupPath + "/server/bootstrap/module-internal.js");
}());

// common globals
global.Buffer = require("buffer").Buffer;
global.process = require("process");
global.setInterval = function () {};
global.clearInterval = function () {};
global.setTimeout = function () {};
global.clearTimeout = function () {};

// extend prototypes for internally defined classes
require("org/arangodb");

// load the actions from the actions directory
require("org/arangodb/actions").startup();

// initialize AQL
require("org/arangodb/aql");

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
