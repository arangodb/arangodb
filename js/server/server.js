/*global require, ArangoAgency */

////////////////////////////////////////////////////////////////////////////////
/// @brief server initialisation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2011-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             server initialisation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief server start
///
/// Note that all the initialisation has been done. E. g. "upgrade-database.js"
/// has been executed.
////////////////////////////////////////////////////////////////////////////////

(function () {
  "use strict";
  var internal = require("internal");

  // in the cluster the kickstarter will call boostrap-role.js
  if (ArangoAgency.prefix() !== "") {
    return true;
  }

  // statistics can be turned off
  if (internal.enableStatistics && internal.threadNumber === 0) {
    require("org/arangodb/statistics").startup();
  }

  // load all foxxes
  if (internal.threadNumber === 0) {
    internal.loadStartup("server/bootstrap/foxxes.js").foxxes();
  }

  // autoload all modules
  internal.loadStartup("server/bootstrap/autoload.js").startup();

  // reload routing information
  internal.loadStartup("server/bootstrap/routing.js").startup();

  // start the queue manager once
  if (internal.threadNumber === 0) {
    require('org/arangodb/foxx/queues/manager').run();
  }

  return true;
}());


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
