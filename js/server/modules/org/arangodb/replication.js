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

var logger  = { };
var applier = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the replication logger - this is a no-op in ArangoDB 2.2 and
/// higher
////////////////////////////////////////////////////////////////////////////////

logger.start = function () {
  'use strict';

  // the logger in ArangoDB 2.2 is now the WAL...
  // so the logger cannot be started but is always running
  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication logger - this is a no-op in ArangoDB 2.2 and
/// higher
////////////////////////////////////////////////////////////////////////////////

logger.stop = function () {
  'use strict';
 
  // the logger in ArangoDB 2.2 is now the WAL...
  // so the logger cannot be stopped
  return false; 
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication logger state
////////////////////////////////////////////////////////////////////////////////

logger.state = function () {  
  'use strict';

  return internal.getStateReplicationLogger();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the configuration of the replication logger
////////////////////////////////////////////////////////////////////////////////
 
logger.properties = function () {
  'use strict';

  return internal.configureReplicationLogger();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the replication applier
////////////////////////////////////////////////////////////////////////////////

applier.start = function (initialTick) {
  'use strict';

  if (initialTick === undefined) {
    return internal.startReplicationApplier();
  }

  return internal.startReplicationApplier(initialTick);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication applier
////////////////////////////////////////////////////////////////////////////////
  
applier.stop = function () {
  'use strict';

  return internal.stopReplicationApplier();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication applier state
////////////////////////////////////////////////////////////////////////////////
  
applier.state = function () {
  'use strict';

  return internal.getStateReplicationApplier();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the applier and "forget" all configuration
////////////////////////////////////////////////////////////////////////////////
  
applier.forget = function () {
  'use strict';

  return internal.forgetStateReplicationApplier();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the configuration of the replication applier
////////////////////////////////////////////////////////////////////////////////
 
applier.properties = function (config) {
  'use strict';

  if (config === undefined) {
    return internal.configureReplicationApplier();
  }
  
  return internal.configureReplicationApplier(config);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   other functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a one-time synchronisation with a remote endpoint
////////////////////////////////////////////////////////////////////////////////

var sync = function (config) {
  'use strict';

  return internal.synchroniseReplication(config);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server's id
////////////////////////////////////////////////////////////////////////////////

var serverId = function () {
  'use strict';

  return internal.serverId();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    module exports
// -----------------------------------------------------------------------------

exports.logger   = logger; 
exports.applier  = applier; 
exports.sync     = sync; 
exports.serverId = serverId; 

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:

