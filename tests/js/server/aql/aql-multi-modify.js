/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, AQL_EXECUTE, AQL_EXPLAIN */

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

var internal = require("internal");
var db = require("@arangodb").db;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var assertQueryError = helper.assertQueryError;

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
      c3 = db._createEdgeCollection(cn3);
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
    
    testTraversalAndModification : function () {
      c1.insert({ _key: "1" });
      c1.insert({ _key: "2" });
      c1.insert({ _key: "3" });
      c1.insert({ _key: "4" });
      c3.insert(cn1 + "/1", cn1 + "/2", { });
      c3.insert(cn1 + "/2", cn1 + "/3", { });

      var q = "FOR v IN 1..99 OUTBOUND '" + cn1 + "/1' @@e REMOVE v._key IN @@cn";
      var actual = AQL_EXECUTE(q, { "@cn": cn1, "@e": cn3 });
      assertEqual([ ], actual.json);
      assertEqual(2, actual.stats.writesExecuted);
      assertEqual(2, c1.count());
      assertTrue(c1.exists("1"));
      assertTrue(c1.exists("4"));
      assertEqual(2, c3.count());
    },
    
    testTraversalAndModificationBig : function () {
      var i;
      for (i = 1; i <= 2010; ++i) {
        c1.insert({ _key: String(i) });
        if (i !== 2010) {
          c3.insert(cn1 + "/" + String(i), cn1 + "/" + String(i + 1), { });
        }
      }

      var q = "FOR v IN 1..2010 OUTBOUND '" + cn1 + "/1' @@e REMOVE v._key IN @@cn";
      var actual = AQL_EXECUTE(q, { "@cn": cn1, "@e": cn3 });

      assertEqual([ ], actual.json);
      assertEqual(2009, actual.stats.writesExecuted);
      assertEqual(1, c1.count());
      assertTrue(c1.exists("1"));
      assertEqual(2009, c3.count());

      // check that 'readCompleteInput' option is set
      var nodes = AQL_EXPLAIN(q, { "@cn": cn1, "@e": cn3 }).plan.nodes, found = false;
      nodes.forEach(function(node) {
        if (node.type === 'RemoveNode') {
          assertTrue(node.modificationFlags.readCompleteInput);
          found = true;
        }
      });
      assertTrue(found);
    },
    
    testWithDeclarationAndModification : function () {
      var q = "WITH @@cn RETURN (INSERT {} INTO @@cn)";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual([ [ ] ], actual.json);
      assertEqual(1, actual.stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(0, c2.count());
    },
    
    testWithDeclarationsAndSingleModificationMultipleCollections : function () {
      var q = "WITH @@cn1, @@cn2 RETURN (INSERT {} INTO @@cn1)";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2" : cn2 });
      assertEqual([ [ ] ], actual.json);
      assertEqual(1, actual.stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(0, c2.count());
    },
    
    testWithDeclarationsAndMultiModificationMultipleCollections : function () {
      var q = "WITH @@cn1, @@cn2 RETURN [(INSERT {} INTO @@cn1), (INSERT {} INTO @@cn2)]";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2" : cn2 });
      assertEqual([ [ [ ], [ ] ] ], actual.json);
      assertEqual(2, actual.stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(1, c2.count());
    },
    
    testWithDeclarationsAndModificationWriteRead : function () {
      var q = "WITH @@cn1, @@cn2 RETURN [(INSERT {} INTO @@cn1), (FOR doc IN @@cn2 RETURN doc)]";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2" : cn2 });
      assertEqual([ [ [ ], [ ] ] ], actual.json);
      assertEqual(1, actual.stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(0, c2.count());
    },
    
    testWithDeclarationSameCollectionWriteThenRead : function () {
      var q = "WITH @@cn1 RETURN [(INSERT {} INTO @@cn1), (FOR doc IN @@cn1 RETURN doc)]";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn1": cn1 });
    },
    
    testWithDeclarationAndModificationMultipleCollectionsThenRead : function () {
      var q = "WITH @@cn1 LET x = (INSERT {} INTO @@cn1) FOR doc IN @@cn1 RETURN doc";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn1": cn1 });
    },
    
    testTraversalAfterModification : function () {
      var q = "INSERT { _key: '1', foo: 'bar' } INTO @@cn FOR doc IN OUTBOUND 'v/1' @@e RETURN doc";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1, "@e": cn3 });
    },
    
    testInsertAfterModification : function () {
      var q = "INSERT { _key: '1', foo: 'bar' } INTO @@cn INSERT { _key: '2', foo: 'baz' } INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },
    
    testUpdateAfterModification : function () {
      var q = "INSERT { _key: '1', foo: 'bar' } INTO @@cn UPDATE '1' WITH { foo: 'baz' } INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },
    
    testReplaceAfterModification : function () {
      var q = "INSERT { _key: '1', foo: 'bar' } INTO @@cn REPLACE '1' WITH { foo: 'baz' } INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },
    
    testRemoveAfterModification : function () {
      var q = "INSERT { _key: '1', foo: 'bar' } INTO @@cn REMOVE '1' IN @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },
    
    testUpsertAfterModification : function () {
      var q = "INSERT { _key: '1', foo: 'bar' } INTO @@cn UPSERT { foo: 'bar' } INSERT { foo: 'bar' } UPDATE { foo: 'baz' } IN @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },
    
    testDocumentAfterModification : function () {
      var q = "INSERT { _key: '1', foo: 'bar' } INTO @@cn RETURN DOCUMENT(@@cn, '1')";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },
    
    testEdgeAfterModification : function () {
      var q = "INSERT { _from: @from, _to: @to } INTO @@cn RETURN DOCUMENT(@@cn, NEW._key)";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn3, from: cn2 + "/1", to: cn2 + "/2" });
    },
    
    testEdgesAfterModification : function () {
      var q = "INSERT { _from: @from, _to: @to } INTO @@cn FOR v, e IN OUTBOUND @from @@cn RETURN e";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn3, from: cn2 + "/1", to: cn2 + "/2" });
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

    testMultiInsertLoopSubquerySingle : function () {
      var q = "FOR i IN 1..1 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(1, actual.json.length);
      assertEqual([ 1 ], actual.json);
      assertEqual(1, c1.count());
    },
    
    testMultiInsertLoopSubquerySingleReturnInside : function () {
      var q = "FOR i IN 1..1 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn RETURN NEW.value) RETURN sub";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(1, actual.json.length);
      assertEqual([ [ 1 ] ], actual.json);
      assertEqual(1, c1.count());
    },

    testMultiInsertLoopSubquerySingleReturned : function () {
      var q = "FOR i IN 1..1 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN sub";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(1, actual.json.length);
      assertEqual([ [ ] ], actual.json);
      assertEqual(1, c1.count());
    },

    testMultiInsertLoopSubqueryTwo : function () {
      var q = "FOR i IN 1..2 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(2, actual.json.length);
      assertEqual([ 1, 1 ], actual.json);
      assertEqual(3, c1.count());
    },
    
    testMultiInsertLoopSubqueryTwoReturnInside : function () {
      var q = "FOR i IN 1..2 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn RETURN NEW._key) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(2, actual.json.length);
      assertEqual([ 1, 1 ], actual.json);
      assertEqual(3, c1.count());
    },

    testMultiInsertLoopSubqueryTwoReturned : function () {
      var q = "FOR i IN 1..2 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN sub";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(2, actual.json.length);
      assertEqual([ [ ], [ ] ], actual.json);
      assertEqual(3, c1.count());
    },

    testMultiInsertLoopSubqueryMore : function () {
      var q = "FOR i IN 1..10 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(10, actual.json.length);
      assertEqual(55, c1.count());
    },

    testMultiInsertLoopSubqueryReturned : function () {
      var q = "FOR i IN 1..2 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN sub";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(2, actual.json.length);
      assertEqual([ [ ], [ ] ], actual.json);
      assertEqual(3, c1.count());
    },

    testMultiInsertLoopSubquerySameCollection : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
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
    
    testMultiInsertLoopSubqueryOtherCollectionReturnInside : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn1 LET sub = (FOR j IN 1..2 INSERT { value: j } INTO @@cn2 RETURN NEW.value) RETURN sub";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2" : cn2 });
      assertEqual(10, actual.json.length);
      assertEqual(20, actual.stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(20, c2.count());
    },

    testMultiInsertLoopSubquerySameCollectionIndependent : function () {
      AQL_EXECUTE("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub1 = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) LET sub2 = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
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
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN 1..2010 INSERT { value: i } INTO @@cn REMOVE { _key: CONCAT('test', i) } INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testMultiRemoveLoopSameCollectionWithRead : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn1 INSERT { _key: doc._key } INTO @@cn2 REMOVE doc IN @@cn1";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2": cn2 });
      assertEqual([ ], actual.json);
      assertEqual(4020, actual.stats.writesExecuted);
      assertEqual(0, c1.count());
      assertEqual(2010, c2.count());

      // check that 'readCompleteInput' option is set
      var nodes = AQL_EXPLAIN(q, { "@cn1": cn1, "@cn2": cn2 }).plan.nodes, found = false;
      nodes.forEach(function(node) {
        if (node.type === 'RemoveNode') {
          assertFalse(node.modificationFlags.readCompleteInput);
          found = true;
        }
      });
      assertTrue(found);
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
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN 1..2010 LET sub = (REMOVE { _key: CONCAT('test', i) } INTO @@cn) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(2010, actual.json.length);
      assertEqual(2010, actual.stats.writesExecuted);
      assertEqual(0, c1.count());
    },
    
    testMultiRemoveLoopSubqueryReturnInside : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN 1..2010 LET sub = (REMOVE { _key: CONCAT('test', i) } INTO @@cn RETURN OLD._key) RETURN sub";
      var actual = AQL_EXECUTE(q, { "@cn": cn1 });
      assertEqual(2010, actual.json.length);
      assertEqual(2010, actual.stats.writesExecuted);
      assertEqual(0, c1.count());
    },

    testMultiRemoveLoopSubquerySameCollection : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub = (REMOVE { _key: i._key } INTO @@cn) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },

    testMultiRemoveLoopSubqueryOtherCollection : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "FOR i IN @@cn1 LET sub = (REMOVE { _key: i._key } INTO @@cn2) RETURN 1";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2" : cn2 });
      assertEqual(2010, actual.json.length);
      assertEqual(2010, actual.stats.writesExecuted);
      assertEqual(2010, c1.count());
      assertEqual(0, c2.count());
    },
    
    testMultiRemoveLoopSubqueryOtherCollectionReturnInside : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "FOR i IN @@cn1 LET sub = (REMOVE { _key: i._key } INTO @@cn2 RETURN OLD._key) RETURN sub";
      var actual = AQL_EXECUTE(q, { "@cn1": cn1, "@cn2" : cn2 });
      assertEqual(2010, actual.json.length);
      assertEqual(2010, actual.stats.writesExecuted);
      assertEqual(2010, c1.count());
      assertEqual(0, c2.count());
    },

    testMultiRemoveLoopSubquerySameCollectionIndependent : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub1 = (INSERT { _key: CONCAT('test', i) } INTO @@cn) LET sub2 = (REMOVE { _key: CONCAT('test', i) } INTO @@cn) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },
    
    testMultiRemoveLoopSubquerySameCollectionIndependentReturnsInside : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub1 = (INSERT { _key: CONCAT('test', i) } INTO @@cn RETURN NEW._key) LET sub2 = (REMOVE { _key: CONCAT('test', i) } INTO @@cn RETURN OLD._key) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testRemoveInSubqueryNoResult : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn) RETURN f"; 
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },
    
    testRemoveInSubqueryNoResultReturnInside : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn RETURN OLD._key) RETURN f"; 
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testRemoveInSubqueryReturnKeys : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn RETURN OLD.value) RETURN f"; 
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testRemoveInSubqueryReturnKeysDoc : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn RETURN OLD) RETURN f[0].value"; 
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },
    
    testInsertRemove : function () {
      var q = "FOR i IN 1..2010 INSERT { value: i } INTO @@cn LET x = NEW REMOVE x._key IN @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },
    
    testInsertRemove2 : function () {
      AQL_EXECUTE("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i), value: i } INTO @@cn", { "@cn" : cn1 });
      assertEqual(2010, c1.count());
      assertEqual(0, c2.count());
      const actual = AQL_EXECUTE("FOR i IN @@cn1 INSERT { value: i } INTO @@cn2 LET x = i._key REMOVE x IN @@cn1", { "@cn1" : cn1, "@cn2": cn2 }).json;
      assertEqual([ ], actual);
      assertEqual(0, c1.count());
      assertEqual(2010, c2.count());
    },
    
    testMultiInsert : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..2010 INSERT { value: i * 3, _key: CONCAT('one-', i) } INTO @@cn1 LET one = NEW._key INSERT { value: i * 5, _key: CONCAT('two-', i) } INTO @@cn2 LET two = NEW._key RETURN [ one, two ]", { "@cn1" : cn1, "@cn2": cn2 }).json;
      assertEqual(2010, actual.length);
      var i, seen = { };
      for (i = 0; i < actual.length; ++i) {
        assertTrue(typeof actual[i][0] === 'string');
        assertFalse(seen.hasOwnProperty(actual[i][0]));
        seen[actual[i][0]] = true;
        assertFalse(seen.hasOwnProperty(actual[i][1]));
        seen[actual[i][1]] = true;
      }
      assertEqual(2010, c1.count());
      assertEqual(2010, c2.count());
      for (i = 0; i < actual.length; ++i) {
        assertTrue(c1.document(actual[i][0]).value % 3 === 0);
        assertTrue(c2.document(actual[i][1]).value % 5 === 0);
      }
    },
    
    testMultiRemove : function () {
      c1.save([ { _key: "a" }, { _key:"b" }, { _key: "c" } ]);
      c3.save([ { _from: cn1 + "/a", _to: cn1 + "/b", _key: "1" }, { _from: cn1 + "/a", _to: cn1 + "/b", _key: "2" } ]);

      var toDelete = [ { v: "b", e: "1" }, { v: "c", e: "2" } ];
      db._query(`FOR x IN @toDelete REMOVE x.v IN ${cn1} REMOVE x.e IN ${cn3}`, { toDelete }).toArray();
      assertEqual(1, c1.toArray().length);
      assertEqual("a", c1.toArray()[0]._key);
      assertEqual([ ], c3.toArray());
    },

    testMultiRemove2 : function () {
      AQL_EXECUTE("FOR i IN 1..2000 INSERT { _key: CONCAT('test', i) } IN @@cn1 INSERT { _key: CONCAT('test', i) } IN @@cn2", { "@cn1" : cn1, "@cn2" : cn2 });
      assertEqual(2000, c1.count());
      assertEqual(2000, c2.count());
      
      AQL_EXECUTE("FOR i IN 1..2000 REMOVE { _key: CONCAT('test', i) } IN @@cn1 REMOVE { _key: CONCAT('test', i) } IN @@cn2", { "@cn1" : cn1, "@cn2" : cn2 });
      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlMultiModifySuite);

return jsunity.done();

