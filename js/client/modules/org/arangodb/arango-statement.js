/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoStatement
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("org/arangodb/arangosh");

var ArangoStatement = require("org/arangodb/arango-statement-common").ArangoStatement;
var ArangoQueryCursor = require("org/arangodb/arango-query-cursor").ArangoQueryCursor;

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoStatement
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the statement
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.toString = function () {  
  return arangosh.getIdString(this, "ArangoStatement");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the help for ArangoStatement
////////////////////////////////////////////////////////////////////////////////

var helpArangoStatement = arangosh.createHelpHeadline("ArangoStatement help") +
  'ArangoStatement constructor:                                        ' + "\n" +
  ' > st = new ArangoStatement(db, { "query" : "for ..." });           ' + "\n" +
  ' > st = db._createStatement({ "query" : "for ..." });               ' + "\n" +
  'Functions:                                                          ' + "\n" +
  '  bind(<key>, <value>);          bind single variable               ' + "\n" +
  '  bind(<values>);                bind multiple variables            ' + "\n" +
  '  setBatchSize(<max>);           set max. number of results         ' + "\n" +
  '                                 to be transferred per roundtrip    ' + "\n" +
  '  setCount(<value>);             set count flag (return number of   ' + "\n" +
  '                                 results in "count" attribute)      ' + "\n" +
  '  getBatchSize();                return max. number of results      ' + "\n" +
  '                                 to be transferred per roundtrip    ' + "\n" +
  '  getCount();                    return count flag (return number of' + "\n" +
  '                                 results in "count" attribute)      ' + "\n" +
  '  getQuery();                    return query string                ' + "\n" +
  '  execute();                     execute query and return cursor    ' + "\n" +
  '  _help();                       this help                          ' + "\n" +
  'Attributes:                                                         ' + "\n" +
  '  _database                      database object                    ' + "\n" +
  'Example:                                                            ' + "\n" +
  ' > st = db._createStatement({ "query" : "for c in coll filter       ' + "\n" +
  '                              c.x == @a && c.y == @b return c" });  ' + "\n" +
  ' > st.bind("a", "hello");                                           ' + "\n" +
  ' > st.bind("b", "world");                                           ' + "\n" +
  ' > c = st.execute();                                                ' + "\n" +
  ' > print(c.elements());                                             ';

ArangoStatement.prototype._help = function () {
  internal.print(helpArangoStatement);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.parse = function () {
  var body = {
    "query" : this._query
  };

  var requestResult = this._database._connection.POST(
    "/_api/query",
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  var result = { "bindVars" : requestResult.bindVars, "collections" : requestResult.collections };
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief explain a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.explain = function () {
  var body = {
    "query" : this._query
  };

  var requestResult = this._database._connection.POST(
    "/_api/explain",
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult.plan;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the query
///
/// Invoking execute() will transfer the query and all bind parameters to the
/// server. It will return a cursor with the query results in case of success.
/// In case of an error, the error will be printed
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.execute = function () {
  var body = {
    "query" : this._query,
    "count" : this._doCount,
    "bindVars" : this._bindVars
  };

  if (this._batchSize) {
    body.batchSize = this._batchSize;
  }

  var requestResult = this._database._connection.POST(
    "/_api/cursor",
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return new ArangoQueryCursor(this._database, requestResult);
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
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
