/*global require, exports */
'use strict';

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

var logger  = { };
var applier = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication logger state
////////////////////////////////////////////////////////////////////////////////

logger.state = function () {

  var db = internal.db;

  var requestResult = db._connection.GET("/_api/replication/logger-state");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the replication applier
////////////////////////////////////////////////////////////////////////////////

applier.start = function (initialTick) {

  var db = internal.db;
  var append = "";

  if (initialTick !== undefined) {
    append = "?from=" + encodeURIComponent(initialTick);
  }

  var requestResult = db._connection.PUT("/_api/replication/applier-start" + append, "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication applier
////////////////////////////////////////////////////////////////////////////////

applier.stop = applier.shutdown = function () {

  var db = internal.db;

  var requestResult = db._connection.PUT("/_api/replication/applier-stop", "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication applier state
////////////////////////////////////////////////////////////////////////////////

applier.state = function () {

  var db = internal.db;

  var requestResult = db._connection.GET("/_api/replication/applier-state");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier state and "forget" all state
////////////////////////////////////////////////////////////////////////////////

applier.forget = function () {

  var db = internal.db;

  var requestResult = db._connection.DELETE("/_api/replication/applier-state");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief configures the replication applier
////////////////////////////////////////////////////////////////////////////////

applier.properties = function (config) {

  var db = internal.db;

  var requestResult;
  if (config === undefined) {
    requestResult = db._connection.GET("/_api/replication/applier-config");
  }
  else {
    requestResult = db._connection.PUT("/_api/replication/applier-config",
      JSON.stringify(config));
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   other functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a one-time synchronisation with a remote endpoint
////////////////////////////////////////////////////////////////////////////////

var sync = function (config) {

  var db = internal.db;

  var body = JSON.stringify(config || { });
  var requestResult = db._connection.PUT("/_api/replication/sync", body);

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches a server's id
////////////////////////////////////////////////////////////////////////////////

var serverId = function () {

  var db = internal.db;

  var requestResult = db._connection.GET("/_api/replication/server-id");

  arangosh.checkRequestResult(requestResult);

  return requestResult.serverId;
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

