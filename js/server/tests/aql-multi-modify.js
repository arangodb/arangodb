/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertNull, assertNotNull, assertTrue, 
  assertNotEqual, assertUndefined, fail, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for multi-modify operations
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

// TODO: test usage of OLD/NEW etc. in queries
var internal = require("internal");
var db = require("org/arangodb").db;
var jsunity = require("jsunity");
var helper = require("org/arangodb/aql-helper");
var getModifyQueryResults = helper.getModifyQueryResults;
var getModifyQueryResultsRaw = helper.getModifyQueryResultsRaw;
var isEqual = helper.isEqual;
var assertQueryError = helper.assertQueryError;
var errors = internal.errors;

var sanitizeStats = function (stats) {
  // remove these members from the stats because they don't matter
  // for the comparisons
  delete stats.scannedFull;
  delete stats.scannedIndex;
  delete stats.filtered;
  return stats;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlMultiModifySuite () {
  var errors = internal.errors;
  var cn1 = "UnitTestsAhuacatlModify1";
  var cn2 = "UnitTestsAhuacatlModify2";
  var cn3 = "UnitTestsAhuacatlModify3";
  var c1, c2, c3;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = db._create(cn1);
      c2 = db._create(cn2);
      c3 = db._create(cn3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = null;
      c2 = null;
      c3 = null;
    },

    testMultiInsertSameCollection : function () {
      var q = "INSERT { value: 1 } INTO @@cn INSERT { value: 2 } INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },

    testMultiInsertOtherCollection : function () {
      var q = "INSERT { value: 1 } INTO @@cn1 INSERT { value: 2 } INTO @@cn2";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2": cn2 });
      assertEqual([ ], actual.json);
      assertEqual(2, actual.stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(1, c1.any().value);
      assertEqual(1, c2.count());
      assertEqual(2, c2.any().value);
    },

    testMultiInsertLoopSameCollection : function () {
      var q = "FOR i IN 1..10 INSERT { value: i } INTO @@cn INSERT { value: i + 1 } INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testMultiInsertLoopOtherCollection : function () {
      var q = "FOR i IN 1..10 INSERT { value: i } INTO @@cn1 INSERT { value: i + 1 } INTO @@cn2";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2": cn2 });
      assertEqual([ ], actual.json);
      assertEqual(20, actual.stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(10, c2.count());
    },

    testMultiInsertLoopSubquery : function () {
      var q = "FOR i IN 1..10 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(10, actual.json.length);
      assertEqual(55, actual.stats.writesExecuted);
      assertEqual(55, c1.count());
    },

    testMultiInsertLoopSubquerySameCollection : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(10, actual.json.length);
      assertEqual(20, actual.stats.writesExecuted);
      assertEqual(30, c1.count());
    },

    testMultiInsertLoopSubqueryOtherCollection : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn1 LET sub = (FOR j IN 1..2 INSERT { value: j } INTO @@cn2) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2" : cn2 });
      assertEqual(10, actual.json.length);
      assertEqual(20, actual.stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(20, c2.count());
    },

    testMultiInsertLoopSubquerySameCollectionIndependent : function () {
//      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub1 = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) LET sub2 = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
//      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
//      assertEqual(10, actual.json.length);
//      assertEqual(40, actual.stats.writesExecuted);
//      assertEqual(50, c1.count());
    },

    testMultiRemoveSameCollection : function () {
      var q = "INSERT { value: 1 } INTO @@cn LET doc = NEW REMOVE doc._key INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },

    testMultiRemoveOtherCollection : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "INSERT { value: 1 } INTO @@cn1 REMOVE { _key: 'test9' } INTO @@cn2";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2": cn2 });
      assertEqual([ ], actual.json);
      assertEqual(2, actual.stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(1, c1.any().value);
      assertEqual(9, c2.count());
      assertFalse(c2.exists('test9'));
      assertTrue(c2.exists('test10'));
    },

    testMultiRemoveLoopSameCollection : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN 1..10 INSERT { value: i } INTO @@cn REMOVE { _key: CONCAT('test', i) } INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testMultiRemoveLoopOtherCollection : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "FOR i IN 1..10 INSERT { value: i } INTO @@cn1 REMOVE { _key: CONCAT('test', i) } INTO @@cn2";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2": cn2 });
      assertEqual([ ], actual.json);
      assertEqual(20, actual.stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

    testMultiRemoveLoopSubquery : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN 1..10 LET sub = (REMOVE { _key: CONCAT('test', i) } INTO @@cn) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(10, actual.json.length);
      assertEqual(10, actual.stats.writesExecuted);
      assertEqual(0, c1.count());
    },

    testMultiRemoveLoopSubquerySameCollection : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub = (REMOVE { _key: i._key } INTO @@cn) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(10, actual.json.length);
      assertEqual(10, actual.stats.writesExecuted);
      assertEqual(0, c1.count());
    },

    testMultiRemoveLoopSubqueryOtherCollection : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      AQL_EXECUTE("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "FOR i IN @@cn1 LET sub = (REMOVE { _key: i._key } INTO @@cn2) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2" : cn2 });
      assertEqual(10, actual.json.length);
      assertEqual(10, actual.stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

    testMultiRemoveLoopSubquerySameCollectionIndependent : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub1 = (INSERT { _key: CONCAT('test', i) } INTO @@cn) LET sub2 = (REMOVE { _key: CONCAT('test', i) } INTO @@cn) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
//      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
//      assertEqual(10, actual.json.length);
//      assertEqual(40, actual.stats.writesExecuted);
//      assertEqual(50, c1.count());
    },





////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

/*
    testRemoveInSubqueryNoResult : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var actual = AQL_EXECUTE("FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn) RETURN f", { "@cn" : cn1 }).json; 
      var expected = [ ];
      for (var i = 1; i <= 10; ++i) {
        expected.push([ ]);
      }
      require("internal").print(actual);
      assertEqual(expected, actual);
    },
*/
    testRemoveInSubqueryReturnKeys : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var actual = AQL_EXECUTE("FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn RETURN OLD.value) RETURN f", { "@cn" : cn1 }).json; 
      var expected = [ ];
      for (var i = 1; i <= 10; ++i) {
        expected.push([ i ]);
      }
      assertEqual(expected, actual);
    },

    testRemoveInSubqueryReturnKeysDoc : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var actual = AQL_EXECUTE("FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn RETURN OLD) RETURN f[0].value", { "@cn" : cn1 }).json; 
      var expected = [ ];
      for (var i = 1; i <= 10; ++i) {
        expected.push(i);
      }
      assertEqual(expected, actual);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlMultiModifySuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
