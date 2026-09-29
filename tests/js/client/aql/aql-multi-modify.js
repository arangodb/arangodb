/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue */

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
// //////////////////////////////////////////////////////////////////////////////

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
      var actual = db._query(q, { "@cn": cn1, "@e": cn3 });
      let res = actual.toArray();
      assertEqual([ ], res);
      assertEqual(2, actual.getExtra().stats.writesExecuted);
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
      var actual = db._query(q, { "@cn": cn1, "@e": cn3 });
      let res = actual.toArray();

      assertEqual([ ], res);
      assertEqual(2009, actual.getExtra().stats.writesExecuted);
      assertEqual(1, c1.count());
      assertTrue(c1.exists("1"));
      assertEqual(2009, c3.count());

      var nodes = db._createStatement({query: q, bindVars: { "@cn": cn1, "@e": cn3 }}).explain().plan.nodes, found = false;
      nodes.forEach(function(node) {
        if (node.type === 'RemoveNode') {
          found = true;
        }
      });
      assertTrue(found);
    },
    
    testWithDeclarationAndModification : function () {
      var q = "WITH @@cn RETURN (INSERT {} INTO @@cn)";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual([ [ ] ], res);
      assertEqual(1, actual.getExtra().stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(0, c2.count());
    },
    
    testWithDeclarationsAndSingleModificationMultipleCollections : function () {
      var q = "WITH @@cn1, @@cn2 RETURN (INSERT {} INTO @@cn1)";
      var actual = db._query(q, { "@cn1": cn1, "@cn2" : cn2 });
      let res = actual.toArray();
      assertEqual([ [ ] ], res);
      assertEqual(1, actual.getExtra().stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(0, c2.count());
    },
    
    testWithDeclarationsAndMultiModificationMultipleCollections : function () {
      var q = "WITH @@cn1, @@cn2 RETURN [(INSERT {} INTO @@cn1), (INSERT {} INTO @@cn2)]";
      var actual = db._query(q, { "@cn1": cn1, "@cn2" : cn2 });
      let res = actual.toArray();
      assertEqual([ [ [ ], [ ] ] ], res);
      assertEqual(2, actual.getExtra().stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(1, c2.count());
    },
    
    testWithDeclarationsAndModificationWriteRead : function () {
      var q = "WITH @@cn1, @@cn2 RETURN [(INSERT {} INTO @@cn1), (FOR doc IN @@cn2 RETURN doc)]";
      var actual = db._query(q, { "@cn1": cn1, "@cn2" : cn2 });
      let res = actual.toArray();
      assertEqual([ [ [ ], [ ] ] ], res);
      assertEqual(1, actual.getExtra().stats.writesExecuted);
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
      var actual = db._query({query: q, bindVars: { "@cn1": cn1, "@cn2": cn2 }});
      let res = actual.toArray();
      assertEqual([ ], res);
      assertEqual(2, actual.getExtra().stats.writesExecuted);
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
      var actual = db._query({query: q, bindVars: { "@cn1": cn1, "@cn2": cn2 }});
      let res = actual.toArray();
      assertEqual([ ], res);
      assertEqual(20, actual.getExtra().stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(10, c2.count());
    },

    testMultiInsertLoopSubquerySingle : function () {
      var q = "FOR i IN 1..1 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(1, res.length);
      assertEqual([ 1 ], res);
      assertEqual(1, c1.count());
    },
    
    testMultiInsertLoopSubquerySingleReturnInside : function () {
      var q = "FOR i IN 1..1 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn RETURN NEW.value) RETURN sub";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(1, res.length);
      assertEqual([ [ 1 ] ], res);
      assertEqual(1, c1.count());
    },

    testMultiInsertLoopSubquerySingleReturned : function () {
      var q = "FOR i IN 1..1 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN sub";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(1, res.length);
      assertEqual([ [ ] ], res);
      assertEqual(1, c1.count());
    },

    testMultiInsertLoopSubqueryTwo : function () {
      var q = "FOR i IN 1..2 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(2, res.length);
      assertEqual([ 1, 1 ], res);
      assertEqual(3, c1.count());
    },
    
    testMultiInsertLoopSubqueryTwoReturnInside : function () {
      var q = "FOR i IN 1..2 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn RETURN NEW._key) RETURN 1";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(2, res.length);
      assertEqual([ 1, 1 ], res);
      assertEqual(3, c1.count());
    },

    testMultiInsertLoopSubqueryTwoReturned : function () {
      var q = "FOR i IN 1..2 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN sub";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(2, res.length);
      assertEqual([ [ ], [ ] ], res);
      assertEqual(3, c1.count());
    },

    testMultiInsertLoopSubqueryMore : function () {
      var q = "FOR i IN 1..10 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(10, res.length);
      assertEqual(55, c1.count());
    },

    testMultiInsertLoopSubqueryReturned : function () {
      var q = "FOR i IN 1..2 LET sub = (FOR j IN 1..i INSERT { value: j } INTO @@cn) RETURN sub";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(2, res.length);
      assertEqual([ [ ], [ ] ], res);
      assertEqual(3, c1.count());
    },

    testMultiInsertLoopSubquerySameCollection : function () {
      db._query("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) RETURN 1";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual([ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ], res);
    },

    testMultiInsertLoopSubqueryOtherCollection : function () {
      db._query("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn1 LET sub = (FOR j IN 1..2 INSERT { value: j } INTO @@cn2) RETURN 1";
      var actual = db._query(q, { "@cn1": cn1, "@cn2" : cn2 });
      let res = actual.toArray();
      assertEqual(10, res.length);
      assertEqual(20, actual.getExtra().stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(20, c2.count());
    },
    
    testMultiInsertLoopSubqueryOtherCollectionReturnInside : function () {
      db._query("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn1 LET sub = (FOR j IN 1..2 INSERT { value: j } INTO @@cn2 RETURN NEW.value) RETURN sub";
      var actual = db._query(q, { "@cn1": cn1, "@cn2" : cn2 });
      let res = actual.toArray();
      assertEqual(10, res.length);
      assertEqual(20, actual.getExtra().stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(20, c2.count());
    },

    testMultiInsertLoopSubquerySameCollectionIndependent : function () {
      db._query("FOR i IN 1..10 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub1 = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) LET sub2 = (FOR j IN 1..2 INSERT { value: j } INTO @@cn) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testMultiRemoveSameCollection : function () {
      var q = "INSERT { value: 1 } INTO @@cn LET doc = NEW REMOVE doc._key INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, { "@cn": cn1 });
    },

    testMultiRemoveOtherCollection : function () {
      db._query("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "INSERT { value: 1 } INTO @@cn1 REMOVE { _key: 'test9' } INTO @@cn2";
      var actual = db._query({query: q, bindVars: { "@cn1": cn1, "@cn2": cn2 }});
      let res = actual.toArray();
      assertEqual([ ], res);
      assertEqual(2, actual.getExtra().stats.writesExecuted);
      assertEqual(1, c1.count());
      assertEqual(1, c1.any().value);
      assertEqual(9, c2.count());
      assertFalse(c2.exists('test9'));
      assertTrue(c2.exists('test10'));
    },

    testMultiRemoveLoopSameCollection : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN 1..2010 INSERT { value: i } INTO @@cn REMOVE { _key: CONCAT('test', i) } INTO @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testMultiRemoveLoopSameCollectionWithRead : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn1 INSERT { _key: doc._key } INTO @@cn2 REMOVE doc IN @@cn1";
      var actual = db._query({query: q, bindVars: { "@cn1": cn1, "@cn2": cn2 }});
      let res = actual.toArray();
      assertEqual([ ], res);
      assertEqual(4020, actual.getExtra().stats.writesExecuted);
      assertEqual(0, c1.count());
      assertEqual(2010, c2.count());

      var nodes = db._createStatement({query: q, bindVars: { "@cn1": cn1, "@cn2": cn2 }}).explain().plan.nodes, found = false;
      nodes.forEach(function(node) {
        if (node.type === 'RemoveNode') {
          found = true;
        }
      });
      assertTrue(found);
    },

    testMultiRemoveLoopOtherCollection : function () {
      db._query("FOR i IN 1..10 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "FOR i IN 1..10 INSERT { value: i } INTO @@cn1 REMOVE { _key: CONCAT('test', i) } INTO @@cn2";
      var actual = db._query({query: q, bindVars: { "@cn1": cn1, "@cn2": cn2 }});
      let res = actual.toArray();
      assertEqual([ ], res);
      assertEqual(20, actual.getExtra().stats.writesExecuted);
      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

    testMultiRemoveLoopSubquery : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN 1..2010 LET sub = (REMOVE { _key: CONCAT('test', i) } INTO @@cn) RETURN 1";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(2010, res.length);
      assertEqual(2010, actual.getExtra().stats.writesExecuted);
      assertEqual(0, c1.count());
    },
    
    testMultiRemoveLoopSubqueryReturnInside : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN 1..2010 LET sub = (REMOVE { _key: CONCAT('test', i) } INTO @@cn RETURN OLD._key) RETURN sub";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(2010, res.length);
      assertEqual(2010, actual.getExtra().stats.writesExecuted);
      assertEqual(0, c1.count());
    },

    testMultiRemoveLoopSubquerySameCollection : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub = (REMOVE { _key: i._key } INTO @@cn) RETURN 1";
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      assertEqual(Array(2010).fill(1), res);
    },

    testMultiRemoveLoopSubqueryOtherCollection : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "FOR i IN @@cn1 LET sub = (REMOVE { _key: i._key } INTO @@cn2) RETURN 1";
      var actual = db._query(q, { "@cn1": cn1, "@cn2" : cn2 });
      let res = actual.toArray();
      assertEqual(2010, res.length);
      assertEqual(2010, actual.getExtra().stats.writesExecuted);
      assertEqual(2010, c1.count());
      assertEqual(0, c2.count());
    },
    
    testMultiRemoveLoopSubqueryOtherCollectionReturnInside : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn2 });
      var q = "FOR i IN @@cn1 LET sub = (REMOVE { _key: i._key } INTO @@cn2 RETURN OLD._key) RETURN sub";
      var actual = db._query(q, { "@cn1": cn1, "@cn2" : cn2 });
      let res = actual.toArray();
      assertEqual(2010, res.length);
      assertEqual(2010, actual.getExtra().stats.writesExecuted);
      assertEqual(2010, c1.count());
      assertEqual(0, c2.count());
    },

    testMultiRemoveLoopSubquerySameCollectionIndependent : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub1 = (INSERT { _key: CONCAT('test', i) } INTO @@cn) LET sub2 = (REMOVE { _key: CONCAT('test', i) } INTO @@cn) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },
    
    testMultiRemoveLoopSubquerySameCollectionIndependentReturnsInside : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i) } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR i IN @@cn LET sub1 = (INSERT { _key: CONCAT('test', i) } INTO @@cn RETURN NEW._key) LET sub2 = (REMOVE { _key: CONCAT('test', i) } INTO @@cn RETURN OLD._key) RETURN 1";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },

    testRemoveInSubqueryNoResult : function () {
      db._query("FOR i IN 1..2010 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn) RETURN f"; 
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      let result = [];
      for (let i = 1; i <= 2010; ++i) {
        result.push([]);
      }
      assertEqual(result, res);
    },
    
    testRemoveInSubqueryNoResultReturnInside : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i), value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn RETURN OLD._key) RETURN f"; 
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      let result = [];
      for (let i = 1; i <= 2010; ++i) {
        result.push([ "test" + i ]);
      }
      assertEqual(result, res);
    },

    testRemoveInSubqueryReturnKeys : function () {
      db._query("FOR i IN 1..2010 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn RETURN OLD.value) RETURN f"; 
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      let result = [];
      for (let i = 1; i <= 2010; ++i) {
        result.push([i]);
      }
      assertEqual(result, res);
    },

    testRemoveInSubqueryReturnKeysDoc : function () {
      db._query("FOR i IN 1..2010 INSERT { value: i } INTO @@cn", { "@cn" : cn1 });
      var q = "FOR doc IN @@cn SORT doc.value LET f = (REMOVE doc IN @@cn RETURN OLD) RETURN f[0].value"; 
      var actual = db._query(q, { "@cn": cn1 });
      let res = actual.toArray();
      let result = [];
      for (let i = 1; i <= 2010; ++i) {
        result.push(i);
      }
      assertEqual(result, res);
    },
    
    testInsertRemove : function () {
      var q = "FOR i IN 1..2010 INSERT { value: i } INTO @@cn LET x = NEW REMOVE x._key IN @@cn";
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, q, {"@cn": cn1 });
    },
    
    testInsertRemove2 : function () {
      db._query("FOR i IN 1..2010 INSERT { _key: CONCAT('test', i), value: i } INTO @@cn", { "@cn" : cn1 });
      assertEqual(2010, c1.count());
      assertEqual(0, c2.count());
      const actual = db._query("FOR i IN @@cn1 INSERT { value: i } INTO @@cn2 LET x = i._key REMOVE x IN @@cn1", { "@cn1" : cn1, "@cn2": cn2 }).toArray();
      assertEqual([ ], actual);
      assertEqual(0, c1.count());
      assertEqual(2010, c2.count());
    },
    
    testMultiInsert : function () {
      var actual = db._query("FOR i IN 1..2010 INSERT { value: i * 3, _key: CONCAT('one-', i) } INTO @@cn1 LET one = NEW._key INSERT { value: i * 5, _key: CONCAT('two-', i) } INTO @@cn2 LET two = NEW._key RETURN [ one, two ]", { "@cn1" : cn1, "@cn2": cn2 }).toArray();
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
      db._query("FOR i IN 1..2000 INSERT { _key: CONCAT('test', i) } IN @@cn1 INSERT { _key: CONCAT('test', i) } IN @@cn2", { "@cn1" : cn1, "@cn2" : cn2 });
      assertEqual(2000, c1.count());
      assertEqual(2000, c2.count());
      
      db._query("FOR i IN 1..2000 REMOVE { _key: CONCAT('test', i) } IN @@cn1 REMOVE { _key: CONCAT('test', i) } IN @@cn2", { "@cn1" : cn1, "@cn2" : cn2 });
      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for the distribute input of several modification nodes
////////////////////////////////////////////////////////////////////////////////

function ahuacatlMultiModifyDistributeInputSuite () {
  var cn1 = "UnitTestsAhuacatlModifyShardedSource";
  var cn2 = "UnitTestsAhuacatlModifyShardedInsert";
  var cn3 = "UnitTestsAhuacatlModifyShardedRemove";
  var cn4 = "UnitTestsAhuacatlModifyShardedRemoveKeys";
  var all = [cn1, cn2, cn3, cn4];
  var numDocs = 100;
  var numRemoved = 3;

  return {

    setUp : function () {
      all.forEach(function (cn) {
        db._drop(cn);
      });
      all.forEach(function (cn) {
        db._create(cn, { numberOfShards: 3 });
      });

      var docs = [];
      for (var i = 0; i < numDocs; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      db[cn1].save(docs);
      // the documents to remove, and a separate collection driving the REMOVE,
      // so that the REMOVE is not restricted to a single shard
      db[cn3].save(docs.slice(0, numRemoved));
      db[cn4].save(docs.slice(0, numRemoved));
    },

    tearDown : function () {
      all.forEach(function (cn) {
        db._drop(cn);
      });
    },

    // An INSERT whose input is a calculation needs that input prepared on the
    // coordinator, so the new document carries the key it is routed by. The
    // REMOVE takes its input straight from a collection enumeration and needs
    // no such preparation - skipping it must not stop the INSERT from getting
    // its own.
    testInsertBeforeRemoveOfEnumeratedDocument : function () {
      if (!internal.isCluster()) {
        return;
      }
      var query = `FOR d IN ${cn1}
                     INSERT { value: d.value } INTO ${cn2}
                     FOR e IN ${cn4}
                       REMOVE e IN ${cn3} OPTIONS { ignoreErrors: true }`;

      var nodes = db._createStatement({ query }).explain().plan.nodes;
      var prepared = nodes.filter(function (n) {
        return n.type === "CalculationNode" &&
               JSON.stringify(n.expression).indexOf("MAKE_DISTRIBUTE_INPUT") !== -1;
      });
      assertEqual(1, prepared.length, JSON.stringify(nodes.map(function (n) {
        return n.type;
      })));

      db._query(query);
      assertEqual(numDocs, db[cn2].count());
      assertEqual(0, db[cn3].count());

      // Documents that were routed to the wrong shard are still found by a
      // full collection scan, but no longer by their key.
      var keys = db._query(`FOR d IN ${cn2} RETURN d._key`).toArray();
      assertEqual(numDocs, keys.length);
      var found = db._query(`FOR k IN @keys
                               FOR d IN ${cn2}
                                 FILTER d._key == k
                                 RETURN 1`, { keys }).toArray();
      assertEqual(numDocs, found.length);
    },

    // Same query with the two operations swapped. Here the REMOVE is seen last
    // and the INSERT keeps its prepared input either way.
    testRemoveOfEnumeratedDocumentBeforeInsert : function () {
      if (!internal.isCluster()) {
        return;
      }
      var query = `FOR e IN ${cn4}
                     REMOVE e IN ${cn3} OPTIONS { ignoreErrors: true }
                     FOR d IN ${cn1}
                       INSERT { value: d.value } INTO ${cn2}`;

      db._query(query);
      assertEqual(0, db[cn3].count());

      var keys = db._query(`FOR d IN ${cn2} RETURN d._key`).toArray();
      var found = db._query(`FOR k IN @keys
                               FOR d IN ${cn2}
                                 FILTER d._key == k
                                 RETURN 1`, { keys }).toArray();
      assertEqual(keys.length, found.length);
    }
  };
}

jsunity.run(ahuacatlMultiModifySuite);
jsunity.run(ahuacatlMultiModifyDistributeInputSuite);

return jsunity.done();
