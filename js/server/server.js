'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief server initialization
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// / Copyright 2011-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief server start
// /
// / Note that all the initialization has been done. E. g. "upgrade-database.js"
// / has been executed.
// //////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require('internal');

  // check if --server.rest-server is disabled
  // in this case we do not (and should not) initialize and start Foxx
  var options = internal.options();
  var restServer = true;
  if (options.hasOwnProperty("server.rest-server")) {
   restServer = options["server.rest-server"];
  }

  // autoload all modules
  internal.loadStartup('server/bootstrap/autoload.js').startup();

  // reload routing information
  if (restServer) {
    internal.loadStartup('server/bootstrap/routing.js').startup();
  }

  // This script is also used by agents. Coords use a different script.
  // Make sure we only run these commands in single-server mode.
  if (internal.threadNumber === 0) {
    if (global.ArangoServerState.role() === 'SINGLE') {
      if (restServer) {
        // startup the foxx manager once
        require('@arangodb/foxx/manager')._startup();
      }

      // start the queue manager once
      try {
        require('@arangodb/foxx/queues/manager').run();
      } catch (err) {
        require("console").warn("unable to start Foxx queues manager: " + String(err));
        // continue with the startup!
      }
    }

    // check available versions
    if (internal.quiet !== true) {
      require('@arangodb').checkAvailableVersions();
    }
  }

  return true;
}());
