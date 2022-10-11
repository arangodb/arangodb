/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertFalse, AQL_EXECUTE */

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

const jsunity = require("jsunity");
const db = require("@arangodb").db;

function optimizerFullcountTestSuite () {
  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      c.save({ values: [ "foo", "bar" ]});
      c.save({ values: [ "bar" ]});
      c.save({ values: [ "baz" ]});
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test no fullcount
////////////////////////////////////////////////////////////////////////////////

    testNoFullcount : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 LIMIT 10 RETURN doc", null, { }); 

      assertFalse(result.stats.hasOwnProperty('fullCount'));
      assertEqual(2, result.json.length);
    },
    
    testFullcount : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 LIMIT 10 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(2, result.json.length);
    },

    testNoFullcountUsingLimitOnlyInSubquery : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 RETURN doc", null, { }); 

      assertFalse(result.stats.hasOwnProperty('fullCount'));
      assertEqual(2, result.json.length);
    },
    
    testFullcountUsingLimitOnlyInSubquery : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(2, result.json.length);
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
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' RETURN true) FILTER LENGTH(found) > 0 LIMIT 1 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(1, result.json.length);
    },
    
    testWithFullcountUsingLimitInSubquery : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(2, result.json.length);
    },
    
    testWithFullcountUsingLimitInSubqueryAndMain : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 1 RETURN true) FILTER LENGTH(found) > 0 LIMIT 1 RETURN doc", null, { fullCount: true }); 

      assertEqual(2, result.stats.fullCount);
      assertEqual(1, result.json.length);
    },
    
    testWithFullcountUsingLimitInLaterSubquery : function () {
      var result = AQL_EXECUTE("FOR doc IN UnitTestsCollection LIMIT 1 LET values = doc.values LET found = (FOR value IN values FILTER value == 'bar' || value == 'foo' LIMIT 0 RETURN true) FILTER LENGTH(found) >= 0 RETURN doc", null, { fullCount: true }); 

      assertEqual(3, result.stats.fullCount);
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
    },

    testJoin1 : function () {
      let result = AQL_EXECUTE("FOR doc1 IN UnitTestsCollection LIMIT 1 FOR doc2 IN UnitTestsCollection RETURN 1", null, { fullCount: true });

      assertEqual(3, result.stats.fullCount);
      assertEqual(3, result.json.length);
    },

    testJoin2 : function () {
      let result = AQL_EXECUTE("FOR doc1 IN UnitTestsCollection LIMIT 1 FOR doc2 IN UnitTestsCollection FILTER doc1._id == doc2._id RETURN 1", null, { fullCount: true });

      assertEqual(3, result.stats.fullCount);
      assertEqual(1, result.json.length);
    }

  };
}

function optimizerFullcountQueryTestSuite () {
  let c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");
      c.ensureIndex({ type: "geo", fields: ["location"] }); 
      c.insert([
        { location: [2, 0] },
        { location: [3, 0] },
        { location: [4, 0] },
        { location: [6, 0] }
      ]); 
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

    testQueryRegression : function () {
      let result = db._query({ query: "FOR e IN " + c.name() + " SORT DISTANCE(e.location[0], e.location[1], 0, 0) LIMIT 2, 2 RETURN e", options: { fullCount: true }}).toArray();
      assertEqual(2, result.length);
      assertEqual([4, 0], result[0].location);
      assertEqual([6, 0], result[1].location);
    },
    
  };
}

jsunity.run(optimizerFullcountTestSuite);
jsunity.run(optimizerFullcountQueryTestSuite);

return jsunity.done();

