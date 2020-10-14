/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNull, assertMatch, fail, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, bind parameters
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
var getModifyQueryResults = helper.getModifyQueryResults;
var assertQueryError = helper.assertQueryError;

var sanitizeStats = function (stats) {
  // remove these members from the stats because they don't matter
  // for the comparisons
  delete stats.scannedFull;
  delete stats.scannedIndex;
  delete stats.filtered;
  delete stats.executionTime;
  delete stats.httpRequests;
  delete stats.fullCount;
  delete stats.peakMemoryUsage;
  return stats;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlModifySuite () {
  var errors = internal.errors;
  var cn1 = "UnitTestsAhuacatlModify1";
  var cn2 = "UnitTestsAhuacatlModify2";
  var c1, c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1, {numberOfShards:5});
      c2 = db._create(cn2, {numberOfShards:5});

      c1.save({ _key: "foo", a: 1 });
      c2.save({ _key: "foo", b: 1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = null;
      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testDynamicOptions1 : function () {
      assertQueryError(errors.ERROR_QUERY_COMPILE_TIME_OPTIONS.code, "FOR d IN @@cn REMOVE d IN @@cn OPTIONS { foo: d }", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testDynamicOptions2 : function () {
      assertQueryError(errors.ERROR_QUERY_COMPILE_TIME_OPTIONS.code, "FOR d IN @@cn REMOVE d IN @@cn OPTIONS { foo: MERGE(d, { }) }", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testInvalidOptions : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR d IN @@cn REMOVE d IN @@cn OPTIONS 'foo'", { "@cn": cn1 });
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlRemoveSuite () {
  var errors = internal.errors;
  var cn1 = "UnitTestsAhuacatlRemove1";
  var cn2 = "UnitTestsAhuacatlRemove2";
  var c1;
  var c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var i;
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1, {numberOfShards:5});
      c2 = db._create(cn2, {numberOfShards:5});

      for (i = 0; i < 100; ++i) {
        c1.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      for (i = 0; i < 50; ++i) {
        c2.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = null;
      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothing : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      let query = "FOR d IN " + cn1 + " FILTER d.value1 < 0 REMOVE d IN " + cn1;
      var actual = getModifyQueryResults(query);

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));

      let rules = AQL_EXPLAIN(query).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothingBind : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      let query = "FOR d IN @@cn FILTER d.value1 < 0 REMOVE d IN @@cn";
      var actual = getModifyQueryResults(query, { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid1 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn REMOVE d.foobar IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid2 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_KEY_MISSING.code, "FOR d IN @@cn REMOVE { } IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid3 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "FOR d IN @@cn REMOVE 'foobar' IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveReturn : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR d IN @@cn REMOVE d IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      actual.json = actual.json.sort(function(l, r) {
        return l.value1 - r.value1;
      });

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc._key);
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore1 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 17 };
      let query = "FOR d IN @@cn REMOVE 'foo' IN @@cn OPTIONS { ignoreErrors: true }";
      var actual = getModifyQueryResults(query, { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertTrue(-1 !== rules.indexOf("restrict-to-single-shard"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore2 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 1 };
      var actual = getModifyQueryResults("FOR i IN 0..100 REMOVE CONCAT('test', i) IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll1 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE d IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll2 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE d._key IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll3 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE { _key: d._key } IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll4 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 0..99 REMOVE { _key: CONCAT('test', i) } IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll5 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE d INTO @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveHalf : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 0..99 FILTER i % 2 == 0 REMOVE { _key: CONCAT('test', i) } IN @@cn", { "@cn": cn1 });

      assertEqual(50, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testSingleRemoveNotFound : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "REMOVE 'foobar' IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testSingleRemove : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResults("REMOVE 'test0' IN @@cn", { "@cn": cn1 });

      assertEqual(99, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsNotFound : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(50, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin1 : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn1 FILTER d.value1 < 50 REMOVE { _key: d._key } IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin2 : function () {
      var expected = { writesExecuted: 48, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn1 FILTER d.value1 >= 2 && d.value1 < 50 REMOVE { _key: d._key } IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(2, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors1 : function () {
      var expected = { writesExecuted: 50, writesIgnored: 50 };
      var actual = getModifyQueryResults("FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2 OPTIONS { ignoreErrors: true }", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors2 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResults("FOR d IN @@cn1 REMOVE { _key: CONCAT('foo', d._key) } IN @@cn2 OPTIONS { ignoreErrors: true }", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(50, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveWaitForSync : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE d IN @@cn OPTIONS { waitForSync: true }", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveEdge : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge");

      for (var i = 0; i < 100; ++i) {
        edge.save("UnitTestsAhuacatlRemove1/foo" + i, "UnitTestsAhuacatlRemove2/bar", { what: i, _key: "test" + i });
      }
      var expected = { writesExecuted: 10, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 0..9 REMOVE CONCAT('test', i) IN @@cn", { "@cn": edge.name() });

      assertEqual(100, c1.count());
      assertEqual(90, edge.count());
      assertEqual(expected, sanitizeStats(actual));
      db._drop("UnitTestsAhuacatlEdge");
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlInsertSuite () {
  var errors = internal.errors;
  var cn1 = "UnitTestsAhuacatlInsert1";
  var cn2 = "UnitTestsAhuacatlInsert2";
  var cn3 = "UnitTestsAhuacatlInsert3";
  var c1;
  var c2;
  var c3;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var i;
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = db._create(cn1, {numberOfShards: 5});
      c2 = db._create(cn2, {numberOfShards: 5});
      c3 = db._create(cn3, {numberOfShards: 2, shardKeys: [ "a", "b" ] });

      for (i = 0; i < 100; ++i) {
        c1.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      for (i = 0; i < 50; ++i) {
        c2.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertDouble : function () {
      c1.truncate({ compact: false });
      c2.truncate({ compact: false });

      const query = `LET d1 = {name : 'foo'}
                     LET d2 = {name : 'bar'}
                     INSERT d1 IN @@c1
                     INSERT d2 IN @@c2
                     RETURN $NEW`;
      const bind = { "@c1" : cn1, "@c2" : cn2 };
      const options = { optimizer : {
                          rules : [ "+restrict-to-single-shard",
                                    "-optimize-cluster-single-document-operations",
                                    "-remove-unnecessary-remote-scatter"
                                  ]
                      } };
      db._query(query, bind, options);
      assertEqual(1, c1.count());
      assertEqual(1, c2.count());
    },

    testInsertTripleWithSub : function () {
      c1.truncate({ compact: false });
      c2.truncate({ compact: false });

      c3.drop();
      c3 = db._createEdgeCollection(cn3);

      const query = `WITH @@usersCollection, @@emailsCollection, @@userToEmailEdges
                     LET user = FIRST(INSERT @user IN @@usersCollection RETURN NEW)
                     INSERT @email IN @@emailsCollection
                     INSERT @edge IN @@userToEmailEdges
                     RETURN user`;
      const bind = { "@usersCollection" : cn1,
                     "@emailsCollection" : cn2,
                     "@userToEmailEdges" : cn3,
                     "user" : { "name" : "ulf" },
                     "email" : { "addr" : "ulf@ulf.de"},
                     "edge" : { "_from" : "usersCollection/abc" , "_to" : "emailsCollection/def"}
                   };
      const options = {optimizer : { rules : ["+all"] } };

      db._query(query, bind, options);
      assertEqual(1, c1.count());
      assertEqual(1, c2.count());
      assertEqual(1, c3.count());
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertNothing : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 INSERT { foxx: true } IN " + cn1);

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertNothingBind : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 INSERT { foxx: true } IN @@cn", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertInvalid1 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn INSERT d.foobar IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertInvalid2 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn INSERT [ ] IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertInvalid3 : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn INSERT 'foo' IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertInvalid4 : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn INSERT { _key: 'foo' } IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueConstraint1 : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn INSERT d IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueConstraint2 : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR i IN 0..100 INSERT { _key: CONCAT('test', i) } IN @@cn", { "@cn": cn2 });
      assertEqual(50, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    // This is disabled for the cluster because we do not yet have
    // full atomic transactions.
    //testInsertUniqueConstraint3 : function () {
    //  assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR i IN 0..50 INSERT { _key: 'foo' } IN @@cn", { "@cn": cn1 });
    //  assertEqual(100, c1.count());
    //},

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturn : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      // key is specified
      var actual = AQL_EXECUTE("FOR d IN 0..99 INSERT { _key: CONCAT('foo', d), bar: d } IN @@cn LET result = NEW RETURN result", { "@cn": cn1 });

      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      actual.json = actual.json.sort(function(l, r) {
        return l.bar - r.bar;
      });

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("foo" + i);
        assertEqual("foo" + i, doc._key);
        assertEqual(i, doc.bar);

        var doc2 = actual.json[i];
        assertEqual("foo" + i, doc2._key);
        assertEqual(i, doc2.bar);
        assertTrue(doc2.hasOwnProperty("_rev"));
        assertTrue(doc2.hasOwnProperty("_id"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturnNoKey : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      // key is specified
      var actual = AQL_EXECUTE("FOR d IN 0..99 INSERT { bar: d } IN @@cn LET result = NEW RETURN result", { "@cn": cn1 });

      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      actual.json = actual.json.sort(function(l, r) {
        return l.bar - r.bar;
      });

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertMatch(/^\d+$/, doc._key);
        assertEqual(i, doc.bar);
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));

        var doc2 = c1.document(doc._key);
        assertEqual(doc._key, doc2._key);
        assertEqual(doc._rev, doc2._rev);
        assertEqual(doc._id, doc2._id);
        assertEqual(i, doc2.bar);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturnShardKeys : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      // key is specified
      var actual = AQL_EXECUTE("FOR d IN 0..99 INSERT { a: d, b: d + 1, bar: d } IN @@cn LET result = NEW RETURN result", { "@cn": cn3 });

      assertEqual(expected, sanitizeStats(actual.stats));

      actual.json = actual.json.sort(function(l, r) {
        return l.bar - r.bar;
      });

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertMatch(/^\d+$/, doc._key);
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertEqual(i, doc.bar);
        assertEqual(i, doc.a);
        assertEqual(i + 1, doc.b);

        var doc2 = c3.document(doc._key);
        assertEqual(doc._key, doc2._key);
        assertEqual(doc._rev, doc2._rev);
        assertEqual(doc._id, doc2._id);
        assertEqual(i, doc2.bar);
        assertEqual(i, doc2.a);
        assertEqual(i + 1, doc2.b);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertKey1 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      // key is specified
      var actual = getModifyQueryResults("FOR d IN 0..99 INSERT { _key: CONCAT('foo', d), bar: d } IN @@cn", { "@cn": cn1 });

      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("foo" + i);
        assertEqual("foo" + i, doc._key);
        assertEqual(i, doc.bar);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertKey2 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      // no key is specified, must be created automatically
      var actual = getModifyQueryResults("FOR d IN 0..99 INSERT { bar: d } IN @@cn", { "@cn": cn1 });

      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual));

      var docs = db._query("FOR doc IN @@cn FILTER doc.bar >= 0 SORT doc.bar RETURN doc", { "@cn": cn1 }).toArray();

      for (var i = 0; i < 100; ++i) {
        var doc = docs[i];
        assertMatch(/^\d+$/, doc._key);
        assertEqual(i, doc.bar);

        var doc2 = c1.document(doc._key);
        assertEqual(doc._key, doc2._key);
        assertEqual(doc._rev, doc2._rev);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertKey3 : function () {
      // key is specified, but sharding is by different attributes
      assertQueryError(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, "FOR i IN 0..50 INSERT { _key: 'foo' } IN @@cn", { "@cn": cn3 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertKey4 : function () {
      // key is specified, but sharding is by different attributes
      assertQueryError(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, "FOR i IN 0..50 INSERT { _key: 'foo', a: i, b: i } IN @@cn", { "@cn": cn3 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertKey5 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      // no key is specified, must be created automatically
      var actual = getModifyQueryResults("FOR d IN 0..99 INSERT { bar: d, a: d, b: d } IN @@cn", { "@cn": cn3 });

      assertEqual(100, c3.count());
      assertEqual(expected, sanitizeStats(actual));

      var docs = db._query("FOR doc IN @@cn FILTER doc.bar >= 0 SORT doc.bar RETURN doc", { "@cn" : cn3 }).toArray();

      for (var i = 0; i < 100; ++i) {
        var doc = docs[i];
        assertMatch(/^\d+$/, doc._key);
        assertEqual(i, doc.bar);
        assertEqual(i, doc.a);
        assertEqual(i, doc.b);

        var doc2 = c3.document(doc._key);
        assertEqual(doc._key, doc2._key);
        assertEqual(doc._rev, doc2._rev);
        assertEqual(doc.a, doc2.a);
        assertEqual(doc.b, doc2.b);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertKey6 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      // no key is specified, must be created automatically
      var actual = getModifyQueryResults("FOR d IN 0..99 INSERT { bar: d, a: d } IN @@cn", { "@cn": cn3 });

      assertEqual(100, c3.count());
      assertEqual(expected, sanitizeStats(actual));

      var docs = db._query("FOR doc IN @@cn FILTER doc.bar >= 0 SORT doc.bar RETURN doc", { "@cn" : cn3 }).toArray();

      for (var i = 0; i < 100; ++i) {
        var doc = docs[i];
        assertMatch(/^\d+$/, doc._key);
        assertEqual(i, doc.bar);
        assertEqual(i, doc.a);

        var doc2 = c3.document(doc._key);
        assertEqual(doc._key, doc2._key);
        assertEqual(doc._rev, doc2._rev);
        assertEqual(doc.a, doc2.a);
        assertEqual(doc.b, doc2.b);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore1 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResults("FOR d IN @@cn INSERT d IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore2 : function () {
      var expected = { writesExecuted: 1, writesIgnored: 50 };
      var actual = getModifyQueryResults("FOR i IN 50..100 INSERT { _key: CONCAT('test', i) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(101, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore3 : function () {
      var expected = { writesExecuted: 51, writesIgnored: 50 };
      var actual = getModifyQueryResults("FOR i IN 0..100 INSERT { _key: CONCAT('test', i) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn2 });

      assertEqual(101, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore4 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResults("FOR i IN 0..99 INSERT { _key: CONCAT('test', i) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore5 : function () {
      var expected = { writesExecuted: 50, writesIgnored: 50 };
      var actual = getModifyQueryResults("FOR i IN 0..99 INSERT { _key: CONCAT('test', i) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn2 });

      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEmpty : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn INSERT { } IN @@cn", { "@cn": cn1 });

      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertCopy : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 50..99 INSERT { _key: CONCAT('test', i) } IN @@cn", { "@cn": cn2 });

      assertEqual(100, c1.count());
      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testSingleInsert : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResults("INSERT { value: 'foobar', _key: 'test' } IN @@cn", { "@cn": cn1 });

      assertEqual(101, c1.count());
      assertEqual("foobar", c1.document("test").value);
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertWaitForSync : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 1..50 INSERT { value: i } INTO @@cn OPTIONS { waitForSync: true }", { "@cn": cn2 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdgeInvalid : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge");

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 INSERT { } INTO @@cn", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdgeNoFrom : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge");

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 INSERT { _to: CONCAT('UnitTestsAhuacatlInsert1/', i) } INTO @@cn", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdgeNoTo : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge");

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 INSERT { _from: CONCAT('UnitTestsAhuacatlInsert1/', i) } INTO @@cn", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdge : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge");

      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 1..50 INSERT { _key: CONCAT('test', i), _from: CONCAT('UnitTestsAhuacatlInsert1/', i), _to: CONCAT('UnitTestsAhuacatlInsert2/', i), value: [ i ], sub: { foo: 'bar' } } INTO @@cn", { "@cn": edge.name() });

      assertEqual(expected, sanitizeStats(actual));
      assertEqual(50, edge.count());

      for (var i = 1; i <= 50; ++i) {
        var doc = edge.document("test" + i);
        assertEqual("UnitTestsAhuacatlInsert1/" + i, doc._from);
        assertEqual("UnitTestsAhuacatlInsert2/" + i, doc._to);
        assertEqual([ i ], doc.value);
        assertEqual({ foo: "bar" }, doc.sub);
      }
      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdgeReturn : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge");

      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR i IN 0..49 INSERT { _key: CONCAT('test', i), _from: CONCAT('UnitTestsAhuacatlInsert1/', i), _to: CONCAT('UnitTestsAhuacatlInsert2/', i), value: [ i ], sub: { foo: 'bar' } } INTO @@cn LET result = NEW RETURN result", { "@cn": edge.name() });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(50, edge.count());

      actual.json = actual.json.sort(function(l, r) {
        return l.value[0] - r.value[0];
      });

      for (var i = 0; i < 50; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc._key);
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertEqual("UnitTestsAhuacatlInsert1/" + i, doc._from);
        assertEqual("UnitTestsAhuacatlInsert2/" + i, doc._to);
        assertEqual([ i ], doc.value);
        assertEqual({ foo: "bar" }, doc.sub);

        var doc2 = edge.document("test" + i);
        assertEqual(doc._rev, doc2._rev);
        assertEqual(doc._id, doc2._id);
        assertEqual(doc._from, doc2._from);
        assertEqual(doc._to, doc2._to);
        assertEqual([ i ], doc2.value);
        assertEqual({ foo: "bar" }, doc2.sub);
      }

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertOverwrite : function () {
      c1.truncate({ compact: false });
      assertEqual(0, c1.count());

      var rv1 = db._query(" INSERT { _key: '123', name: 'ulf' } IN @@cn OPTIONS { overwrite: false } RETURN NEW", { "@cn": cn1 });
      assertEqual(1, c1.count());
      var doc1 = rv1.toArray()[0];
      assertEqual(doc1._key, '123');
      assertEqual(doc1.name, 'ulf');

      var rv2 = db._query(" INSERT { _key: '123', name: 'ulfine' } IN @@cn OPTIONS { overwriteMode: 'replace' } RETURN {old: OLD, new: NEW}", { "@cn": cn1 });
      assertEqual(1, c1.count());
      var doc2 = rv2.toArray()[0];
      assertEqual(doc2.new._key, '123');
      assertEqual(doc2.new.name, 'ulfine');
      assertEqual(doc2.old._rev, doc1._rev);
      assertEqual(doc2.old._key, doc1._key);
      assertEqual(doc2.old.name, doc1.name);

      var rv3 = db._query(`
          LET x = (
            FOR a IN 3..5
              INSERT { _key: CONCAT('12',a), name: a }
            IN @@cn
              OPTIONS { overwrite: true }
              RETURN {old: OLD, new: NEW}
          )

          FOR d IN x SORT d.new._key
            RETURN d
        `, { "@cn": cn1 });

      var resultArray3 = rv3.toArray();
      assertEqual(3, c1.count());

      var doc3a = resultArray3[0];
      var doc3b = resultArray3[1];
      var doc3c = resultArray3[2];

      assertEqual(doc3a.old._rev, doc2.new._rev);
      assertEqual(doc3a.old._key, doc2.new._key);
      assertEqual(doc3a.old.name, "ulfine");
      assertEqual(doc3a.new.name, 3);
      assertEqual(doc3b.old, null);
      assertEqual(doc3b.new.name, 4);
      assertEqual(doc3c.old, null);
      assertEqual(doc3c.new.name, 5);

    },

    testInsertOverwriteUpsert : function () {
      c1.truncate({ compact: false });
      assertEqual(0, c1.count());

      var rv1 = db._query(" INSERT { _key: '123', name: 'ulf', drinks : { hard : 'korn' } } IN @@cn OPTIONS { overwrite: false } RETURN NEW", { "@cn": cn1 });
      assertEqual(1, c1.count());
      var doc1 = rv1.toArray()[0];
      assertEqual(doc1._key, '123');
      assertEqual(doc1.name, 'ulf');

      var rv2 = db._query(`
        INSERT { _key: '123'
               , partner: 'ulfine'
               , drinks : { soft : 'korn' }
               } IN @@cn OPTIONS { overwriteMode : 'update'
                                 , mergeObjects: false
                                 }
          RETURN {old: OLD, new: NEW}`
      , { "@cn": cn1 });
      assertEqual(1, c1.count());
      var doc2 = rv2.toArray()[0];
      assertEqual(doc2.new._key, '123');
      assertEqual(doc2.new.name, 'ulf');
      assertEqual(doc2.new.partner, 'ulfine');
      assertEqual(doc2.new.drinks, { 'soft' : 'korn'});
      assertEqual(doc2.old._rev, doc1._rev);
      assertEqual(doc2.old._key, doc1._key);
      assertEqual(doc2.old.name, doc1.name);
    },

  }; // end insert tests
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlUpdateSuite () {
  var errors = internal.errors;
  var cn1 = "UnitTestsAhuacatlUpdate1";
  var cn2 = "UnitTestsAhuacatlUpdate2";
  var cn3 = "UnitTestsAhuacatlUpdate3";
  var cn4 = "UnitTestsAhuacatlUpdate4";
  var c1, c2, c3, c4;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var i;
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      db._drop(cn4);
      c1 = db._create(cn1, {numberOfShards: 5});
      c2 = db._create(cn2, {numberOfShards: 5});
      c3 = db._create(cn3, {numberOfShards: 1});
      c4 = db._create(cn4, {numberOfShards: 1});

      let docs = [];
      for (i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      c1.insert(docs);
      docs = [];
      for (i = 0; i < 50; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      c2.insert(docs);
      docs = [];
      for (i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      c3.insert(docs);
      docs = [];
      for (i = 0; i < 50; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      c4.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      db._drop(cn4);
      c1 = null;
      c2 = null;
      c3 = null;
      c4 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateNothing : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 UPDATE { foxx: true } IN " + cn1);

      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateNothingBind : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 UPDATE { foxx: true } IN @@cn", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateInvalidType1 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn UPDATE d.foobar IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateInvalidType2 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn UPDATE [ ] IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateInvalidType : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn UPDATE 'foo' IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateInvalidKey : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "FOR d IN @@cn UPDATE { _key: 'foo' } IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateUniqueConstraint1 : function () {
      try {
        c1.ensureUniqueConstraint("value1");
        fail();
      }
      catch (e) {
        assertEqual(errors.ERROR_CLUSTER_UNSUPPORTED.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateUniqueConstraint2 : function () {
      c3.ensureUniqueConstraint("value1");
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn UPDATE d._key WITH { value1: 1 } IN @@cn", { "@cn": cn3 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateUniqueConstraint3 : function () {
      c3.ensureUniqueConstraint("value3", { sparse: true });
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn UPDATE d._key WITH { value3: 1 } IN @@cn", { "@cn": cn3 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateIgnore1 : function () {
      c3.ensureUniqueConstraint("value3", { sparse: true });
      var expected = { writesExecuted: 1, writesIgnored: 99 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d WITH { value3: 1 } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn3 });

      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateIgnore2 : function () {
      c3.ensureUniqueConstraint("value1");
      var expected = { writesExecuted: 0, writesIgnored: 51 };
      var actual = getModifyQueryResults("FOR i IN 50..100 UPDATE { _key: CONCAT('test', i), value1: 1 } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn3 });

      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateEmpty1 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE { _key: d._key } IN @@cn", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual));
      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateEmpty2 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d IN @@cn", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual));
      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update and return
////////////////////////////////////////////////////////////////////////////////

    testUpdateAndReturn : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const query = `
        FOR d IN @@cn
          UPDATE d WITH { updated: true } IN @@cn
          FILTER NEW._key == "test0"
          RETURN NEW`;

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));

      const actual = getModifyQueryResults(query, { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual));
      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
        assertTrue(doc.updated);
      }

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testSingleUpdateNotFound : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "UPDATE { _key: 'foobar' } WITH { value1: 1 } IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testSingleUpdate : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResults("UPDATE { value: 'foobar', _key: 'test17' } IN @@cn", { "@cn": cn1 });

      assertEqual("foobar", c1.document("test17").value);
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateOldValue : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE { _key: d._key, value1: d.value2, value2: d.value1, value3: d.value1 + 5 } IN @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual("test" + i, doc.value1);
        assertEqual(i, doc.value2);
        assertEqual(i + 5, doc.value3);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWaitForSync : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 1..50 UPDATE { _key: CONCAT('test', i) } INTO @@cn OPTIONS { waitForSync: true }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullDefault : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertNull(doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertNull(doc.value9);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullTrue : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn OPTIONS { keepNull: true }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertNull(doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertNull(doc.value9);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullFalse : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn OPTIONS { keepNull: false }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertFalse(doc.hasOwnProperty("value9"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateFilter : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 % 2 == 0 UPDATE d._key WITH { value2: 100 } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        if (i % 2 === 0) {
          assertEqual(100, doc.value2);
        }
        else {
          assertEqual("test" + i, doc.value2);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateUpdate : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { counter: HAS(d, 'counter') ? d.counter + 1 : 1 } INTO @@cn", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual));
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(5, doc.counter);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateAfterInsert : function () {
      AQL_EXECUTE("FOR i IN 0..99 INSERT { _key: CONCAT('sometest', i), value: i, wantToFind: true } IN @@cn", { "@cn": cn1 });

      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d.wantToFind UPDATE d._key WITH { value2: d.value % 2 == 0 ? d.value : d.value + 1 } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("sometest" + i);
        assertTrue(doc.wantToFind);
        assertEqual(i, doc.value);
        assertEqual(i % 2 === 0 ? i : i + 1, doc.value2);
        assertEqual(i, doc.value);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnOld : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR d IN @@cn UPDATE d WITH { value3: d.value1 + 5 } IN @@cn LET previous = OLD RETURN previous", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      actual.json = actual.json.sort(function(l, r) {
        return l.value1 - r.value1;
      });

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc._key);
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
        assertFalse(doc.hasOwnProperty("value3"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR d IN @@cn UPDATE d WITH { value3: d.value1 + 5 } IN @@cn LET now = NEW RETURN now", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      actual.json = actual.json.sort(function(l, r) {
        return l.value1 - r.value1;
      });

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc._key);
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual(i + 5, doc.value3);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace1 : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN @@cn REPLACE d._key WITH { value4: 12 } INTO @@cn";
      var actual = getModifyQueryResults(query, { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertFalse(doc.hasOwnProperty("value3"));
        assertEqual(12, doc.value4);
      }

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace2 : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN @@cn REPLACE { _key: d._key, value4: 13 } INTO @@cn";
      var actual = getModifyQueryResults(query, { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertFalse(doc.hasOwnProperty("value3"));
        assertEqual(13, doc.value4);
      }

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceReplace : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN @@cn REPLACE d._key WITH { value1: d.value1 + 1 } INTO @@cn";
      for (i = 0; i < 5; ++i) {
        var actual = getModifyQueryResults(query, { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual));
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i + 5, doc.value1);
        assertFalse(doc.hasOwnProperty("value2"));
      }

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceReturnOld : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN @@cn REPLACE d WITH { value3: d.value1 + 5 } IN @@cn LET previous = OLD RETURN previous";
      var actual = AQL_EXECUTE(query, { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      actual.json = actual.json.sort(function(l, r) {
        return l.value1 - r.value1;
      });

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc._key);
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
        assertFalse(doc.hasOwnProperty("value3"));
      }

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceReturnNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN @@cn REPLACE d WITH { value3: d.value1 + 5 } IN @@cn LET now = NEW RETURN now";
      var actual = AQL_EXECUTE(query, { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      actual.json = actual.json.sort(function(l, r) {
        return l.value3 - r.value3;
      });

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc._key);
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertEqual(i + 5, doc.value3);
      }

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceAfterInsert : function () {
      AQL_EXECUTE("FOR i IN 0..99 INSERT { _key: CONCAT('sometest', i), value: i, wantToFind: true } IN @@cn", { "@cn" : cn1 });

      var expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN @@cn FILTER d.wantToFind REPLACE d._key WITH { value2: d.value % 2 == 0 ? d.value : d.value + 1 } INTO @@cn";
      var actual = getModifyQueryResults(query, { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("sometest" + i);
        assertEqual(i % 2 === 0 ? i : i + 1, doc.value2);
        assertFalse(doc.hasOwnProperty("value"));
        assertFalse(doc.hasOwnProperty("wantToFind"));
      }

      let rules = AQL_EXPLAIN(query, { "@cn": cn1 }).plan.rules;
      assertEqual(-1, rules.indexOf("restrict-to-single-shard"));
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlModifySuite);
jsunity.run(ahuacatlRemoveSuite);
jsunity.run(ahuacatlInsertSuite);
jsunity.run(ahuacatlUpdateSuite);

return jsunity.done();
