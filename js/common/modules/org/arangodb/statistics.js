/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, eqeq: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var cluster = require("org/arangodb/cluster");

// -----------------------------------------------------------------------------
// --SECTION--                                  module "org/arangodb/statistics"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster id or undefined for standalone
////////////////////////////////////////////////////////////////////////////////

var clusterId;

if (cluster.isCluster()) {
  clusterId = ArangoServerState.id();
}
  
// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the user authentication data
////////////////////////////////////////////////////////////////////////////////
  
exports.historian = function (param) {
  "use strict";

  try {
    var result = {};

    result.time = internal.time();
    result.system = internal.processStatistics();
    result.client = internal.clientStatistics();
    result.http = internal.httpStatistics();
    result.server = internal.serverStatistics();

    if (clusterId !== undefined) {
      result.clusterId = clusterId;
    }

    internal.db._statistics.save(result);
  }
  catch (err) {
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:
