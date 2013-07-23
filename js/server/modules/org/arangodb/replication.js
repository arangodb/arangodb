/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Replication management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal"); 

// -----------------------------------------------------------------------------
// --SECTION--                                 module "org/arangodb/replication"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

var logger  = { };
var applier = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the replication logger
////////////////////////////////////////////////////////////////////////////////

logger.start = function () {
  return internal.startReplicationLogger();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication logger
////////////////////////////////////////////////////////////////////////////////

logger.stop = function () {
  return internal.stopReplicationLogger();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication logger state
////////////////////////////////////////////////////////////////////////////////

logger.state = function () {  
  return internal.getStateReplicationLogger();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the replication applier
////////////////////////////////////////////////////////////////////////////////

applier.start = function (forceFullSynchronisation) {
  return internal.startReplicationApplier(forceFullSynchronisation || false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication applier
////////////////////////////////////////////////////////////////////////////////
  
applier.stop = function () {
  return internal.stopReplicationApplier();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication applier state
////////////////////////////////////////////////////////////////////////////////
  
applier.state = function () {
  return internal.getStateReplicationApplier();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the configuration of the replication applier
////////////////////////////////////////////////////////////////////////////////
 
applier.properties = function (config) {
  if (config === undefined) {
    return internal.configureReplicationApplier();
  }
  
  return internal.configureReplicationApplier(config);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    module exports
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////
  
exports.logger  = logger; 
exports.applier = applier; 

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:

