/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function sortTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 8 });

      for (var i = 0; i < 11111; ++i) {
        c.save({ _key: "test" + i, value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test without index
////////////////////////////////////////////////////////////////////////////////

    testSortNoIndex : function () {
      var result = AQL_EXECUTE("FOR doc IN " + c.name() + " SORT doc.value RETURN doc.value").json;
      assertEqual(11111, result.length);

      var last = -1;
      for (var i = 0; i < result.length; ++i) {
        assertTrue(result[i] > last);
        last = result[i];
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with index
////////////////////////////////////////////////////////////////////////////////

    testSortSkiplist : function () {
      c.ensureIndex({ type: "skiplist", fields: [ "value" ]});
      var result = AQL_EXECUTE("FOR doc IN " + c.name() + " SORT doc.value RETURN doc.value").json;
      assertEqual(11111, result.length);

      var last = -1;
      for (var i = 0; i < result.length; ++i) {
        assertTrue(result[i] > last);
        last = result[i];
      }
    }

  };
}

jsunity.run(sortTestSuite);

return jsunity.done();

