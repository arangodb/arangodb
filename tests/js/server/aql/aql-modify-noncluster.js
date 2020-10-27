/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertNull, assertNotNull, assertTrue, 
  assertNotEqual, assertUndefined, fail, AQL_EXECUTE */

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
  delete stats.executionTime;
  delete stats.httpRequests;
  delete stats.fullCount;
  delete stats.peakMemoryUsage;
  return stats;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief 
////////////////////////////////////////////////////////////////////////////////

var validateDocuments = function (documents, isEdgeCollection) {
  var index;
  for (index in documents) {
    if (documents.hasOwnProperty(index)) {
      assertTrue(documents[index].hasOwnProperty('_id'));
      assertTrue(documents[index].hasOwnProperty('_key'));
      assertTrue(documents[index].hasOwnProperty('_rev'));
      if (isEdgeCollection) {
        assertTrue(documents[index].hasOwnProperty('_from'));
        assertTrue(documents[index].hasOwnProperty('_to'));
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the documents inserted are equal on the db.
////////////////////////////////////////////////////////////////////////////////

var validateModifyResultInsert = function (collection, results) {
  var index;
  for (index in results) {
    if (results.hasOwnProperty(index)){
      assertTrue(isEqual(collection.document(results[index]._key), results[index]));
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the documents reported deleted are really gone
////////////////////////////////////////////////////////////////////////////////

var validateDeleteGone = function (collection, results) {
  var index;
  for (index in results) {
    if (results.hasOwnProperty(index)){
      try {
        assertEqual(collection.document(results[index]._key), {});
        fail();
      }
      catch (e) {
        assertTrue(e.errorNum !== undefined, "unexpected error format while calling checking for deleted entry");
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, e.errorNum, "unexpected error code (" + e.errorMessage + "): ");
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief convert flat document database to an associative array with the keys
///        as object
////////////////////////////////////////////////////////////////////////////////

var wrapToKeys = function (results) {
  var keyArray = {};
  var index;
  for (index in results) {
    if (results.hasOwnProperty(index)){
      keyArray[results[index]._key] = results[index];
    }    
  }
  return keyArray;
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
      c1 = db._create(cn1);
      c2 = db._create(cn2);

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
    
    testVariableScope : function () {
      let query = "INSERT {value: 1} INTO " + cn1 + " LET id1 = (UPDATE 'foo' WITH {value: 2} IN " + cn2 + ")  RETURN NEW";
      let result = AQL_EXECUTE(query).json;
      assertEqual(1, result[0].value);
      assertEqual(2, c1.count()); 
      assertEqual(1, c2.count()); 
      assertEqual(2, c2.document("foo").value);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of NEW
////////////////////////////////////////////////////////////////////////////////

    testInvalidUsageOfNew : function () {
      assertQueryError(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, "REMOVE 'abc' IN @@cn LET removed = NEW RETURN removed", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD
////////////////////////////////////////////////////////////////////////////////

    testInvalidUsageOfOld : function () {
      assertQueryError(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, "INSERT { } IN @@cn LET inserted = OLD RETURN inserted", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variable names
////////////////////////////////////////////////////////////////////////////////

    testInvalidVariableNames1 : function () {
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, "REMOVE 'abc' IN @@cn LET removed1 = OLD RETURN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variable names
////////////////////////////////////////////////////////////////////////////////

    testInvalidVariableNames2 : function () {
      assertQueryError(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, "REMOVE 'abc' IN @@cn LET removed1 = OLD RETURN removed2", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variable names
////////////////////////////////////////////////////////////////////////////////

    testInvalidVariableNames3 : function () {
      assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, "UPDATE 'abc' WITH { } IN @@cn LET updated = NEW RETURN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variable names
////////////////////////////////////////////////////////////////////////////////

    testInvalidVariableNames4 : function () {
      assertQueryError(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, "UPDATE 'abc' WITH { } IN @@cn LET updated = NEW RETURN foo", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty results
////////////////////////////////////////////////////////////////////////////////

    testEmptyResultRemove : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR d IN " + cn1 + " REMOVE d IN " + cn1, {});
    
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty results
////////////////////////////////////////////////////////////////////////////////

    testEmptyResultInsert : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("INSERT { _key: 'baz' } IN " + cn1, {});

      assertEqual(2, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty results
////////////////////////////////////////////////////////////////////////////////

    testEmptyResultUpdate : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPDATE { _key: 'foo' } WITH { baz: true } IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty results
////////////////////////////////////////////////////////////////////////////////

    testEmptyResultReplace : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("REPLACE { _key: 'foo' } WITH { baz: true } IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD/NEW variables
////////////////////////////////////////////////////////////////////////////////

    testOldNewUsageRemove : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR OLD IN " + cn1 + " LET NEW = OLD REMOVE NEW IN " + cn1, {});

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD/NEW variables
////////////////////////////////////////////////////////////////////////////////

    testOldNewUsageInsert : function () {
      var expected = { writesExecuted: 0, writesIgnored: 1 };
      var actual = AQL_EXECUTE("FOR OLD IN " + cn1 + " LET NEW = OLD INSERT NEW IN " + cn1 + " OPTIONS { ignoreErrors: true }", {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD/NEW variables
////////////////////////////////////////////////////////////////////////////////

    testOldNewUsageUpdate : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR OLD IN " + cn1 + " LET NEW = OLD UPDATE NEW WITH NEW IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD/NEW variables
////////////////////////////////////////////////////////////////////////////////

    testOldNewUsageReplace : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR OLD IN " + cn1 + " LET NEW = OLD REPLACE NEW WITH NEW IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyCollection : function () {
      c1.truncate({ compact: false });

      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } UPDATE { bark: 'bart' } IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.toArray()[0];
      assertFalse(doc.hasOwnProperty("a"));
      assertEqual("baz", doc.bar);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyCollectionBind : function () {
      c1.truncate({ compact: false });

      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT @what INSERT { bar: 'baz' } UPDATE { bark: 'bart' } IN " + cn1, { what: { } });

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.toArray()[0];
      assertFalse(doc.hasOwnProperty("a"));
      assertEqual("baz", doc.bar);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmpty : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } UPDATE { bark: 'bart' } IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertEqual(1, doc.a);
      assertEqual("bart", doc.bark);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyBind : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT @what INSERT { bar: 'baz' } UPDATE { bark: 'bart' } IN " + cn1, { what: { } });

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertEqual(1, doc.a);
      assertEqual("bart", doc.bark);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyReplace : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } REPLACE { bark: 'bart' } IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertFalse(doc.hasOwnProperty("a"));
      assertEqual("bart", doc.bark);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyUpdateKeepNullTrue : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } UPDATE { bark: 'bart', foxx: null, a: null } IN " + cn1 + " OPTIONS { keepNull: true }", {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertTrue(doc.hasOwnProperty("a"));
      assertTrue(doc.hasOwnProperty("foxx"));
      assertNull(doc.a);
      assertNull(doc.foxx);
      assertEqual("bart", doc.bark);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyUpdateKeepNullFalse : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } UPDATE { bark: 'bart', foxx: null, a: null } IN " + cn1 + " OPTIONS { keepNull: false }", {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertFalse(doc.hasOwnProperty("a"));
      assertFalse(doc.hasOwnProperty("foxx"));
      assertEqual("bart", doc.bark);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyUpdateMergeObjectsTrue : function () {
      c1.save({ "c" : { a: 1, b: 2 } });

      var actual = AQL_EXECUTE("UPSERT { `c` : { a: 1, b: 2 } } INSERT { } UPDATE { `c` : { c : 3 } } IN " + cn1 + " OPTIONS { mergeObjects: true } RETURN NEW", {});
      assertEqual(2, c1.count());

      assertEqual(1, actual.json.length);
      var doc = actual.json[0];
      assertTrue(doc.hasOwnProperty("c"));
      assertEqual({ a: 1, b: 2, c: 3 }, doc["c"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyUpdateMergeObjectsTrueBind : function () {
      c1.save({ "c" : { a: 1, b: 2 } });

      var actual = AQL_EXECUTE("UPSERT @what INSERT { } UPDATE { `c` : { c : 3 } } IN " + cn1 + " OPTIONS { mergeObjects: true } RETURN NEW", { what: { c: { a: 1, b: 2 } } });
      assertEqual(2, c1.count());

      assertEqual(1, actual.json.length);
      var doc = actual.json[0];
      assertTrue(doc.hasOwnProperty("c"));
      assertEqual({ a: 1, b: 2, c: 3 }, doc["c"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyUpdateMergeObjectsFalse : function () {
      c1.save({ "c" : { a: 1, b: 2 } });

      var actual = AQL_EXECUTE("UPSERT { 'c' : { a: 1, b: 2 } } INSERT { } UPDATE { 'c' : { c : 3 } } IN " + cn1 + " OPTIONS { mergeObjects: false } RETURN NEW", {});

      assertEqual(2, c1.count());
      assertEqual(1, actual.json.length);
      var doc = actual.json[0];
      assertTrue(doc.hasOwnProperty("c"));
      assertEqual({ c: 3 }, doc["c"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyUpdateMergeObjectsFalseBind : function () {
      c1.save({ "c" : { a: 1, b: 2 } });

      var actual = AQL_EXECUTE("UPSERT @what INSERT { } UPDATE { 'c' : { c : 3 } } IN " + cn1 + " OPTIONS { mergeObjects: false } RETURN NEW", { what: { c: { a: 1, b: 2 } } });

      assertEqual(2, c1.count());
      assertEqual(1, actual.json.length);
      var doc = actual.json[0];
      assertTrue(doc.hasOwnProperty("c"));
      assertEqual({ c: 3 }, doc["c"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateEmpty: function () {
      c1.save({ "c" : { a: 1, b: 2 } });

      var actual = AQL_EXECUTE("UPSERT { 'c' : { a: 1, b: 2 } } INSERT { } UPDATE { } IN " + cn1 + " RETURN NEW", {});
      
      assertEqual(2, c1.count());
      assertEqual(1, actual.json.length);
      assertTrue(actual.json[0].hasOwnProperty("_key"));
      assertTrue(actual.json[0].hasOwnProperty("_id"));
      assertTrue(actual.json[0].hasOwnProperty("_rev"));
      assertTrue(actual.json[0].hasOwnProperty("c"));
      assertEqual(4, Object.keys(actual.json[0]).length);
      assertEqual({ a: 1, b: 2 }, actual.json[0].c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateEmptyBind: function () {
      c1.save({ "c" : { a: 1, b: 2 } });

      var actual = AQL_EXECUTE("UPSERT @what INSERT { } UPDATE { } IN " + cn1 + " RETURN NEW", { what: { c: { a: 1, b: 2 } } });
      
      assertEqual(2, c1.count());
      assertEqual(1, actual.json.length);
      assertTrue(actual.json[0].hasOwnProperty("_key"));
      assertTrue(actual.json[0].hasOwnProperty("_id"));
      assertTrue(actual.json[0].hasOwnProperty("_rev"));
      assertTrue(actual.json[0].hasOwnProperty("c"));
      assertEqual(4, Object.keys(actual.json[0]).length);
      assertEqual({ a: 1, b: 2 }, actual.json[0].c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsertEmpty: function () {
      var actual = AQL_EXECUTE("UPSERT { 'c' : { a: 1 } } INSERT { } UPDATE { } IN " + cn1 + " RETURN NEW", {});

      assertEqual(1, actual.json.length);
      assertTrue(actual.json[0].hasOwnProperty("_key"));
      assertTrue(actual.json[0].hasOwnProperty("_id"));
      assertTrue(actual.json[0].hasOwnProperty("_rev"));
      assertEqual(3, Object.keys(actual.json[0]).length);
      assertEqual(2, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsertEmptyBind: function () {
      var actual = AQL_EXECUTE("UPSERT @what INSERT { } UPDATE { } IN " + cn1 + " RETURN NEW", { what: { c: { a: 1 } } });

      assertEqual(1, actual.json.length);
      assertTrue(actual.json[0].hasOwnProperty("_key"));
      assertTrue(actual.json[0].hasOwnProperty("_id"));
      assertTrue(actual.json[0].hasOwnProperty("_rev"));
      assertEqual(3, Object.keys(actual.json[0]).length);
      assertEqual(2, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with non existing search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertNonExisting : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT { bart: 'bark' } INSERT { bar: 'baz', _key: 'testfoo' } UPDATE { bark: 'barx' } IN " + cn1, {});

      assertEqual(2, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.document("testfoo");
      assertEqual("testfoo", doc._key);
      assertFalse(doc.hasOwnProperty("a"));
      assertEqual("baz", doc.bar);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with non existing search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertNonExistingBind : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT @what INSERT { bar: 'baz', _key: 'testfoo' } UPDATE { bark: 'barx' } IN " + cn1, { what: { bart: "bark" } } );

      assertEqual(2, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      var doc = c1.document("testfoo");
      assertEqual("testfoo", doc._key);
      assertFalse(doc.hasOwnProperty("a"));
      assertEqual("baz", doc.bar);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertDynamicAttributes : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "UPSERT { [ 1 + 1 ] : true } INSERT { } UPDATE [ ] INTO @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInvalidSearchDocument : function () {
      c1.truncate({ compact: false });

      var queries = [
        [ "UPSERT 'test1' INSERT { } UPDATE { } INTO @@cn", { } ],
        [ "UPSERT null INSERT { } UPDATE { } INTO @@cn", { } ],
        [ "UPSERT false INSERT { } UPDATE { } INTO @@cn", { } ],
        [ "UPSERT true INSERT { } UPDATE { } INTO @@cn", { } ],
        [ "UPSERT [ ] INSERT { } UPDATE { } INTO @@cn", { } ],
        [ "UPSERT @val INSERT { } UPDATE { } INTO @@cn", { val: 'test1' } ],
        [ "UPSERT @val INSERT { } UPDATE { } INTO @@cn", { val: null } ],
        [ "UPSERT @val INSERT { } UPDATE { } INTO @@cn", { val: false } ],
        [ "UPSERT @val INSERT { } UPDATE { } INTO @@cn", { val: [ ] } ]
      ];

      queries.forEach(function(query) { 
        var params = query[1];
        params["@cn"] = cn1;
        assertQueryError(errors.ERROR_QUERY_PARSE.code, query[0], params, query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInvalidDocumentsInsert : function () {
      c1.truncate({ compact: false });

      var queries = [
        [ "UPSERT { } INSERT null UPDATE { } INTO @@cn", { } ],
        [ "UPSERT { } INSERT true UPDATE { } INTO @@cn", { } ],
        [ "UPSERT { } INSERT 'foo' UPDATE { } INTO @@cn", { } ],
        [ "UPSERT { } INSERT [ ] UPDATE { } INTO @@cn", { } ], 
        [ "UPSERT { } INSERT @val UPDATE { } INTO @@cn", { val: null } ], 
        [ "UPSERT { } INSERT @val UPDATE { } INTO @@cn", { val: true } ], 
        [ "UPSERT { } INSERT @val UPDATE { } INTO @@cn", { val: 'foo' } ],
        [ "UPSERT { } INSERT @val UPDATE { } INTO @@cn", { val: [ 1, 2 ] } ],
        [ "UPSERT { } INSERT @val UPDATE { } INTO @@cn", { val: [ ] } ]
      ];

      queries.forEach(function(query) { 
        var params = query[1];
        params["@cn"] = cn1;
        assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, query[0], params, query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInvalidDocumentsUpdate : function () {
      var queries = [
        [ "UPSERT { } INSERT { } UPDATE null INTO @@cn", { } ],
        [ "UPSERT { } INSERT { } UPDATE true INTO @@cn", { } ],
        [ "UPSERT { } INSERT { } UPDATE 'foo' INTO @@cn", { } ],
        [ "UPSERT { } INSERT { } UPDATE [ ] INTO @@cn", { } ], 
        [ "UPSERT { } INSERT { } UPDATE @val INTO @@cn", { val: null } ], 
        [ "UPSERT { } INSERT { } UPDATE @val INTO @@cn", { val: true } ], 
        [ "UPSERT { } INSERT { } UPDATE @val INTO @@cn", { val: 'foo' } ],
        [ "UPSERT { } INSERT { } UPDATE @val INTO @@cn", { val: [ 1, 2 ] } ],
        [ "UPSERT { } INSERT { } UPDATE @val INTO @@cn", { val: [ ] } ]
      ];

      queries.forEach(function(query) { 
        var params = query[1];
        params["@cn"] = cn1;
        assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, query[0], params, query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertFollowedByDocumentAccess : function () {
      var queries = [
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn RETURN DOCUMENT('foo')", 
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn RETURN PASSTHRU(DOCUMENT('foo'))", 
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn RETURN COLLECTIONS()", 
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn RETURN UNIQUE(@@cn)"
      ];

      queries.forEach(function(query) { 
        assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, query, { "@cn": cn1 }, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertFollowedBySubquery : function () {
      var queries = [
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn RETURN (FOR i IN @@cn RETURN i)", 
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn LET x = (FOR i IN @@cn RETURN i) RETURN x" 
      ];

      queries.forEach(function(query) { 
        assertQueryError(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, query, { "@cn": cn1 }, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert on same document
////////////////////////////////////////////////////////////////////////////////

    testUpsertDocumentInitiallyPresent : function () {
      AQL_EXECUTE("FOR i IN 0..1999 INSERT { _key: CONCAT('test', i), value1: i } IN @@cn1", { "@cn1" : cn1 });

      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT { value1: 0 } INSERT { value1: 0, value2: 0 } UPDATE { value2: 1 } IN " + cn1, {});

      assertEqual(2001, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      assertEqual(0, c1.document("test0").value1);
      assertEqual(1, c1.document("test0").value2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert on same document
////////////////////////////////////////////////////////////////////////////////

    testUpsertDocumentInitiallyNotPresent : function () {
      AQL_EXECUTE("FOR i IN 0..1999 INSERT { _key: CONCAT('test', i), value1: i } IN @@cn1", { "@cn1" : cn1 });

      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = AQL_EXECUTE("UPSERT { value1: 999999 } INSERT { _key: 'test999999', value1: 999999, value2: 0 } UPDATE { value2: 1 } IN " + cn1, {});

      assertEqual(2002, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      assertEqual(999999, c1.document("test999999").value1);
      assertEqual(0, c1.document("test999999").value2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert on same document
////////////////////////////////////////////////////////////////////////////////

    testUpsertDocumentRepeatedInitiallyPresent : function () {
      AQL_EXECUTE("FOR i IN 0..1999 INSERT { _key: CONCAT('test', i), value1: i } IN @@cn1", { "@cn1" : cn1 });

      var expected = { writesExecuted: 2000, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR i IN 1..2000 UPSERT { value1: 0 } INSERT { value1: 0, value2: 0 } UPDATE { value2: 1 } IN " + cn1, {});

      assertEqual(2001, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      assertEqual(0, c1.document("test0").value1);
      assertEqual(1, c1.document("test0").value2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert on same document
////////////////////////////////////////////////////////////////////////////////

    testUpsertDocumentRepeatedInitiallyNotPresent : function () {
      AQL_EXECUTE("FOR i IN 0..1999 INSERT { _key: CONCAT('test', i), value1: i } IN @@cn1", { "@cn1" : cn1 });

      var expected = { writesExecuted: 2000, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR i IN 1..2000 UPSERT { value1: 999999 } INSERT { _key: 'test999999', value1: 999999, value2: 0 } UPDATE { value2: 1 } IN " + cn1, {});

      assertEqual(2002, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      assertEqual(999999, c1.document("test999999").value1);
      assertEqual(1, c1.document("test999999").value2);
    }

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
      c1 = db._create(cn1);
      c2 = db._create(cn2);

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
      var actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 REMOVE d IN " + cn1, {});
    
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothingWhat : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 REMOVE d IN " + cn1 + " LET removed = OLD RETURN removed", {});

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveReturnDoc : function () {
      var actual = AQL_EXECUTE("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET removed = OLD RETURN OLD", {}).json;

      assertEqual(0, c1.count());
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        assertTrue(actual[i].hasOwnProperty("_key"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveReturnDocObjectLiteral : function () {
      var actual = AQL_EXECUTE("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET removed = OLD RETURN { OLD }", {}).json;

      assertEqual(0, c1.count());
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        var doc = actual[i];
        assertTrue(doc.hasOwnProperty("OLD"));
        assertTrue(doc.OLD.hasOwnProperty("_key"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveReturnKey : function () {
      var actual = AQL_EXECUTE("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET removed = OLD RETURN removed._key", {}).json;

      assertEqual(0, c1.count());
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        assertTrue(actual[i].startsWith("test"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveReturnCalculated : function () {
      var actual = AQL_EXECUTE("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET removed = OLD RETURN CONCAT('--', removed._key)", {}).json;

      assertEqual(0, c1.count());
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        assertTrue(actual[i].startsWith("--test"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveReturnMultiCalculated : function () {
      var actual = AQL_EXECUTE("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET a = CONCAT('--', OLD._key) LET b = CONCAT('--', a) LET c = CONCAT('xx', b) RETURN SUBSTRING(c, 4)", {}).json;

      assertEqual(0, c1.count());
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        assertTrue(actual[i].startsWith("--test"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothingBind : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 REMOVE d IN @@cn", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothingBindWhat : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 REMOVE d IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid1 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn REMOVE d.foobar IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid1What : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn REMOVE d.foobar IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });
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

    testRemoveInvalid4 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "FOR i iN 0..100 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore1 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE 'foo' IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore1What : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE 'foo' IN @@cn OPTIONS { ignoreErrors: true } LET removed = OLD RETURN removed", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore2 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 101 };
      var actual = getModifyQueryResults("FOR i IN 0..200 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore2What : function () {
      var expected = { writesExecuted: 100, writesIgnored: 101 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..200 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn OPTIONS { ignoreErrors: true } LET removed = OLD RETURN removed ", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(100, actual.json.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll1What : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE d IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(100, actual.json.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll2What : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE d._key IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(100, actual.json.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll3What : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE { _key: d._key } IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(100, actual.json.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll4 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 0..99 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll4What : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(100, actual.json.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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

    testRemoveAll5What : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE d INTO @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(100, actual.json.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveHalf : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 0..99 FILTER i % 2 == 0 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn1 });

      assertEqual(50, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveHalfWhat : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 FILTER i % 2 == 0 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(50, actual.json.length);
      assertEqual(50, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testSingleRemoveNotFound : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "REMOVE 'foobar' IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testSingleRemoveNotFoundWhat : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "REMOVE 'foobar' IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testSingle : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResults("REMOVE 'test0' IN @@cn", { "@cn": cn1 });

      assertEqual(99, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testSingleWhat : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("REMOVE 'test0' IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(1, actual.json.length);
      assertEqual(99, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsNotFoundWhat : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2 LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin1What : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn1 FILTER d.value1 < 50 REMOVE { _key: d._key } IN @@cn2 LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c2, actual.json);
      assertEqual(50, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin2What : function () {
      var expected = { writesExecuted: 48, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn1 FILTER d.value1 >= 2 && d.value1 < 50 REMOVE { _key: d._key } IN @@cn2 LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c2, actual.json);
      assertEqual(48, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(2, c2.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors1What : function () {
      var expected = { writesExecuted: 50, writesIgnored: 50 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2 OPTIONS { ignoreErrors: true } LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c2, actual.json);
      assertEqual(50, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors2What : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn1 REMOVE { _key: CONCAT('foo', d._key) } IN @@cn2 OPTIONS { ignoreErrors: true } LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(0, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(50, c2.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveWaitForSyncWhat : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE d IN @@cn OPTIONS { waitForSync: true } LET removed = OLD RETURN removed", { "@cn": cn1 });

      validateDocuments(actual.json, false);
      validateDeleteGone(c1, actual.json);
      assertEqual(100, actual.json.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
      var actual = getModifyQueryResults("FOR i IN 0..9 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn", { "@cn": edge.name() });

      assertEqual(100, c1.count());
      assertEqual(90, edge.count());
      assertEqual(expected, sanitizeStats(actual));
      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveEdgeWhat : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      for (var i = 0; i < 100; ++i) {
        edge.save("UnitTestsAhuacatlRemove1/foo" + i, "UnitTestsAhuacatlRemove2/bar", { what: i, _key: "test" + i });
      }
      var expected = { writesExecuted: 10, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..9 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn LET removed = OLD RETURN removed", { "@cn": edge.name() });

      validateDocuments(actual.json, true);
      validateDeleteGone(edge, actual.json);
      assertEqual(10, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(90, edge.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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
      c1 = db._create(cn1);
      c2 = db._create(cn2);

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
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertNothing : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 INSERT { foxx: true } IN " + cn1, {});
    
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertNothingWhat : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " FILTER d.value1 < 0 INSERT { foxx: true } IN " + cn1 + " LET inserted = NEW RETURN inserted", {});
    
      assertEqual(0, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturnDoc : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: true } IN " + cn1 + " RETURN NEW", {});
    
      assertEqual(100, actual.json.length);
      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < actual.json.length; ++i) {
        assertTrue(actual.json[i].hasOwnProperty("foxx"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturnObjectLiteral : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: true } IN " + cn1 + " RETURN { NEW }", {});
    
      assertEqual(100, actual.json.length);
      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < actual.json.length; ++i) {
        var doc = actual.json[i];
        assertTrue(doc.hasOwnProperty("NEW"));
        assertTrue(doc.NEW.hasOwnProperty("foxx"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturnKey : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: true } IN " + cn1 + " RETURN NEW._key", {});
    
      assertEqual(100, actual.json.length);
      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < actual.json.length; ++i) {
        assertTrue(typeof actual.json[i] === "string");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturnCalculated : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: 1, value: 42 } IN " + cn1 + " RETURN CONCAT(NEW.foxx, '/', NEW.value)", {});
    
      assertEqual(100, actual.json.length);
      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < actual.json.length; ++i) {
        assertEqual("1/42", actual.json[i]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturnMultiCalculated : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: 1, value: 42 } IN " + cn1 + " LET a = NEW.foxx LET b = NEW.value LET c = CONCAT(a, '-', b) RETURN c", {});
    
      assertEqual(100, actual.json.length);
      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < actual.json.length; ++i) {
        assertEqual("1-42", actual.json[i]);
      }
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

    testInsertNothingBindWhat : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d.value1 < 0 INSERT { foxx: true } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });

      assertEqual(0, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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

    testInsertInvalid1What : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn INSERT d.foobar IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });
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

    testInsertInvalid2What : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn INSERT [ ] IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertInvalid3 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn INSERT 'foo' IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertInvalid3What : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn INSERT 'foo' IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });
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

    testInsertUniqueConstraint1What : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn INSERT d IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueConstraint2 : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR i IN 0..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn2 });
      assertEqual(50, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueConstraint2What : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR i IN 0..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn2 });
      assertEqual(50, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueConstraint3 : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR i IN 0..50 INSERT { _key: 'foo' } IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueConstraint3What : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR i IN 0..50 INSERT { _key: 'foo' } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });
      assertEqual(100, c1.count());
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

    testInsertIgnore1What : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn INSERT d IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn1 });

      assertEqual(0, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore2 : function () {
      var expected = { writesExecuted: 1, writesIgnored: 50 };
      var actual = getModifyQueryResults("FOR i IN 50..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(101, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore2What : function () {
      var expected = { writesExecuted: 1, writesIgnored: 50 };
      var actual = getModifyQueryResultsRaw("FOR i IN 50..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn1 });

      assertEqual(1, actual.json.length);
      validateModifyResultInsert(c1, actual.json);
      validateDocuments(actual.json, false);

      assertEqual(101, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore3 : function () {
      var expected = { writesExecuted: 51, writesIgnored: 50 };
      var actual = getModifyQueryResults("FOR i IN 0..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn2 });

      assertEqual(101, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore3What : function () {
      var expected = { writesExecuted: 51, writesIgnored: 50 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn2 });

      validateModifyResultInsert(c2, actual.json);
      validateDocuments(actual.json, false);
      assertEqual(51, actual.json.length);
      assertEqual(101, c2.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore4 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResults("FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore4What : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn1 });

      assertEqual(0, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore5 : function () {
      var expected = { writesExecuted: 50, writesIgnored: 50 };
      var actual = getModifyQueryResults("FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn2 });

      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore5What : function () {
      var expected = { writesExecuted: 50, writesIgnored: 50 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn2 });

      validateModifyResultInsert(c2, actual.json);
      validateDocuments(actual.json, false);
      assertEqual(50, actual.json.length);
      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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

    testInsertEmptyWhat : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn INSERT { } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });

      validateModifyResultInsert(c1, actual.json);
      validateDocuments(actual.json, false);
      assertEqual(100, actual.json.length);
      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertCopy : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 50..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn2 });

      assertEqual(100, c1.count());
      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertCopyWhat : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 50..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn2 });

      validateModifyResultInsert(c2, actual.json);
      validateDocuments(actual.json, false);
      assertEqual(50, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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

    testSingleInsertWhat : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("INSERT { value: 'foobar', _key: 'test' } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });

      validateModifyResultInsert(c1, actual.json);
      validateDocuments(actual.json, false);
      assertEqual(1, actual.json.length);
      assertEqual(101, c1.count());
      assertEqual("foobar", c1.document("test").value);
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertWaitForSync : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 1..50 INSERT { value: i } INTO @@cn OPTIONS { waitForSync: true }", { "@cn": cn2 });

      assertEqual(100, c1.count());
      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertWaitForSyncWhat : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 1..50 INSERT { value: i } INTO @@cn OPTIONS { waitForSync: true } LET inserted = NEW RETURN inserted", { "@cn": cn2 });

      validateModifyResultInsert(c2, actual.json);
      validateDocuments(actual.json, false);
      assertEqual(50, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual.stats));
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

    testInsertEdgeInvalidWhat : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 INSERT { } INTO @@cn LET inserted = NEW RETURN inserted", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdgeNoFrom : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 INSERT { _to: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)) } INTO @@cn", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdgeNoFromWhat : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 INSERT { _to: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)) } INTO @@cn LET inserted = NEW RETURN inserted", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdgeNoTo : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 INSERT { _from: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)) } INTO @@cn", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdgeNoToWhat : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 INSERT { _from: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)) } INTO @@cn LET inserted = NEW RETURN inserted", { "@cn": edge.name() });
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
      var actual = getModifyQueryResults("FOR i IN 1..50 INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlInsert2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } INTO @@cn", { "@cn": edge.name() });

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

    testInsertEdgeWhat : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 1..50 INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlInsert2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } INTO @@cn LET inserted = NEW RETURN inserted", { "@cn": edge.name() });

      validateModifyResultInsert(edge, actual.json);
      validateDocuments(actual.json, true);
      assertEqual(50, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(50, edge.count());

      for (var i = 1; i <= 50; ++i) {
        var doc = edge.document("test" + i);
        assertEqual("UnitTestsAhuacatlInsert1/" + i, doc._from);
        assertEqual("UnitTestsAhuacatlInsert2/" + i, doc._to);
        assertEqual([ i ], doc.value);
        assertEqual({ foo: "bar" }, doc.sub);
      }
      db._drop("UnitTestsAhuacatlEdge");
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlUpdateSuite () {
  var errors = internal.errors;
  var cn1 = "UnitTestsAhuacatlUpdate1";
  var cn2 = "UnitTestsAhuacatlUpdate2";
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
      c1 = db._create(cn1);
      c2 = db._create(cn2);

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
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateNothing : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 UPDATE { foxx: true } IN " + cn1, {});
    
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

    testUpdateInvalid1 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn UPDATE d.foobar IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateInvalid2 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn UPDATE [ ] IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateInvalid3 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn UPDATE 'foo' IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateUniqueConstraint1 : function () {
      c1.ensureUniqueConstraint("value1");
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn UPDATE d._key WITH { value1: 1 } IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateUniqueConstraint2 : function () {
      c1.ensureUniqueConstraint("value3", { sparse: true });
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn UPDATE d._key WITH { value3: 1 } IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateIgnore1 : function () {
      c1.ensureUniqueConstraint("value3", { sparse: true });
      var expected = { writesExecuted: 1, writesIgnored: 99 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d WITH { value3: 1 } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateIgnore2 : function () {
      c1.ensureUniqueConstraint("value1", { sparse: true });
      var expected = { writesExecuted: 0, writesIgnored: 51 };
      var actual = getModifyQueryResults("FOR i IN 50..100 UPDATE { _key: CONCAT('test', TO_STRING(i)), value1: 1 } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

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

    testUpdateEmpty1WhatNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key } IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);
      var keyArray = wrapToKeys(actual.json);
      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual("test" + i, keyArray[doc._key].value2);
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
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateEmpty2WhatNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);
      var keyArray = wrapToKeys(actual.json);
      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual("test" + i, keyArray[doc._key].value2);
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

    testSingleUpdateNotFoundWhatNew : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "UPDATE { _key: 'foobar' } WITH { value1: 1 } IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });
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

    testSingleUpdateWhatNew : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("UPDATE { value: 'foobar', _key: 'test17' } IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual("foobar", c1.document("test17").value);
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testSingleUpdateWhatOld : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("UPDATE { value: 'foobar', _key: 'test17' } IN @@cn LET old = OLD RETURN old", { "@cn": cn1 });

      assertFalse(actual.json[0].hasOwnProperty('foobar'));
      assertEqual("test17", actual.json[0]._key);
      assertEqual(17, actual.json[0].value1);

      assertEqual("foobar", c1.document("test17").value);
      assertEqual(17, c1.document("test17").value1);
      assertEqual(expected, sanitizeStats(actual.stats));
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

    testUpdateReturnOld : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn RETURN OLD", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual(doc._key.substr(4), doc.value1);
        assertEqual(doc._key, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnOldKey : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn RETURN OLD._key", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        assertEqual("test", actual.json[i].substr(0, 4));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnCalculated : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn RETURN { newValue: NEW.value2, newKey: NEW._key }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        assertEqual("overwrite!", actual.json[i].newValue);
        assertEqual("test", actual.json[i].newKey.substr(0, 4));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnMultiCalculated : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn LET newValue = NEW.value2 LET newKey = NEW._key RETURN { newValue: newValue, newKey: newKey }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        assertEqual("overwrite!", actual.json[i].newValue);
        assertEqual("test", actual.json[i].newKey.substr(0, 4));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn RETURN NEW", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual(doc._key.substr(4), doc.value1);
        assertEqual("overwrite!", doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithNothing : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPDATE d WITH { } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertEqual(i, doc["new"].value1);
        assertEqual("test" + i, doc.old.value2);
        assertEqual("test" + i, doc["new"].value2);
        assertEqual(doc.old._rev, doc["new"]._rev); // _rev should not have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithNothingReturnObjectLiteral : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPDATE d WITH { } IN @@cn RETURN { OLD, NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.OLD._key);
        assertEqual("test" + i, doc.NEW._key);
        assertEqual(i, doc.OLD.value1);
        assertEqual(i, doc.NEW.value1);
        assertEqual("test" + i, doc.OLD.value2);
        assertEqual("test" + i, doc.NEW.value2);
        assertEqual(doc.OLD._rev, doc.NEW._rev); // _rev should not have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithSomething : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPDATE d WITH { value1: d.value1, value2: d.value2 } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertEqual(i, doc["new"].value1);
        assertEqual("test" + i, doc.old.value2);
        assertEqual("test" + i, doc["new"].value2);
        assertNotEqual(doc.old._rev, doc["new"]._rev); // _rev should have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithSomethingReturnObjectLiteral : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPDATE d WITH { value1: d.value1, value2: d.value2 } IN @@cn RETURN { OLD, NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.OLD._key);
        assertEqual("test" + i, doc.NEW._key);
        assertEqual(i, doc.OLD.value1);
        assertEqual(i, doc.NEW.value1);
        assertEqual("test" + i, doc.OLD.value2);
        assertEqual("test" + i, doc.NEW.value2);
        assertNotEqual(doc.OLD._rev, doc.NEW._rev); // _rev should have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateOldValueWhatNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value1: d.value2, value2: d.value1, value3: d.value1 + 5 } IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);
      var keyArray = wrapToKeys(actual.json);

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual("test" + i, doc.value1);
        assertEqual(i, doc.value2);
        assertEqual(i + 5, doc.value3);

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual("test" + i, keyArray[doc._key].value1);
        assertEqual(i, keyArray[doc._key].value2);
        assertEqual(i + 5, keyArray[doc._key].value3);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateOldValueWhatOld : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value1: d.value2, value2: d.value1, value3: d.value1 + 5 } IN @@cn LET old = OLD RETURN old", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);
      var keyArray = wrapToKeys(actual.json);

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual("test" + i, doc.value1);
        assertEqual(i, doc.value2);
        assertEqual(i + 5, doc.value3);

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual(i, keyArray[doc._key].value1);
        assertEqual("test" + i, keyArray[doc._key].value2);
        assertFalse(keyArray[doc._key].hasOwnProperty("value3"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWaitForSync : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 1..50 UPDATE { _key: CONCAT('test', TO_STRING(i)) } INTO @@cn OPTIONS { waitForSync: true }", { "@cn": cn1 });
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

    testUpdateWaitForSyncWhatNew : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 1..50 UPDATE { _key: CONCAT('test', TO_STRING(i)) } INTO @@cn OPTIONS { waitForSync: true } LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(50, actual.json.length);
      var keyArray = wrapToKeys(actual.json);
      var count = 0;

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);

        /// only compare the updated documents
        if (keyArray.hasOwnProperty(doc._key)){
          assertEqual("test" + i, keyArray[doc._key].value2);
          count++;
        }
      }
      assertEqual(50, count);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullDefault : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null, a: { b: null } } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertNull(doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertTrue(doc.hasOwnProperty("value9"));
        assertNull(doc.value9);
        assertTrue(doc.hasOwnProperty("a"));
        assertNull(doc.a.b);
        assertTrue(doc.a.hasOwnProperty("b"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullDefaultWhatNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);
      var keyArray = wrapToKeys(actual.json);

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertNull(doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertTrue(doc.hasOwnProperty("value9"));
        assertNull(doc.value9);

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual("test" + i, keyArray[doc._key].value2);
        assertEqual("foobar", keyArray[doc._key].value3);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullTrue : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null, a: { b: null } } INTO @@cn OPTIONS { keepNull: true }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertNull(doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertTrue(doc.hasOwnProperty("value9"));
        assertNull(doc.value9);
        assertTrue(doc.hasOwnProperty("a"));
        assertNull(doc.a.b);
        assertTrue(doc.a.hasOwnProperty("b"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullTrueWhatNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn OPTIONS { keepNull: true } LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);
      var keyArray = wrapToKeys(actual.json);

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertNull(doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertTrue(doc.hasOwnProperty("value9"));
        assertNull(doc.value9);

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual("test" + i, keyArray[doc._key].value2);
        assertEqual("foobar", keyArray[doc._key].value3);
        assertNull(keyArray[doc._key].value9);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullFalse : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null, a: { b: null } } INTO @@cn OPTIONS { keepNull: false }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertFalse(doc.hasOwnProperty("value9"));
        assertTrue(doc.hasOwnProperty("a"));
        assertEqual({}, doc.a);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullFalseWhatNew : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null, a: { b: null } } INTO @@cn OPTIONS { keepNull: false } LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);
      var keyArray = wrapToKeys(actual.json);

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertFalse(doc.hasOwnProperty("value9"));
        assertTrue(doc.hasOwnProperty("a"));
        assertEqual({}, doc.a);

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual("test" + i, keyArray[doc._key].value2);
        assertEqual("foobar", keyArray[doc._key].value3);
        assertFalse(keyArray[doc._key].hasOwnProperty('value9'));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullFalseWhatOld : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn OPTIONS { keepNull: false } LET updated = OLD RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);
      var keyArray = wrapToKeys(actual.json);

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertFalse(doc.hasOwnProperty("value9"));

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual(i, keyArray[doc._key].value1);
        assertEqual("test" + i, keyArray[doc._key].value2);
        assertFalse(keyArray[doc._key].hasOwnProperty('value3'));
        assertFalse(keyArray[doc._key].hasOwnProperty('value9'));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update with search document
////////////////////////////////////////////////////////////////////////////////

    testUpdateSubKeepNullFalse : function () {
      let expected = { writesExecuted: 1, writesIgnored: 0 };
      c1.truncate({ compact: false });
      c1.insert({ _key: "foo" });

      // Patch non-existing substructure:
      var q = `FOR doc IN ${cn1}
  UPDATE doc WITH { foo: {
 bark: 'bart',
 foxx: null,
 a: null }}
 IN ${cn1} OPTIONS { keepNull: false }
 RETURN NEW`;
      var actual = AQL_EXECUTE(q, {});
      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertTrue(actual.json[0].hasOwnProperty("foo"));
      assertFalse(actual.json[0].foo.hasOwnProperty("a"));
      assertFalse(actual.json[0].foo.hasOwnProperty("foxx"));
      assertTrue(actual.json[0].foo.hasOwnProperty("bark"));
      assertEqual("bart", actual.json[0].foo.bark);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertTrue(doc.hasOwnProperty("foo"));
      assertFalse(doc.foo.hasOwnProperty("a"));
      assertFalse(doc.foo.hasOwnProperty("foxx"));
      assertEqual("bart", doc.foo.bark);

      actual = AQL_EXECUTE(q, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertTrue(doc.hasOwnProperty("foo"));
      assertFalse(doc.foo.hasOwnProperty("a"));
      assertFalse(doc.foo.hasOwnProperty("foxx"));
      assertEqual("bart", doc.foo.bark);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update with search document
////////////////////////////////////////////////////////////////////////////////

    testUpdateSubKeepNullArrayFalse : function () {
      let expected = { writesExecuted: 1, writesIgnored: 0 };
      c1.truncate({ compact: false });
      c1.insert({ _key: "foo" });

      // Patch non-existing substructure:
      var q = `FOR doc IN ${cn1}
  UPDATE doc WITH { foo: [{
 bark: 'bart',
 foxx: null,
 a: null },
null,
"abc",
false
]}
 IN ${cn1} OPTIONS { keepNull: false }
 RETURN NEW`;
      var actual = AQL_EXECUTE(q, {});
      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertTrue(actual.json[0].hasOwnProperty("foo"));
      assertFalse(actual.json[0].foo.hasOwnProperty("a"));
      assertFalse(actual.json[0].foo.hasOwnProperty("foxx"));
      assertFalse(actual.json[0].foo.hasOwnProperty("bark"));
      assertEqual("bart", actual.json[0].foo[0].bark);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertTrue(doc.hasOwnProperty("foo"));
      assertFalse(doc.foo.hasOwnProperty("a"));
      assertFalse(doc.foo.hasOwnProperty("foxx"));
      assertEqual("bart", doc.foo[0].bark);

      actual = AQL_EXECUTE(q, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));

      doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertTrue(doc.hasOwnProperty("foo"));
      assertFalse(doc.foo.hasOwnProperty("a"));
      assertFalse(doc.foo.hasOwnProperty("foxx"));
      assertEqual("bart", doc.foo[0].bark);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsDefault : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      var doc = c1.document("something");
      assertEqual({ foo: 1, bar: 42, baz: 3, bumm : 23 }, doc.values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsDefaultWhatNew : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(1, actual.json.length);
      
      var doc = c1.document("something");
      assertEqual({ foo: 1, bar: 42, baz: 3, bumm : 23 }, doc.values);
      assertTrue(isEqual(doc, actual.json[0]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsTrue : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn OPTIONS { mergeObjects: true }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));
      
      var doc = c1.document("something");
      assertEqual({ foo: 1, bar: 42, baz: 3, bumm : 23 }, doc.values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsTrueWhatNew : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn OPTIONS { mergeObjects: true } LET updated = NEW RETURN updated", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(1, actual.json.length);
      
      var doc = c1.document("something");
      assertEqual({ foo: 1, bar: 42, baz: 3, bumm : 23 }, doc.values);
      assertTrue(isEqual(doc, actual.json[0]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsFalse : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn OPTIONS { mergeObjects: false }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));
      
      var doc = c1.document("something");
      assertEqual({ bar: 42, bumm: 23 }, doc.values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsFalseWhatNew : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn OPTIONS { mergeObjects: false } LET updated = NEW RETURN updated", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(1, actual.json.length);
      
      var doc = c1.document("something");
      assertEqual({ bar: 42, bumm: 23 }, doc.values);
      assertTrue(isEqual(doc, actual.json[0]));
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

    testUpdateFilterWhatNew : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d.value1 % 2 == 0 UPDATE d._key WITH { value2: 100 } INTO @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(50, actual.json.length);
      var keyArray = wrapToKeys(actual.json);
      var count = 0;

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        if (i % 2 === 0) {
          assertEqual(100, doc.value2);
        }
        else {
          assertEqual("test" + i, doc.value2);
        }

        if (keyArray.hasOwnProperty(doc._key)) {
          count++;
          assertTrue(isEqual(doc, keyArray[doc._key]));
        }
      }
      assertEqual(50, count);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateFilterWhatOld : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d.value1 % 2 == 0 UPDATE d._key WITH { value2: 100 } INTO @@cn LET updated = OLD RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(50, actual.json.length);
      var keyArray = wrapToKeys(actual.json);
      var count = 0;

      for (var i = 0; i < 100; i += 2) {
        var doc = c1.document("test" + i);
        assertEqual(100, doc.value2);
        count++;
        assertEqual(i, keyArray[doc._key].value1);
      }
      assertEqual(50, count);
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

    testUpdateUpdateWhatNew : function () {
      var i, j;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual=[];

      for (j = 0; j < 5; ++j) {
        actual[j] = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { counter: HAS(d, 'counter') ? d.counter + 1 : 1 } INTO @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual[j].stats));
        assertEqual(100, actual[j].json.length);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(5, doc.counter);
      }

      for (j = 0; j < 5; ++j) {
        for (i = 0; i < 100; ++i) {
          assertEqual(j + 1, actual[j].json[i].counter);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace1 : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        var actual = getModifyQueryResults("FOR d IN @@cn REPLACE d._key WITH { value4: 12 } INTO @@cn", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual));
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertFalse(doc.hasOwnProperty("value3"));
        assertEqual(12, doc.value4);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace1WhatNew : function () {
      var i, j;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual=[];
      for (j = 0; j < 5; ++j) {
        actual[j] = getModifyQueryResultsRaw("FOR d IN @@cn REPLACE d._key WITH { value4: 12 } INTO @@cn LET replaced = NEW RETURN replaced", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual[j].stats));
        assertEqual(100, actual[j].json.length);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertFalse(doc.hasOwnProperty("value3"));
        assertEqual(12, doc.value4);
      }
      for (j = 0; j < 5; ++j) {
        for (i = 0; i < 100; ++i) {
          assertFalse(actual[j].json[i].hasOwnProperty("value1"));
          assertFalse(actual[j].json[i].hasOwnProperty("value2"));
          assertFalse(actual[j].json[i].hasOwnProperty("value3"));
          assertEqual(12, actual[j].json[i].value4);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace2 : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        var actual = getModifyQueryResults("FOR d IN @@cn REPLACE { _key: d._key, value4: 13 } INTO @@cn", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual));
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertFalse(doc.hasOwnProperty("value3"));
        assertEqual(13, doc.value4);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace2WhatNew : function () {
      var i, j;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual=[];
      for (j = 0; j < 5; ++j) {
        actual[j] = getModifyQueryResultsRaw("FOR d IN @@cn REPLACE { _key: d._key, value4: " + j + " } INTO @@cn LET replaced = NEW RETURN replaced", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual[j].stats));
        assertEqual(100, actual[j].json.length);
      }
      
      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertFalse(doc.hasOwnProperty("value3"));
        assertEqual(4, doc.value4);
      }

      for (j = 0; j < 5; ++j) {
        for (i = 0; i < 100; ++i) {
          assertFalse(actual[j].json[i].hasOwnProperty("value1"));
          assertFalse(actual[j].json[i].hasOwnProperty("value2"));
          assertFalse(actual[j].json[i].hasOwnProperty("value3"));
          assertEqual(j, actual[j].json[i].value4);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceReplace : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        var actual = getModifyQueryResults("FOR d IN @@cn REPLACE d._key WITH { value1: d.value1 + 1 } INTO @@cn", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual));
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i + 5, doc.value1);
        assertFalse(doc.hasOwnProperty("value2"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceReplaceWhatNew : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        var actual = getModifyQueryResultsRaw("FOR d IN @@cn REPLACE d._key WITH { value1: d.value1 + 1 } INTO @@cn LET replaced = NEW RETURN replaced", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual.stats));
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i + 5, doc.value1);
        assertFalse(doc.hasOwnProperty("value2"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceReturnOld : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = AQL_EXECUTE("FOR d IN @@cn REPLACE d WITH { value3: d.value1 + 5 } IN @@cn LET previous = OLD RETURN previous", { "@cn": cn1 });

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
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateWithNothing : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } UPDATE { } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertEqual(i, doc["new"].value1);
        assertEqual("test" + i, doc.old.value2);
        assertEqual("test" + i, doc["new"].value2);
        assertEqual(doc.old._rev, doc["new"]._rev); // _rev should not have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateWithNothingReturnObjectLiteral : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } UPDATE { } IN @@cn RETURN { OLD, NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.OLD._key);
        assertEqual("test" + i, doc.NEW._key);
        assertEqual(i, doc.OLD.value1);
        assertEqual(i, doc.NEW.value1);
        assertEqual("test" + i, doc.OLD.value2);
        assertEqual("test" + i, doc.NEW.value2);
        assertEqual(doc.OLD._rev, doc.NEW._rev); // _rev should not have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReplaceWithNothing : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } REPLACE { } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertFalse(doc["new"].hasOwnProperty("value1"));
        assertEqual("test" + i, doc.old.value2);
        assertFalse(doc["new"].hasOwnProperty("value2"));
        assertNotEqual(doc.old._rev, doc["new"]._rev); // _rev should have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateWithSomething : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } UPDATE { value1: OLD.value1, value2: OLD.value2 } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertEqual(i, doc["new"].value1);
        assertEqual("test" + i, doc.old.value2);
        assertEqual("test" + i, doc["new"].value2);
        assertNotEqual(doc.old._rev, doc["new"]._rev); // _rev should have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReplaceWithSomething : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } REPLACE { value1: OLD.value1, value2: OLD.value2, value3: OLD.value1 } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, actual.json.length);

      for (var i = 0; i < 100; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertEqual(i, doc["new"].value1);
        assertEqual("test" + i, doc.old.value2);
        assertEqual("test" + i, doc["new"].value2);
        assertEqual(i, doc["new"].value3);
        assertNotEqual(doc.old._rev, doc["new"]._rev); // _rev should have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertCopy : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR doc IN @@cn1 UPSERT { _key: doc._key } INSERT { _key: doc._key, value1: doc.value1, value2: doc.value2, new: true } UPDATE { new: false } INTO @@cn2", { "@cn1": cn1, "@cn2": cn2 });
      assertEqual(expected, sanitizeStats(actual.stats));

      for (i = 0; i < 100; ++i) {
        var doc = c2.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
        if (i < 50) {
          assertFalse(doc["new"]);
        }
        else {
          assertTrue(doc["new"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsertUniqueConstraint1 : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR i IN 1..100 UPSERT { notHere: true } INSERT { _key: 'test' } UPDATE { } IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsertUniqueConstraint2 : function () {
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn UPSERT { notHere: true } INSERT d UPDATE { } IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsertUniqueConstraintIgnore : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR d IN @@cn UPSERT { } INSERT d UPDATE { } IN @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsert : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { eule: i } INSERT { _key: CONCAT('owl', i), x: i } UPDATE { y: i } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.stats));

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("owl" + i);
        assertEqual(i, doc.x);
        assertFalse(doc.hasOwnProperty("y"));
        assertFalse(doc.hasOwnProperty("eule"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdate : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('owl', i), x: i } UPDATE { y: i } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.y);
        assertFalse(doc.hasOwnProperty("x"));
        assertTrue(doc.hasOwnProperty("value1"));
        assertTrue(doc.hasOwnProperty("value2"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReplace : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('owl', i), x: i } REPLACE { y: i } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.y);
        assertFalse(doc.hasOwnProperty("x"));
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertMixed : function () {
      var expected = { writesExecuted: 200, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..199 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), x: i } UPDATE { y: i } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < 200; ++i) {
        var doc = c1.document("test" + i);
        if (i <= 99) {
          assertEqual(i, doc.y);
          assertFalse(doc.hasOwnProperty("x"));
          assertTrue(doc.hasOwnProperty("value1"));
          assertTrue(doc.hasOwnProperty("value2"));
        }
        else {
          assertEqual(i, doc.x);
          assertFalse(doc.hasOwnProperty("y"));
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReferToOld : function () {
      var expected = { writesExecuted: 200, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 0..199 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.stats));

      for (var i = 0; i < 200; ++i) {
        var doc = c1.document("test" + i);
        if (i <= 99) {
          assertFalse(doc.hasOwnProperty("x"));
          assertEqual(i * 2, doc.value1);
          assertFalse(doc["new"]);
          assertTrue(doc.hasOwnProperty("value2"));
        }
        else {
          assertFalse(doc.hasOwnProperty("y"));
          assertTrue(doc["new"]);
          assertFalse(doc.hasOwnProperty("value2"));
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnOld : function () {
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN { value: OLD.value1, doc: OLD }", { "@cn": cn1 });

      for (var i = 0; i < actual.json.length; ++i) {
        var doc = actual.json[i];
        assertEqual(i, doc.value);
        assertEqual(i, doc.doc.value1);
        assertUndefined(doc["new"]);
        assertFalse(doc.doc["new"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnNew : function () {
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN { value: OLD.value1, doc: NEW }", { "@cn": cn1 });

      for (var i = 0; i < actual.json.length; ++i) {
        var doc = actual.json[i];
        assertEqual(i, doc.value);
        assertEqual(i * 2, doc.doc.value1);
        assertUndefined(doc["new"]);
        assertFalse(doc.doc["new"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnOldEmpty : function () {
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('floxx', i) } INSERT { new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN OLD", { "@cn": cn1 });

      for (var i = 0; i < actual.json.length; ++i) {
        assertNull(actual.json[i]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnNewEmpty : function () {
      var actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test' , i), new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN NEW", { "@cn": cn1 });

      for (var i = 0; i < actual.json.length; ++i) {
        var doc = actual.json[i];
        assertEqual(i * 2, doc.value1);
        assertFalse(doc["new"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnOldNew : function () {
      var actual = getModifyQueryResultsRaw("FOR i IN 0..199 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), new: true, value1: 4 * i } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN { value: i, old: OLD, new: NEW }", { "@cn": cn1 });

      for (var i = 0; i < actual.json.length; ++i) {
        var doc = actual.json[i];
        assertEqual("test" + i, doc.new._key);

        if (doc.value <= 99) {
          assertEqual(i, doc["old"].value1);
          assertFalse(doc["new"]["new"]);
          assertNotNull(doc["old"]);
          assertEqual(i * 2, doc["new"].value1);
        }
        else {
          assertEqual(4 * i, doc["new"].value1);
          assertTrue(doc["new"]["new"]);
          assertNull(doc["old"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsertKey : function () {
      var actual = getModifyQueryResultsRaw("UPSERT { _key: 'quux' } INSERT { _key: 'f0xx' } UPDATE { gotIt : true } INTO @@cn RETURN NEW", { "@cn": cn1 });

      assertEqual(1, actual.json.length);
      var doc = actual.json[0];
      assertEqual("f0xx", doc._key);
      assertEqual([ "_id", "_key", "_rev" ], Object.keys(doc).sort()); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertFilterConditionHit1 : function () {
      c1.save({ foo: { bar: { baz: "a" }, foobar: [ 1, 2 ] } });

      var actual = getModifyQueryResultsRaw("UPSERT { foo: { bar: { baz: 'a' } } } INSERT { _key: 'f0xx' } UPDATE { gotIt : true } INTO @@cn RETURN NEW", { "@cn": cn1 });

      assertEqual(1, actual.json.length);
      var doc = actual.json[0];
      assertTrue(doc.hasOwnProperty("gotIt"));
      assertTrue(doc.gotIt);
      assertEqual("a", doc.foo.bar.baz);
      assertEqual([ 1, 2 ], doc.foo.foobar);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertFilterConditionHit2 : function () {
      c1.save({ foo: { bar: { baz: "a" }, foobar: [ 1, 2 ] } });

      var actual = getModifyQueryResultsRaw("UPSERT { foo: { foobar: [ 1, 2 ], bar: { baz: 'a' } } } INSERT { _key: 'f0xx' } UPDATE { gotIt : true } INTO @@cn RETURN NEW", { "@cn": cn1 });

      assertEqual(1, actual.json.length);
      var doc = actual.json[0];
      assertTrue(doc.hasOwnProperty("gotIt"));
      assertTrue(doc.gotIt);
      assertEqual("a", doc.foo.bar.baz);
      assertEqual([ 1, 2 ], doc.foo.foobar);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertFilterConditionMiss : function () {
      c1.save({ foo: { bar: { bazz: "a" }, foobar: [ 1, 2 ] } });

      var actual = getModifyQueryResultsRaw("UPSERT { foo: { bar: { baz: 'a' } } } INSERT { _key: 'f0xx' } UPDATE { gotIt : true } INTO @@cn RETURN NEW", { "@cn": cn1 });

      assertEqual(1, actual.json.length);
      var doc = actual.json[0];
      assertFalse(doc.hasOwnProperty("gotIt"));
      assertFalse(doc.hasOwnProperty("foo"));
      assertEqual("f0xx", doc._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnConditional : function () {
      var actual = getModifyQueryResultsRaw("FOR i IN 98..100 LET a = PASSTHRU(CONCAT('test', i)) UPSERT { _key: a, value1: i, value2: a } INSERT { _key: a, wasInserted: true } UPDATE { value5: OLD.value1 + 1 } INTO @@cn RETURN { doc: NEW, type: OLD ? 'update' : 'insert'}", { "@cn": cn1 });

      assertEqual(3, actual.json.length);
      var doc = actual.json[0];
      assertEqual("update", doc.type);
      assertEqual("test98", doc.doc._key);
      assertEqual(98, doc.doc.value1);
      assertEqual("test98", doc.doc.value2);
      assertEqual(99, doc.doc.value5);

      doc = actual.json[1];
      assertEqual("update", doc.type);
      assertEqual("test99", doc.doc._key);
      assertEqual(99, doc.doc.value1);
      assertEqual("test99", doc.doc.value2);
      assertEqual(100, doc.doc.value5);
      
      doc = actual.json[2];
      assertEqual("insert", doc.type);
      assertEqual("test100", doc.doc._key);
      assertFalse(doc.doc.hasOwnProperty("value1"));
      assertFalse(doc.doc.hasOwnProperty("value2"));
      assertFalse(doc.doc.hasOwnProperty("value5"));
      assertTrue(doc.doc.wasInserted);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertBind : function () {
      var insert = { 
        foo: "bar",
        baz: [ true, 1, null, [ { bar: "baz" } ] ]
      };
      
      var update = { 
        boo: "far",
        value2: { "foo": { "bar" : 1 } } 
      };

      var actual = getModifyQueryResultsRaw("FOR i IN 98..100 LET a = CONCAT('test', i) UPSERT { _key: a, value1: i, value2: a } INSERT @insert UPDATE @update INTO @@cn RETURN { doc: NEW, type: OLD ? 'update' : 'insert'}", { "@cn": cn1, insert: insert, update: update });

      assertEqual(3, actual.json.length);
      var doc = actual.json[0];
      assertEqual("update", doc.type);
      assertEqual("test98", doc.doc._key);
      assertEqual(98, doc.doc.value1);
      assertEqual("far", doc.doc.boo);
      assertEqual({ foo: { bar: 1 } }, doc.doc.value2);

      doc = actual.json[1];
      assertEqual("update", doc.type);
      assertEqual("test99", doc.doc._key);
      assertEqual(99, doc.doc.value1);
      assertEqual("far", doc.doc.boo);
      assertEqual({ foo: { bar: 1 } }, doc.doc.value2);
      
      doc = actual.json[2];
      assertEqual("insert", doc.type);
      assertEqual("bar", doc.doc.foo);
      assertEqual([ true, 1, null, [ { bar: "baz" } ] ], doc.doc.baz);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertEdgeInvalid : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 UPSERT { foo: 1 } INSERT { foo: 'bar'} UPDATE { } INTO @@cn", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertEdgeNoFrom : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 UPSERT { foo: 1 } INSERT { _to: CONCAT('UnitTestsAhuacatlUpdate1/', TO_STRING(i)) } UPDATE { } INTO @@cn", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertEdgeNoTo : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      assertQueryError(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, "FOR i IN 1..50 UPSERT { foo: 1 } INSERT { _from: CONCAT('UnitTestsAhuacatlUpdate1/', TO_STRING(i)) } UPDATE { } INTO @@cn", { "@cn": edge.name() });
      assertEqual(0, edge.count());

      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertEdge : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResults("FOR i IN 1..50 UPSERT { foo: 1 } INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlUpdate1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlUpdate2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } UPDATE { } INTO @@cn", { "@cn": edge.name() });

      assertEqual(expected, sanitizeStats(actual));
      assertEqual(50, edge.count());

      for (var i = 1; i <= 50; ++i) {
        var doc = edge.document("test" + i);
        assertEqual("UnitTestsAhuacatlUpdate1/" + i, doc._from);
        assertEqual("UnitTestsAhuacatlUpdate2/" + i, doc._to);
        assertEqual([ i ], doc.value);
        assertEqual({ foo: "bar" }, doc.sub);
      }
      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertEdgeWhat : function () {
      db._drop("UnitTestsAhuacatlEdge");
      var edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var actual = getModifyQueryResultsRaw("FOR i IN 1..50 UPSERT { foo: 1 } INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlUpdate1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlUpdate2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } UPDATE { } INTO @@cn LET inserted = NEW RETURN inserted", { "@cn": edge.name() });

      validateModifyResultInsert(edge, actual.json);
      validateDocuments(actual.json, true);
      assertEqual(50, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual(50, edge.count());

      for (var i = 1; i <= 50; ++i) {
        var doc = edge.document("test" + i);
        assertEqual("UnitTestsAhuacatlUpdate1/" + i, doc._from);
        assertEqual("UnitTestsAhuacatlUpdate2/" + i, doc._to);
        assertEqual([ i ], doc.value);
        assertEqual({ foo: "bar" }, doc.sub);
      }
      db._drop("UnitTestsAhuacatlEdge");
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

