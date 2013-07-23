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
var arangosh = require("org/arangodb/arangosh");

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
  'use strict';

  var db = internal.db;

  var requestResult = db._connection.PUT("_api/replication/log-start", "");
  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication logger
////////////////////////////////////////////////////////////////////////////////
  
logger.stop = function () {
  'use strict';

  var db = internal.db;

  var requestResult = db._connection.PUT("_api/replication/log-stop", "");
  arangosh.checkRequestResult(requestResult);
  
  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication logger state
////////////////////////////////////////////////////////////////////////////////
  
logger.state = function () {
  'use strict';

  var db = internal.db;

  var requestResult = db._connection.GET("_api/replication/log-state");
  arangosh.checkRequestResult(requestResult);
  
  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the replication applier
////////////////////////////////////////////////////////////////////////////////
  
applier.start = function (forceFullSynchronisation) {
  'use strict';

  var db = internal.db;
  var append = (forceFullSynchronisation ? "?fullSync=true" : "");

  var requestResult = db._connection.PUT("_api/replication/apply-start" + append, "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication applier
////////////////////////////////////////////////////////////////////////////////
  
applier.stop = function () {
  'use strict';

  var db = internal.db;

  var requestResult = db._connection.PUT("_api/replication/apply-stop", "");
  arangosh.checkRequestResult(requestResult);
  
  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication applier state
////////////////////////////////////////////////////////////////////////////////
  
applier.state = function () {
  'use strict';

  var db = internal.db;

  var requestResult = db._connection.GET("_api/replication/apply-state");
  arangosh.checkRequestResult(requestResult);
  
  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief configures the replication applier
////////////////////////////////////////////////////////////////////////////////
  
applier.properties = function (config) {
  'use strict';

  var db = internal.db;

  var requestResult;
  if (config === undefined) {
    requestResult = db._connection.GET("_api/replication/apply-config");
  }
  else {
    requestResult = db._connection.PUT("_api/replication/apply-config",
      JSON.stringify(config));
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
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

