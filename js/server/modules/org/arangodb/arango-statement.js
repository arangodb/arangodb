/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoStatement
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var ArangoStatement = require("org/arangodb/arango-statement-common").ArangoStatement;
var GeneralArrayCursor = require("org/arangodb/simple-query-common").GeneralArrayCursor;
var QUERY = require("internal").AQL_QUERY;
var PARSE = require("internal").AQL_PARSE;
var EXPLAIN = require("internal").AQL_EXPLAIN;

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoStatement
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoStatement
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.parse = function () {
  var result = PARSE(this._query); 

  return { "bindVars" : result.parameters, "collections" : result.collections };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief explain a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.explain = function () {
  return EXPLAIN(this._query, this._bindVars); 
};

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the query
///
/// This will return a cursor with the query results in case of success.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.execute = function () {
  var result = QUERY(this._query, 
                     this._bindVars, 
                     this._doCount !== undefined ? this._doCount : false, 
                     null, 
                     true);  
  return new GeneralArrayCursor(result, 0, null);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoStatement
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.ArangoStatement = ArangoStatement; 

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
