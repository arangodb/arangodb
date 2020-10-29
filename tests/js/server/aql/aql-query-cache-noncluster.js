/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse, assertMatch, AQL_EXECUTE, 
  AQL_QUERY_CACHE_PROPERTIES, AQL_QUERY_CACHE_INVALIDATE */

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

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var internal = require("internal");

function ahuacatlQueryCacheTestSuite () {
  var cacheProperties;
  var c1, c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      cacheProperties = AQL_QUERY_CACHE_PROPERTIES();
      AQL_QUERY_CACHE_INVALIDATE();

      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");

      c1 = db._create("UnitTestsAhuacatlQueryCache1");
      c2 = db._create("UnitTestsAhuacatlQueryCache2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");

      c1 = null;
      c2 = null;

      AQL_QUERY_CACHE_PROPERTIES(cacheProperties);
      AQL_QUERY_CACHE_INVALIDATE();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fullCount
////////////////////////////////////////////////////////////////////////////////

    testFullCount : function () {
      let query = "FOR i IN 1..10000 LIMIT 10, 7 RETURN i";

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      let result = AQL_EXECUTE(query, null, { fullCount: true });
      assertFalse(result.cached);
      assertEqual([ 11, 12, 13, 14, 15, 16, 17 ], result.json);
      assertEqual(10000, result.stats.fullCount);

      result = AQL_EXECUTE(query, null, { fullCount: true });
      assertTrue(result.cached);
      assertEqual([ 11, 12, 13, 14, 15, 16, 17 ], result.json);
      assertEqual(10000, result.stats.fullCount);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test setting modes
////////////////////////////////////////////////////////////////////////////////

    testModes : function () {
      var result;

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "off" });
      assertEqual("off", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("off", result.mode);

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      assertEqual("on", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("on", result.mode);

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "demand" });
      assertEqual("demand", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("demand", result.mode);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rename collection
////////////////////////////////////////////////////////////////////////////////

    testRenameCollection1 : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }
      c2.drop();

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      c1.rename("UnitTestsAhuacatlQueryCache2");

      try {
        AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache1" });
      } catch (err) {
        fail();
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }

      result = AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache2" });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache2" });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rename collection
////////////////////////////////////////////////////////////////////////////////

    testRenameCollection2 : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache2" });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache2" });
      assertTrue(result.cached);
      assertEqual([ ], result.json);

      c2.drop();
      c1.rename("UnitTestsAhuacatlQueryCache2");

      try {
        AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache1" });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }

      result = AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache2" });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache2" });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test drop collection
////////////////////////////////////////////////////////////////////////////////

    testDropCollection : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      c1.drop();

      try {
        AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache1" });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test drop and recreation of collection
////////////////////////////////////////////////////////////////////////////////

    testDropAndRecreateCollection : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      c1.drop();

      try {
        AQL_EXECUTE(query, { "@collection": "UnitTestsAhuacatlQueryCache1" });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }

      // re-create collection with same name
      c1 = db._create("UnitTestsAhuacatlQueryCache1");
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test queries w/ parse error
////////////////////////////////////////////////////////////////////////////////

    testParseError : function () {
      var query = "FOR i IN 1..3 RETURN";

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      try {
        AQL_EXECUTE(query);
        fail();
      } catch (err1) {
        assertEqual(internal.errors.ERROR_QUERY_PARSE.code, err1.errorNum);
      }

      // nothing should have been cached, so we shall be getting the same error again
      try {
        AQL_EXECUTE(query);
        fail();
      } catch (err2) {
        assertEqual(internal.errors.ERROR_QUERY_PARSE.code, err2.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test queries w/ other error
////////////////////////////////////////////////////////////////////////////////

    testOtherError : function () {
      db._drop("UnitTestsAhuacatlQueryCache3");

      var query = "FOR doc IN UnitTestsAhuacatlQueryCache3 RETURN doc";

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      try {
        AQL_EXECUTE(query);
        fail();
      } catch (err1) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err1.errorNum);
      }

      // nothing should have been cached, so we shall be getting the same error again
      try {
        AQL_EXECUTE(query);
        fail();
      } catch (err2) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err2.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test queries w/ warnings
////////////////////////////////////////////////////////////////////////////////

    testWarnings : function () {
      var query = "FOR i IN 1..3 RETURN i / 0";
      var result;

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query);
      assertFalse(result.cached);
      assertEqual([ null, null, null ], result.json);
      assertEqual(3, result.warnings.length);

      result = AQL_EXECUTE(query);
      assertFalse(result.cached); // won't be cached because of the warnings
      assertEqual([ null, null, null ], result.json);
      assertEqual(3, result.warnings.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-deterministic queries
////////////////////////////////////////////////////////////////////////////////

    testNonDeterministicQueriesRandom : function () {
      var query = "FOR doc IN @@collection RETURN RAND()";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual(5, result.json.length);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual(5, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-deterministic queries
////////////////////////////////////////////////////////////////////////////////

    testNonDeterministicQueriesDocument : function () {
      var query = "FOR i IN 1..5 RETURN DOCUMENT(@@collection, CONCAT('test', i))";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i, _key: "test" + i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual(5, result.json.length);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual(5, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test slightly different queries
////////////////////////////////////////////////////////////////////////////////

    testSlightlyDifferentQueries : function () {
      var queries = [
        "FOR doc IN @@collection SORT doc.value RETURN doc.value",
        "FOR doc IN @@collection SORT doc.value ASC RETURN doc.value",
        " FOR doc IN @@collection SORT doc.value RETURN doc.value",
        "FOR doc IN @@collection  SORT doc.value RETURN doc.value",
        "FOR doc IN @@collection SORT doc.value RETURN doc.value ",
        "FOR doc IN @@collection RETURN doc.value",
        "FOR doc IN @@collection RETURN doc.value ",
        " FOR doc IN @@collection RETURN doc.value ",
        "/* foo */ FOR doc IN @@collection RETURN doc.value",
        "FOR doc IN @@collection RETURN doc.value /* foo */",
        "FOR doc IN @@collection LIMIT 10 RETURN doc.value",
        "FOR doc IN @@collection FILTER doc.value < 99 RETURN doc.value",
        "FOR doc IN @@collection FILTER doc.value <= 99 RETURN doc.value",
        "FOR doc IN @@collection FILTER doc.value < 98 RETURN doc.value",
        "FOR doc IN @@collection RETURN doc.value + 0"
      ];

      for (var i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      queries.forEach(function (query) {
        var result = AQL_EXECUTE(query, { "@collection": c1.name() });
        assertFalse(result.cached);
        assertEqual(5, result.json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same query with different bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDifferentBindOrders : function () {
      var query = "FOR doc IN @@collection SORT doc.value LIMIT @offset, @count RETURN doc.value";
      var result, i;

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      for (i = 1; i <= 10; ++i) {
        c1.save({ value: i });
      }

      result = AQL_EXECUTE(query, { "@collection": c1.name(), offset: 0, count: 1 });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);
      
      result = AQL_EXECUTE(query, { "@collection": c1.name(), offset: 0, count: 1 });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      // same bind parameter values, but in exchanged order
      result = AQL_EXECUTE(query, { "@collection": c1.name(), offset: 1, count: 0 });
      assertFalse(result.cached);
      assertEqual([ ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same query with different bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDifferentBindOrdersArray : function () {
      var query = "RETURN @values";
      var result;

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });

      result = AQL_EXECUTE(query, { values: [ 1, 2, 3, 4, 5 ] });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json[0]);

      result = AQL_EXECUTE(query, { values: [ 1, 2, 3, 4, 5 ] });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json[0]);
      
      // same bind parameter values, but in exchanged order
      result = AQL_EXECUTE(query, { values: [ 5, 4, 3, 2, 1 ] });
      assertFalse(result.cached);
      assertEqual([ 5, 4, 3, 2, 1 ], result.json[0]);

      result = AQL_EXECUTE(query, { values: [ 1, 2, 3, 5, 4 ] });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 5, 4 ], result.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same query with different bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDifferentBindValues : function () {
      var query = "FOR doc IN @@collection FILTER doc.value == @value RETURN doc.value";
      var result, i;

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      for (i = 1; i <= 5; ++i) {
        result = AQL_EXECUTE(query, { "@collection": c1.name(), value: i });
        assertFalse(result.cached);
        assertEqual([ i ], result.json);
      }

      // now the query results should be fully cached
      for (i = 1; i <= 5; ++i) {
        result = AQL_EXECUTE(query, { "@collection": c1.name(), value: i });
        assertTrue(result.cached);
        assertEqual([ i ], result.json);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same query with different bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDifferentBindValuesCollection : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
        c2.save({ value: i + 1 });
      }

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      // now the query results should be fully cached
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c2.name() });
      assertFalse(result.cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c2.name() });
      assertTrue(result.cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single insert operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterRead : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      var doc = c1.save({ value: 1 });

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      c1.document(doc._key); // this will not invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single insert operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterInsertSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      c1.save({ value: 1 });

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      c1.save({ value: 2 }); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);

      assertEqual([ 1, 2 ], result.json);
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single update operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterUpdateSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      var doc = c1.save({ value: 1 });

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      c1.update(doc, { value: 42 }); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 42 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 42 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single remove operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterRemoveSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      c1.save({ value: 1 });

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      c1.remove(c1.any()._key); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after truncate operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterTruncate : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 10; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.json);

      c1.truncate({ compact: false }); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ ], result.json);

      for (i = 1; i <= 10; ++i) {
        c1.save({ value: i });
      }
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after AQL insert operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterAqlInsert : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("INSERT { value: 9 } INTO @@collection", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after AQL update operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterAqlUpdate : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("FOR doc IN @@collection UPDATE doc._key WITH { value: doc.value + 1 } IN @@collection", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after AQL remove operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterAqlRemove : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("FOR doc IN @@collection REMOVE doc._key IN @@collection", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after AQL multi-collection operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterAqlMulti : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      // collection1
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);
      
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      // collection2
      result = AQL_EXECUTE(query, { "@collection": c2.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c2.name() });
      assertTrue(result.cached);
      assertEqual([ ], result.json);

      AQL_EXECUTE("FOR doc IN @@collection1 INSERT doc IN @@collection2", { "@collection1" : c1.name(), "@collection2" : c2.name() });

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c2.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c2.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation of multiple queries
////////////////////////////////////////////////////////////////////////////////

    testInvalidationMultipleQueries : function () {
      var query1 = "FOR doc IN @@collection SORT doc.value ASC RETURN doc.value";
      var query2 = "FOR doc IN @@collection SORT doc.value DESC RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query1, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);
      
      result = AQL_EXECUTE(query1, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query2, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 5, 4, 3, 2, 1 ], result.json);
      
      result = AQL_EXECUTE(query2, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 5, 4, 3, 2, 1 ], result.json);

      c1.save({ value: 6 });

      result = AQL_EXECUTE(query1, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 6 ], result.json);

      result = AQL_EXECUTE(query1, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 6 ], result.json);

      result = AQL_EXECUTE(query2, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 6, 5, 4, 3, 2, 1 ], result.json);

      result = AQL_EXECUTE(query2, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 6, 5, 4, 3, 2, 1 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transaction commit
////////////////////////////////////////////////////////////////////////////////

    testTransactionCommit : function () {
      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      
      var query = "FOR doc IN @@collection RETURN doc.value";
      var result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      db._executeTransaction({
        collections: { write: c1.name() },
        action: function(params) {
          var db = require("@arangodb").db;
          db._collection(params.c1).insert({ value: "foo" });
        },
        params: { c1: c1.name() }
      });
      
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ "foo" ], result.json);
      
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ "foo" ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transaction rollback
////////////////////////////////////////////////////////////////////////////////

    testTransactionRollback : function () {
      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      
      var query = "FOR doc IN @@collection RETURN doc.value";
      
      try {
        db._executeTransaction({
          collections: { write: c1.name() },
          action: function(params) {
            var result = AQL_EXECUTE(query, { "@collection": c1.name() });
            assertFalse(result.cached);
            assertEqual([ ], result.json);
            
            result = AQL_EXECUTE(query, { "@collection": c1.name() });
            assertTrue(result.cached);
            assertEqual([ ], result.json);

            var db = require("@arangodb").db;
            db._collection(params.c1).insert({ value: "foo" });
        
            result = AQL_EXECUTE(query, { "@collection": c1.name() });
            assertFalse(result.cached);
            assertEqual([ "foo" ], result.json);
            
            result = AQL_EXECUTE(query, { "@collection": c1.name() });
            assertTrue(result.cached);
            assertEqual([ "foo" ], result.json);

            throw "peng!";
          },
          params: { c1: c1.name() }
        });
        fail();
      } catch (err) {
        assertMatch(/peng!/, String(err));
      }
            
      var result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);
    }

  };
}

function ahuacatlQueryCacheViewTestSuite () {
  var cacheProperties;
  var c1, c2, v;

  return {

    setUp : function () {
      cacheProperties = AQL_QUERY_CACHE_PROPERTIES();
      AQL_QUERY_CACHE_INVALIDATE();

      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");
      db._dropView("UnitTestsView");
      db._dropView("UnitTestsViewRenamed");

      c1 = db._create("UnitTestsAhuacatlQueryCache1");
      c2 = db._create("UnitTestsAhuacatlQueryCache2");

      v = db._createView("UnitTestsView", "arangosearch", {});
    },

    tearDown : function () {
      db._dropView("UnitTestsView");
      db._dropView("UnitTestsViewRenamed");
      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");

      AQL_QUERY_CACHE_PROPERTIES(cacheProperties);
      AQL_QUERY_CACHE_INVALIDATE();
    },

    testQueryOnView : function () {
      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);
      
      c1.insert({ value: 1 }, { waitForSync: true });

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      let result1 = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result1.cached);
      assertEqual(1, result1.json.length);

      let result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result2.cached);
      assertEqual(1, result2.json.length);
      assertEqual(result1.json, result2.json);
    },

    testRenameView : function () {
      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);
      
      c1.insert({ value: 1 }, { waitForSync: true });

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      let result1 = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result1.cached);
      assertEqual(1, result1.json.length);

      let result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result2.cached);
      assertEqual(1, result2.json.length);
      assertEqual(result1.json, result2.json);

      v.rename("UnitTestsViewRenamed");

      try {
        AQL_EXECUTE(query, { "@view": "UnitTestsView" });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }

      result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result2.cached);
      assertEqual(result1.json, result2.json);

      result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result2.cached);
      assertEqual(result1.json, result2.json);
    },
    
    testPropertyChangeView : function () {
      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);
      
      c1.insert({ value: 1 }, { waitForSync: true });

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      let result1 = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result1.cached);
      assertEqual(1, result1.json.length);

      let result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result2.cached);
      assertEqual(1, result2.json.length);
      assertEqual(result1.json, result2.json);

      meta = { links: { "UnitTestsAhuacatlQueryCache1" : null, "UnitTestsAhuacatlQueryCache2" : { includeAllFields: true } } };
      v.properties(meta);

      c2.insert({ value: 1 }, { waitForSync: false });
      c2.insert({ value: 2 }, { waitForSync: true });
      
      result1 = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result1.cached);
      assertEqual(2, result1.json.length);

      result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result2.cached);
      assertEqual(2, result2.json.length);
    },

    testDropView : function () {
      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);
      
      c1.insert({ value: 1 }, { waitForSync: true });

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      let result1 = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result1.cached);
      assertEqual(1, result1.json.length);

      let result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result2.cached);
      assertEqual(1, result2.json.length);
      assertEqual(result1.json, result2.json);

      v.drop();

      try {
        AQL_EXECUTE(query, { "@view": "UnitTestsView" });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },

    testDropAndRecreateView : function () {
      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);
      
      c1.insert({ value: 1 }, { waitForSync: true });

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      let result1 = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result1.cached);
      assertEqual(1, result1.json.length);

      let result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result2.cached);
      assertEqual(1, result2.json.length);
      assertEqual(result1.json, result2.json);

      v.drop();
      
      v = db._createView("UnitTestsView", "arangosearch", {});
      v.properties(meta);
      
      result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result2.cached);
      assertEqual(1, result2.json.length);

      result2 = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result2.cached);
      assertEqual(1, result2.json.length);
    },
    
    testViewInvalidationAfterAqlInsertNoSync : function () {
      if (!internal.debugCanUseFailAt()) {
        return;
      }

      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);

      let result;

      for (let i = 1; i <= 5; ++i) {
        c1.insert({ value: i }, { waitForSync: i === 5 });
      }


      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE("FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value", { "@view": v.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE("FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value", { "@view": v.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      try {
        internal.debugSetFailAt("FlushThreadDisableAll");
        internal.sleep(5); // give FlushThread some time

        // explicitly without waitForSync here
        AQL_EXECUTE("INSERT { value: 9 } INTO @@collection", { "@collection" : c1.name() });

        // re-run query to repopulate the cache. however, the document is not yet contained
        // in the view as we turned off the flush thread
        result = AQL_EXECUTE("FOR doc IN @@view SORT doc.value RETURN doc.value", { "@view": v.name() });
        assertFalse(result.cached);
        assertEqual([ 1, 2, 3, 4, 5 ], result.json);

        result = AQL_EXECUTE("FOR doc IN @@view SORT doc.value RETURN doc.value", { "@view": v.name() });
        assertTrue(result.cached);
        assertEqual([ 1, 2, 3, 4, 5 ], result.json);
      } finally {
        internal.debugClearFailAt();
      }
        
      internal.sleep(5); // give FlushThread some time
        
      // invalidate view query cache
      AQL_EXECUTE("FOR doc in @@view OPTIONS { waitForSync: true } COLLECT WITH COUNT INTO count RETURN count", { "@view": v.name() });

      result = AQL_EXECUTE("FOR doc IN @@view SORT doc.value RETURN doc.value", { "@view": v.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.json);
      
      result = AQL_EXECUTE("FOR doc IN @@view SORT doc.value RETURN doc.value", { "@view": v.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.json);
    },
    
    testViewInvalidationAfterAqlInsert : function () {
      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value";
      let result;

      for (let i = 1; i <= 5; ++i) {
        c1.insert({ value: i }, { waitForSync: i === 5 });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("INSERT { value: 9 } INTO @@collection OPTIONS { waitForSync: true }", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.json);

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.json);
    },
    
    testViewInvalidationAfterAqlUpdate : function () {
      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value";
      let result;

      for (let i = 1; i <= 5; ++i) {
        c1.insert({ _key: "test" + i, value: i }, { waitForSync: i === 5 });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("UPDATE 'test5' WITH { value: 9 } INTO @@collection OPTIONS { waitForSync: true }", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 9 ], result.json);

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 9 ], result.json);
    },
    
    testViewInvalidationAfterAqlRemove : function () {
      let meta = { links: { "UnitTestsAhuacatlQueryCache1" : { includeAllFields: true } } };
      v.properties(meta);

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value";
      let result;

      for (let i = 1; i <= 5; ++i) {
        c1.insert({ _key: "test" + i, value: i }, { waitForSync: i === 5 });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("REMOVE 'test5' INTO @@collection OPTIONS { waitForSync: true }", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4 ], result.json);

      result = AQL_EXECUTE(query, { "@view": v.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4 ], result.json);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryCacheTestSuite);
jsunity.run(ahuacatlQueryCacheViewTestSuite);

return jsunity.done();
