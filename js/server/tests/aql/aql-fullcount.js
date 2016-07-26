/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, AQL_EXECUTE */

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

function optimizerFullcountTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      c.save({ values: [ "foo", "bar" ]});
      c.save({ values: [ "bar" ]});
      c.save({ values: [ "baz" ]});
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test no fullcount
////////////////////////////////////////////////////////////////////////////////

    testNoFullcount : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 LIMIT 10 RETURN doc", null, { }); 

      assertUndefined(result.stats.fullCount);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testWithFullcount : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 LIMIT 10 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(2, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testWithFullcountUsingLimit : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 LIMIT 1 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(1, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testWithFullCountSingleLevel : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection FILTER 'bar' IN doc.values LIMIT 10 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(2, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testWithFullCountSingleLevelUsingLimit : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection FILTER 'bar' IN doc.values LIMIT 1 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(1, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testHigherLimit : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LIMIT 100 RETURN doc", null, { fullCount: true }); 

      assertEqual(3, result.stats.fullCount);
      assertEqual(3, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testHigherLimit2 : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LIMIT 1, 100 RETURN doc", null, { fullCount: true }); 

      assertEqual(3, result.stats.fullCount);
      assertEqual(2, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testHigherLimit3 : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LIMIT 2, 100 RETURN doc", null, { fullCount: true }); 

      assertEqual(3, result.stats.fullCount);
      assertEqual(1, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testHigherLimit4 : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LIMIT 3, 100 RETURN doc", null, { fullCount: true }); 

      assertEqual(3, result.stats.fullCount);
      assertEqual(0, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testHigherLimit5 : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LIMIT 10000, 100 RETURN doc", null, { fullCount: true }); 

      assertEqual(3, result.stats.fullCount);
      assertEqual(0, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testHigherLimit6 : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection FILTER 'bar' IN doc.values LIMIT 1, 10 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(1, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testHigherLimit7 : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection FILTER 'bar' IN doc.values LIMIT 10, 10 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(0, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with fullcount
////////////////////////////////////////////////////////////////////////////////

    testHigherLimit8 : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection FILTER 'bar' IN doc.values LIMIT 1000, 1 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(0, result.json.length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerFullcountTestSuite);

return jsunity.done();

