/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Michael Hackstein
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var internal = require("internal");
let IM = global.instanceManager;

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
      db._query(query, null, { optimizer: { rules: rulesToExclude } });
      fail();
    } catch (err) {
      assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, query);
    }
  };

  return {

    setUpAll: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
    },

    setUp: function () {
      IM.debugClearFailAt();
    },

    tearDownAll: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = null;
    },

    tearDown: function() {
      IM.debugClearFailAt();
    },

    testThatQueryIsntStuckAtShutdown: function() {
      IM.debugSetFailAt("Query::finalize_before_done");
      assertFailingQuery(`INSERT {Hallo:12} INTO ${cn}`);
    },
  };
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (IM.debugCanUseFailAt()) {
  jsunity.run(queryFailureSuite);
}

return jsunity.done();

