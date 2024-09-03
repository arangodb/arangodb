/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse, assertMatch  */

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
let cache = require("@arangodb/aql/cache");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const isEnterprise = internal.isEnterprise();

function ahuacatlQueryCacheTestSuite () {
  var cacheProperties;
  var c1, c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      cacheProperties = cache.properties();
      cache.clear();
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
      cache.properties(cacheProperties);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fullCount
////////////////////////////////////////////////////////////////////////////////

    testFullCount : function () {
      let query = "FOR i IN 1..10000 LIMIT 10, 7 RETURN i";

      cache.properties({ mode: "on" });
      let result = db._createStatement({query: query, bindVars: null, options: {fullCount: true}}).execute();
      assertFalse(result._cached);
      assertEqual([ 11, 12, 13, 14, 15, 16, 17 ], result.toArray());
      assertEqual(10000, result.getExtra().stats.fullCount);

      let result1 = db._createStatement({query: query, bindVars: null, options: {fullCount: true}}).execute();
      assertTrue(result1._cached);
      assertEqual([ 11, 12, 13, 14, 15, 16, 17 ], result1.toArray());
      assertEqual(10000, result1.getExtra().stats.fullCount);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test setting modes
////////////////////////////////////////////////////////////////////////////////

    testModes : function () {
      var result;

      result = cache.properties({ mode: "off" });
      assertEqual("off", result.mode);
      cache.clear();
      result = cache.properties();
      assertEqual("off", result.mode);

      result = cache.properties({ mode: "on" });
      assertEqual("on", result.mode);
      result = cache.properties();
      assertEqual("on", result.mode);

      result = cache.properties({ mode: "demand" });
      assertEqual("demand", result.mode);
      cache.clear();
      result = cache.properties();
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      c1.rename("UnitTestsAhuacatlQueryCache2");

      try {
        db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache1" }}).execute();
      } catch (err) {
        fail();
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }

      result = db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache2" }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache2" }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache2" }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache2" }}).execute();
      assertTrue(result._cached);
      assertEqual([ ], result.toArray());

      c2.drop();
      c1.rename("UnitTestsAhuacatlQueryCache2");

      try {
        db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache1" }}).execute();
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }

      result = db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache2" }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache2" }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      c1.drop();

      try {
        db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache1" }}).execute();
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      c1.drop();

      try {
        db._createStatement({query: query, bindVars:  { "@collection": "UnitTestsAhuacatlQueryCache1" }}).execute();
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }

      // re-create collection with same name
      c1 = db._create("UnitTestsAhuacatlQueryCache1");
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ ], result.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test queries w/ parse error
////////////////////////////////////////////////////////////////////////////////

    testParseError : function () {
      var query = "FOR i IN 1..3 RETURN";

      cache.properties({ mode: "on" });
      try {
        db._query(query);
        fail();
      } catch (err1) {
        assertEqual(internal.errors.ERROR_QUERY_PARSE.code, err1.errorNum);
      }

      // nothing should have been cached, so we shall be getting the same error again
      try {
        db._query(query);
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

      cache.properties({ mode: "on" });
      try {
        db._query(query);
        fail();
      } catch (err1) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err1.errorNum);
      }

      // nothing should have been cached, so we shall be getting the same error again
      try {
        db._query(query);
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

      cache.properties({ mode: "on" });
      result = db._query(query);
      assertFalse(result._cached);
      assertEqual([ null, null, null ], result.toArray());
      assertEqual(3, result.getExtra().warnings.length);

      result = db._query(query);
      assertFalse(result._cached); // won't be cached because of the warnings
      assertEqual([ null, null, null ], result.toArray());
      assertEqual(3, result.getExtra().warnings.length);
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual(5, result.toArray().length);

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual(5, result.toArray().length);
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual(5, result.toArray().length);

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual(5, result.toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test slightly different queries
////////////////////////////////////////////////////////////////////////////////

    testSlightlyDifferentQueries : function () {
      var testQueries = [
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

      cache.properties({ mode: "on" });
      testQueries.forEach(function (query) {
        var result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
        assertFalse(result._cached);
        assertEqual(5, result.toArray().length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same query with different bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDifferentBindOrders : function () {
      var query = "FOR doc IN @@collection SORT doc.value LIMIT @offset, @count RETURN doc.value";
      var result, i;

      cache.properties({ mode: "on" });
      for (i = 1; i <= 10; ++i) {
        c1.save({ value: i });
      }

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name(), offset: 0, count: 1 }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1 ], result.toArray());
      
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name(), offset: 0, count: 1 }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1 ], result.toArray());

      // same bind parameter values, but in exchanged order
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name(), offset: 1, count: 0 }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same query with different bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDifferentBindOrdersArray : function () {
      var query = "RETURN @values";
      var result;

      cache.properties({ mode: "on" });

      result = db._createStatement({query: query, bindVars:  { values: [ 1, 2, 3, 4, 5 ] }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray()[0]);

      result = db._createStatement({query: query, bindVars:  { values: [ 1, 2, 3, 4, 5 ] }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray()[0]);
      
      // same bind parameter values, but in exchanged order
      result = db._createStatement({query: query, bindVars:  { values: [ 5, 4, 3, 2, 1 ] }}).execute();
      assertFalse(result._cached);
      assertEqual([ 5, 4, 3, 2, 1 ], result.toArray()[0]);

      result = db._createStatement({query: query, bindVars:  { values: [ 1, 2, 3, 5, 4 ] }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 5, 4 ], result.toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same query with different bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDifferentBindValues : function () {
      var query = "FOR doc IN @@collection FILTER doc.value == @value RETURN doc.value";
      var result, i;

      cache.properties({ mode: "on" });
      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      for (i = 1; i <= 5; ++i) {
        result = db._createStatement({query: query, bindVars:  { "@collection": c1.name(), value: i }}).execute();
        assertFalse(result._cached);
        assertEqual([ i ], result.toArray());
      }

      // now the query results should be fully cached
      for (i = 1; i <= 5; ++i) {
        result = db._createStatement({query: query, bindVars:  { "@collection": c1.name(), value: i }}).execute();
        assertTrue(result._cached);
        assertEqual([ i ], result.toArray());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same query with different bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDifferentBindValuesCollection : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      cache.properties({ mode: "on" });
      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
        c2.save({ value: i + 1 });
      }

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      // now the query results should be fully cached
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c2.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c2.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single insert operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterRead : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      var doc = c1.save({ value: 1 });

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1 ], result.toArray());

      c1.document(doc._key); // this will not invalidate cache

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1 ], result.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single insert operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterInsertSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      c1.save({ value: 1 });

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1 ], result.toArray());

      c1.save({ value: 2 }); // this will invalidate cache

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);

      assertEqual([ 1, 2 ], result.toArray());
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2 ], result.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single update operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterUpdateSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      var doc = c1.save({ value: 1 });

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1 ], result.toArray());

      c1.update(doc, { value: 42 }); // this will invalidate cache

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 42 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 42 ], result.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single remove operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterRemoveSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      c1.save({ value: 1 });

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1 ], result.toArray());

      c1.remove(c1.any()._key); // this will invalidate cache

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ ], result.toArray());
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.toArray());

      c1.truncate({ compact: false }); // this will invalidate cache

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ ], result.toArray());

      for (i = 1; i <= 10; ++i) {
        c1.save({ value: i });
      }
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.toArray());
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      db._query("INSERT { value: 9 } INTO @@collection", { "@collection" : c1.name() });

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.toArray());
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      db._query("FOR doc IN @@collection UPDATE doc._key WITH { value: doc.value + 1 } IN @@collection", { "@collection" : c1.name() });

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.toArray());
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      db._createStatement({query: "FOR doc IN @@collection REMOVE doc._key IN @@collection", bindVars:  { "@collection" : c1.name() }}).execute();

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ ], result.toArray());
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

      cache.properties({ mode: "on" });
      // collection1
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());
      
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      // collection2
      result = db._createStatement({query: query, bindVars:  { "@collection": c2.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c2.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ ], result.toArray());

      db._createStatement({query: "FOR doc IN @@collection1 INSERT doc IN @@collection2", bindVars:  { "@collection1" : c1.name(), "@collection2" : c2.name() }}).execute();

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c2.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c2.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());
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

      cache.properties({ mode: "on" });
      result = db._createStatement({query: query1, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());
      
      result = db._createStatement({query: query1, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query2, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 5, 4, 3, 2, 1 ], result.toArray());
      
      result = db._createStatement({query: query2, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 5, 4, 3, 2, 1 ], result.toArray());

      c1.save({ value: 6 });

      result = db._createStatement({query: query1, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 6 ], result.toArray());

      result = db._createStatement({query: query1, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 6 ], result.toArray());

      result = db._createStatement({query: query2, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 6, 5, 4, 3, 2, 1 ], result.toArray());

      result = db._createStatement({query: query2, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 6, 5, 4, 3, 2, 1 ], result.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transaction commit
////////////////////////////////////////////////////////////////////////////////

    testTransactionCommit : function () {
      cache.properties({ mode: "on" });
      
      var query = "FOR doc IN @@collection RETURN doc.value";
      var result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());

      db._executeTransaction({
        collections: { write: c1.name() },
        action: function(params) {
          var db = require("@arangodb").db;
          db._collection(params.c1).insert({ value: "foo" });
        },
        params: { c1: c1.name() }
      });
      
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ "foo" ], result.toArray());
      
      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ "foo" ], result.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transaction rollback
////////////////////////////////////////////////////////////////////////////////

    testTransactionRollback : function () {
      cache.properties({ mode: "on" });

      var query = "FOR doc IN @@collection RETURN doc.value";
      
      try {
        db._executeTransaction({
          collections: { write: c1.name() },
          action: function(params) {
            var db = require("@arangodb").db;
            const _ = require('lodash');

            var result = db._query(params.query, { "@collection": params.c1 });
            if (result._cached) {
              throw "err1";
            }
            if (!_.isEqual([ ], result.toArray())) {
              throw "err2";
            }
            
            result = db._query(params.query, { "@collection": params.c1 });
            if (result._cached) {
              throw "err3";
            }
            if (!_.isEqual([ ], result.toArray())) {
              throw "err4";
            }

            db._collection(params.c1).insert({ value: "foo" });
        
            result = db._query(params.query, { "@collection": params.c1 });
            if(result._cached) {
              throw "err5";
            }
            if(!_.isEqual([ "foo" ], result.toArray())) {
              throw "err6";
            }
            
            result = db._query(params.query, { "@collection": params.c1 });
            if (result._cached) {
              throw "err7";
            }
            if (!_.isEqual([ "foo" ], result.toArray())) {
              throw "err8";
            }

            throw "peng!";
          },
          params: { c1: c1.name(), query: query }
        });
        fail();
      } catch (err) {
        assertMatch(/peng!/, String(err));
      }
            
      var result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@collection": c1.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ ], result.toArray());
    }

  };
}

function ahuacatlQueryCacheViewTestSuite(isSearchAlias) {
  var cacheProperties;
  var c1, c2, v;

  return {

    setUp: function () {
      cacheProperties = cache.properties();
      cache.clear();

      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");
      db._dropView("UnitTestsView");
      db._dropView("UnitTestsViewRenamed");

      c1 = db._create("UnitTestsAhuacatlQueryCache1");
      c2 = db._create("UnitTestsAhuacatlQueryCache2");
      if (isSearchAlias) {
        v = db._createView("UnitTestsView", "search-alias", {});
      } else {
        v = db._createView("UnitTestsView", "arangosearch", {});
      }
    },

    tearDown : function () {
      db._dropView("UnitTestsView");
      db._dropView("UnitTestsViewRenamed");
      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");

      cache.properties(cacheProperties);
    },

    testQueryOnView : function () {
      let viewMeta = {};
      let indexMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }

        let i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      c1.insert({value: 1}, {waitForSync: true});
      c1.insert({"value_nested": [{ "nested_1": [{ "nested_2": "dog" }] }]}, {waitForSync: true});

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      cache.properties({mode: "on"});
      let result1 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertFalse(result1._cached);
      assertEqual(2, result1.toArray().length);

      let result2 = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result2._cached);
      assertEqual(2, result2.toArray().length);
      assertEqual(result1.toArray(), result2.toArray());
    },

    testRenameView : function () {
      let viewMeta = {};
      let indexMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }
        let i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      c1.insert({value: 1}, {waitForSync: true});
      c1.insert({"value_nested": [{ "nested_1": [{ "nested_2": "chpok" }] }]}, {waitForSync: true});

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      cache.properties({mode: "on"});
      let result1 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertFalse(result1._cached);
      let res1_json = result1.toArray();
      assertEqual(2, res1_json.length);

      let result2 = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result2._cached);
      let res2_json = result2.toArray();
      assertEqual(2, res2_json.length);
      
      assertEqual(res1_json,res2_json);

      v.rename("UnitTestsViewRenamed");

      try {
        db._createStatement({query: query, bindVars:  { "@view": "UnitTestsView" }}).execute();
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }

      result2 = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertFalse(result2._cached);
      assertEqual(res1_json, result2.toArray());

      result2 = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result2._cached);
      assertEqual(res1_json, result2.toArray());
    },
    
    testPropertyChangeView : function () {
      let i;
      let viewMeta = {};
      let indexMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }
        i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      c1.insert({value: 1}, {waitForSync: true});
      c1.insert({"value_nested": [{ "nested_1": [{ "nested_2": "dog" }] }]}, {waitForSync: true});

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      cache.properties({mode: "on"});
      let result1 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertFalse(result1._cached);
      assertEqual(2, result1.toArray().length);

      let result2 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertTrue(result2._cached);
      assertEqual(2, result2.toArray().length);
      assertEqual(result1.toArray(), result2.toArray());

      if (isSearchAlias) {
        let i2 = db.UnitTestsAhuacatlQueryCache2.ensureIndex(indexMeta);
        viewMeta = {
          indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name, operation: "del"},
            {collection: "UnitTestsAhuacatlQueryCache2", index: i2.name}]
        };
      } else {
        if (isEnterprise) {
          viewMeta = {
            links: {
              "UnitTestsAhuacatlQueryCache1": null,
              "UnitTestsAhuacatlQueryCache2": {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}
            }
          };
        } else {
          viewMeta = {
            links: {
              "UnitTestsAhuacatlQueryCache1": null,
              "UnitTestsAhuacatlQueryCache2": {includeAllFields: true}
            }
          };
        }
      }
      v.properties(viewMeta);

      c2.insert({value: 1}, {waitForSync: false});
      c2.insert({value: 2}, {waitForSync: true});
      c2.insert({"value_nested": [{ "nested_1": [{ "nested_2": "dog" }] }]}, {waitForSync: true});

      result1 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertFalse(result1._cached);
      assertEqual(3, result1.toArray().length);

      result2 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertTrue(result2._cached);
      assertEqual(3, result2.toArray().length);
    },

    testDropView : function () {
      let viewMeta = {};
      let indexMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }
        let i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      c1.insert({value: 1}, {waitForSync: true});
      c1.insert({"value_nested": [{ "nested_1": [{ "nested_2": "frog" }] }]}, {waitForSync: true});

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      cache.properties({mode: "on"});
      let result1 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertFalse(result1._cached);
      assertEqual(2, result1.toArray().length);

      let result2 = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result2._cached);
      assertEqual(2, result2.toArray().length);
      assertEqual(result1.toArray(), result2.toArray());

      v.drop();

      try {
        db._createStatement({query: query, bindVars:  { "@view": "UnitTestsView" }}).execute();
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },

    testDropAndRecreateView : function () {
      let viewMeta = {};
      let indexMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }
        
        let i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      c1.insert({value: 1}, {waitForSync: true});
      c1.insert({"value_nested": [{ "nested_1": [{ "nested_2": "bat" }] }]}, {waitForSync: true});

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } RETURN doc.value";
      cache.properties({mode: "on"});
      let result1 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertFalse(result1._cached);
      assertEqual(2, result1.toArray().length);

      let result2 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertTrue(result2._cached);
      assertEqual(2, result2.toArray().length);
      assertEqual(result1.toArray(), result2.toArray());

      v.drop();

      if (isSearchAlias) {
        v = db._createView("UnitTestsView", "search-alias", {});
      } else {
        v = db._createView("UnitTestsView", "arangosearch", {});
      }
      v.properties(viewMeta);

      result2 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertFalse(result2._cached);
      assertEqual(2, result2.toArray().length);

      result2 = db._createStatement({query: query, bindVars:  {"@view": v.name()}}).execute();
      assertTrue(result2._cached);
      assertEqual(2, result2.toArray().length);
    },
    
    testViewInvalidationAfterAqlInsertNoSync : function () {
      if (!global.instanceManager.debugCanUseFailAt()) {
        return;
      }
      let indexMeta = {};
      let viewMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }
        
        let i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      let result;

      for (let i = 1; i <= 5; ++i) {
        c1.insert({value: i, "value_nested": [{ "nested_1": [{ "nested_2": "bat" }] }]}, {waitForSync: i === 5});
      }

      cache.properties({mode: "on"});
      result = db._query("FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value", { "@view": v.name() });
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._query("FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value", { "@view": v.name() });
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      try {
        global.instanceManager.debugSetFailAt("RocksDBBackgroundThread::run");
        internal.sleep(5); // give FlushThread some time

        // explicitly without waitForSync here
        db._query("INSERT { value: 9 } INTO @@collection", { "@collection" : c1.name() });

        // re-run query to repopulate the cache. however, the document is not yet contained
        // in the view as we turned off the flush thread
        result = db._createStatement({query: "FOR doc IN @@view SORT doc.value RETURN doc.value", bindVars:  { "@view": v.name() }}).execute();
        assertFalse(result._cached);
        assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

        result = db._createStatement({query: "FOR doc IN @@view SORT doc.value RETURN doc.value", bindVars:  { "@view": v.name() }}).execute();
        assertTrue(result._cached);
        assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());
      } finally {
        global.instanceManager.debugClearFailAt();
      }
        
      internal.sleep(5); // give FlushThread some time
        
      // invalidate view query cache
      db._query("FOR doc in @@view OPTIONS { waitForSync: true } COLLECT WITH COUNT INTO count RETURN count", { "@view": v.name() });

      result = db._createStatement({query: "FOR doc IN @@view SORT doc.value RETURN doc.value", bindVars:  { "@view": v.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.toArray());
      
      result = db._createStatement({query: "FOR doc IN @@view SORT doc.value RETURN doc.value", bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.toArray());
    },
    
    testViewInvalidationAfterAqlInsert : function () {
      let viewMeta = {};
      let indexMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }

        let i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value";
      let result;

      for (let i = 1; i <= 5; ++i) {
        c1.insert({value: i, "value_nested": [{ "nested_1": [{ "nested_2": "bat" }] }]}, {waitForSync: i === 5});
      }

      cache.properties({mode: "on"});
      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      db._query("INSERT { value: 9 } INTO @@collection OPTIONS { waitForSync: true }", { "@collection" : c1.name() });

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.toArray());
    },
    
    testViewInvalidationAfterAqlUpdate : function () {
      let viewMeta  = {};
      let indexMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }

        let i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value";
      let result;

      for (let i = 1; i <= 5; ++i) {
        c1.insert({_key: "test" + i, value: i, "value_nested": [{ "nested_1": [{ "nested_2": "bat" }] }]}, {waitForSync: i === 5});
      }

      cache.properties({mode: "on"});
      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      db._query("UPDATE 'test5' WITH { value: 9 } INTO @@collection OPTIONS { waitForSync: true }", { "@collection" : c1.name() });

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 9 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 9 ], result.toArray());
    },
    
    testViewInvalidationAfterAqlRemove : function () {
      let viewMeta = {};
      let indexMeta = {};
      if (isSearchAlias) {
        if (isEnterprise) {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
          ]};
        } else {
          indexMeta = {type: "inverted", includeAllFields: true, fields:[
            {"name": "value_nested[*]"}
          ]};
        }

        let i = db.UnitTestsAhuacatlQueryCache1.ensureIndex(indexMeta);
        viewMeta = {indexes: [{collection: "UnitTestsAhuacatlQueryCache1", index: i.name}]};
      } else {
        if (isEnterprise) {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": 
          {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {"UnitTestsAhuacatlQueryCache1": {includeAllFields: true}}};
        }
      }
      v.properties(viewMeta);

      let query = "FOR doc IN @@view OPTIONS { waitForSync: true } SORT doc.value RETURN doc.value";
      let result;

      for (let i = 1; i <= 5; ++i) {
        c1.insert({_key: "test" + i, value: i, "value_nested": [{ "nested_1": [{ "nested_2": "bat" }] }]}, {waitForSync: i === 5});
      }

      cache.properties({mode: "on"});
      result = db._createStatement({query: query, bindVars: { "@view": v.name() }, options: { cache: true }}).execute();

      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.toArray());

      db._query("REMOVE 'test5' INTO @@collection OPTIONS { waitForSync: true }", { "@collection" : c1.name() });

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertFalse(result._cached);
      assertEqual([ 1, 2, 3, 4 ], result.toArray());

      result = db._createStatement({query: query, bindVars:  { "@view": v.name() }}).execute();
      assertTrue(result._cached);
      assertEqual([ 1, 2, 3, 4 ], result.toArray());
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryCacheTestSuite);

function ahuacatlQueryCacheArangoSearchTestSuite() {
  let suite = {};
  deriveTestSuite(
    ahuacatlQueryCacheViewTestSuite(false),
    suite,
    "_arangosearch"
  );
  return suite;
}

function ahuacatlQueryCacheSearchAliasTestSuite() {
  let suite = {};
  deriveTestSuite(
    ahuacatlQueryCacheViewTestSuite(true),
    suite,
    "_search-alias"
  );
  return suite;
}

jsunity.run(ahuacatlQueryCacheArangoSearchTestSuite);
jsunity.run(ahuacatlQueryCacheSearchAliasTestSuite);

return jsunity.done();
