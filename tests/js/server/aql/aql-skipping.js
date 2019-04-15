/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, simple queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Tobias Goedderz, Heiko Kernbach
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
// var errors = require("internal").errors;
var internal = require("internal");
var helper = require("@arangodb/aql-helper");
var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlSkippingTestsuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

    testDefaultSkipOffset: function () {
      var query = "FOR i in 1..100 let p = i+2 limit 90, 10 return p";
      var bindParams = {};
      var queryOptions = {};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json, [ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ]);
    },

    testDefaultSkipOffsetWithFullCount: function () {
      var query = "FOR i in 1..100 let p = i+2 limit 90, 10 return p";
      var bindParams = {};
      var queryOptions = {fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json, [ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ]);
      assertEqual(result.stats.fullCount, 100);
    },

    testPassSkipOffset: function () {
      var query = "FOR i in 1..100 let p = i+2 limit 90, 10 return p";
      var bindParams = {};
      var queryOptions = {optimizer: {"rules": ["-move-calculations-down"]}};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json, [ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ]);
    },

    testPassSkipOffsetWithFullCount: function () {
      var query = "FOR i in 1..100 let p = i+2 limit 90, 10 return p";
      var bindParams = {};
      var queryOptions = {fullCount: true, optimizer: {"rules": ["-move-calculations-down"]}};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json, [ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ]);
      assertEqual(result.stats.fullCount, 100);
    },

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlSkippingTestsuite);

return jsunity.done();

