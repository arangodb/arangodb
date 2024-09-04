/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail */

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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////
let IM = global.instanceManager;

function optimizerPlansTestSuite () {
  var c;

  return {
    setUp : function () {
      IM.debugClearFailAt();
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 150; ++i) {
        c.save({ _key: "test" + i, value: i });
      }
    },

    tearDown : function () {
      IM.debugClearFailAt();
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test OOM in createPlans
////////////////////////////////////////////////////////////////////////////////

    testCreatePlansOom : function () {
      IM.debugSetFailAt("Optimizer::createPlansOom");
      try {
        db._query("FOR i IN 1..10 FILTER i == 1 || i == 2 RETURN i");
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerPlansTestSuite);
return jsunity.done();

