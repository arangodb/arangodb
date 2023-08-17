/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure scenarios for both single and cluster
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
/// Copyright 2012-2021 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Michael Hackstein
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function queryFailureSuite () {
  'use strict';
  var cn = "UnitTestsQueryFailures";
  var c;
        
  var assertFailingQuery = function (query, rulesToExclude) {
    if (!rulesToExclude) {
      rulesToExclude = [];
    }
    try {
      AQL_EXECUTE(query, null, { optimizer: { rules: rulesToExclude } });
      fail();
    } catch (err) {
      assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, query);
    }
  };

  return {

    setUpAll: function () {
      internal.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
    },

    setUp: function () {
      internal.debugClearFailAt();
    },

    tearDownAll: function () {
      internal.debugClearFailAt();
      db._drop(cn);
      c = null;
    },

    tearDown: function() {
      internal.debugClearFailAt();
    },

    testThatQueryIsntStuckAtShutdown: function() {
      internal.debugSetFailAt("Query::finalize_before_done");
      assertFailingQuery(`INSERT {Hallo:12} INTO ${cn}`);
    },
  };
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (internal.debugCanUseFailAt()) {
  jsunity.run(queryFailureSuite);
}

return jsunity.done();

