module.define("org/arangodb", function(exports, module) {
/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript base module
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var common = require("org/arangodb-common");

var key;

for (key in common) {
  if (common.hasOwnProperty(key)) {
    exports[key] = common[key];
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Arango
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoCollection"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoConnection"
////////////////////////////////////////////////////////////////////////////////

exports.ArangoConnection = internal.ArangoConnection;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoDatabase"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoDatabase = require("org/arangodb/arango-database").ArangoDatabase;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoStatement"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoStatement = require("org/arangodb/arango-statement").ArangoStatement;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoQueryCursor"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoQueryCursor = require("org/arangodb/arango-query-cursor").ArangoQueryCursor;

////////////////////////////////////////////////////////////////////////////////
/// @brief the global "db" and "arango" object
////////////////////////////////////////////////////////////////////////////////

if (typeof internal.arango !== 'undefined') {
  try {
    exports.arango = internal.arango;
    exports.db = new exports.ArangoDatabase(internal.arango);
    internal.db = exports.db; // TODO remove
  }
  catch (err) {
    internal.print("cannot connect to server: " + String(err));
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
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
});
