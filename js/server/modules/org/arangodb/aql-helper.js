/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, assertTrue, assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief aql test helper functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
var arangodb = require("org/arangodb");

// -----------------------------------------------------------------------------
// --SECTION--                                         AQL test helper functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalise a single row result 
////////////////////////////////////////////////////////////////////////////////

function normalizeRow (row, recursive) {
  if (row !== null && 
      typeof row === 'object' && 
      ! Array.isArray(row)) {
    var keys = Object.keys(row);

    keys.sort();

    var i, n = keys.length, out = { };
    for (i = 0; i < n; ++i) {
      var key = keys[i];

      if (key[0] !== '_') {
        out[key] = row[key];
      }
    }

    return out;
  }

  if (recursive && Array.isArray(row)) {
    row = row.map(normalizeRow);
  }

  return row;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query explanation
////////////////////////////////////////////////////////////////////////////////

function getQueryExplanation (query, bindVars) {
  var result = internal.AQL_EXPLAIN(query, bindVars);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query
////////////////////////////////////////////////////////////////////////////////

function getRawQueryResults (query, bindVars) {
  var queryResult = internal.AQL_QUERY(query, bindVars, true, 3000);

  if (queryResult instanceof arangodb.ArangoCursor) {
    queryResult = queryResult.toArray();
  }

  return queryResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query in a normalised way
////////////////////////////////////////////////////////////////////////////////

function getQueryResults (query, bindVars, recursive) {
  var result = getRawQueryResults(query, bindVars);

  if (Array.isArray(result)) {
    result = result.map(function (row) {
      return normalizeRow(row, recursive);
    });
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert a specific error code when running a query
////////////////////////////////////////////////////////////////////////////////

function assertQueryError (errorCode, query, bindVars) {
  try {
    getQueryResults(query, bindVars);
    fail();
  }
  catch (e) {
    assertTrue(e.errorNum !== undefined, "unexpected error format");
    assertEqual(errorCode, e.errorNum, "unexpected error code");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    module exports
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.assertQueryError    = assertQueryError;
exports.getQueryExplanation = getQueryExplanation;
exports.getRawQueryResults  = getRawQueryResults;
exports.getQueryResults     = getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
