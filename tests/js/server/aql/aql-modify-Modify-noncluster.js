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

const internal = require("internal");
const db = require("@arangodb").db;
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getModifyQueryResults = helper.getModifyQueryResults;
const getModifyQueryResultsRaw = helper.getModifyQueryResultsRaw;
const isEqual = helper.isEqual;
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;

let sanitizeStats = function (stats) {
  // remove these members from the stats because they don't matter
  // for the comparisons
  delete stats.scannedFull;
  delete stats.scannedIndex;
  delete stats.cursorsCreated;
  delete stats.cursorsRearmed;
  delete stats.cacheHits;
  delete stats.cacheMisses;
  delete stats.filtered;
  delete stats.executionTime;
  delete stats.httpRequests;
  delete stats.fullCount;
  delete stats.peakMemoryUsage;
  delete stats.intermediateCommits;
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
  const cn1 = "UnitTestsAhuacatlModify1";
  const cn2 = "UnitTestsAhuacatlModify2";
  const cn3 = "UnitTestsAhuacatlModify3";

  let c1, c2, c3;

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

      c1.save({ _key: "foo", a: 1 });
      c2.save({ _key: "foo", b: 1 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({name: `test${i}`});
      }
      c3.insert(docs);
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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("FOR d IN " + cn1 + " REMOVE d IN " + cn1, {});
    
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty results
////////////////////////////////////////////////////////////////////////////////

    testEmptyResultInsert : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("INSERT { _key: 'baz' } IN " + cn1, {});

      assertEqual(2, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty results
////////////////////////////////////////////////////////////////////////////////

    testEmptyResultUpdate : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPDATE { _key: 'foo' } WITH { baz: true } IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty results
////////////////////////////////////////////////////////////////////////////////

    testEmptyResultReplace : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("REPLACE { _key: 'foo' } WITH { baz: true } IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD/NEW variables
////////////////////////////////////////////////////////////////////////////////

    testOldNewUsageRemove : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("FOR OLD IN " + cn1 + " LET NEW = OLD REMOVE NEW IN " + cn1, {});

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD/NEW variables
////////////////////////////////////////////////////////////////////////////////

    testOldNewUsageInsert : function () {
      const expected = { writesExecuted: 0, writesIgnored: 1 };
      const actual = AQL_EXECUTE("FOR OLD IN " + cn1 + " LET NEW = OLD INSERT NEW IN " + cn1 + " OPTIONS { ignoreErrors: true }", {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD/NEW variables
////////////////////////////////////////////////////////////////////////////////

    testOldNewUsageUpdate : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("FOR OLD IN " + cn1 + " LET NEW = OLD UPDATE NEW WITH NEW IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage of OLD/NEW variables
////////////////////////////////////////////////////////////////////////////////

    testOldNewUsageReplace : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("FOR OLD IN " + cn1 + " LET NEW = OLD REPLACE NEW WITH NEW IN " + cn1, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertEmptyCollection : function () {
      c1.truncate({ compact: false });

      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } UPDATE { bark: 'bart' } IN " + cn1, {});

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

      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT @what INSERT { bar: 'baz' } UPDATE { bark: 'bart' } IN " + cn1, { what: { } });

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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } UPDATE { bark: 'bart' } IN " + cn1, {});

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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT @what INSERT { bar: 'baz' } UPDATE { bark: 'bart' } IN " + cn1, { what: { } });

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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } REPLACE { bark: 'bart' } IN " + cn1, {});

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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } UPDATE { bark: 'bart', foxx: null, a: null } IN " + cn1 + " OPTIONS { keepNull: true }", {});

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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT { } INSERT { bar: 'baz' } UPDATE { bark: 'bart', foxx: null, a: null } IN " + cn1 + " OPTIONS { keepNull: false }", {});

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

      const actual = AQL_EXECUTE("UPSERT { `c` : { a: 1, b: 2 } } INSERT { } UPDATE { `c` : { c : 3 } } IN " + cn1 + " OPTIONS { mergeObjects: true } RETURN NEW", {});
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

      const actual = AQL_EXECUTE("UPSERT @what INSERT { } UPDATE { `c` : { c : 3 } } IN " + cn1 + " OPTIONS { mergeObjects: true } RETURN NEW", { what: { c: { a: 1, b: 2 } } });
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

      const actual = AQL_EXECUTE("UPSERT { 'c' : { a: 1, b: 2 } } INSERT { } UPDATE { 'c' : { c : 3 } } IN " + cn1 + " OPTIONS { mergeObjects: false } RETURN NEW", {});

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

      const actual = AQL_EXECUTE("UPSERT @what INSERT { } UPDATE { 'c' : { c : 3 } } IN " + cn1 + " OPTIONS { mergeObjects: false } RETURN NEW", { what: { c: { a: 1, b: 2 } } });

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

      const actual = AQL_EXECUTE("UPSERT { 'c' : { a: 1, b: 2 } } INSERT { } UPDATE { } IN " + cn1 + " RETURN NEW", {});
      
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
/// @brief test upsert update with empty object
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateEmpty2: function () {
      for (let i = 0; i < 5; ++i) {
        const actual = AQL_EXECUTE(`UPSERT {name: "test1500"} INSERT {name: "test1500"} UPDATE {} IN ${cn3} OPTIONS { } RETURN { new: NEW, old: OLD }`);
        const res = actual.json[0];
        if (i > 0) {
          assertEqual(res.old._rev, res.new._rev);
          assertEqual(res.old.name, "test1500");
          assertEqual(4, Object.keys(res.old).length);
        }
        assertEqual(1001, c3.count());
        assertEqual(4, Object.keys(res.new).length);
        assertEqual(res.new.name, "test1500");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with no search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateEmptyBind: function () {
      c1.save({ "c" : { a: 1, b: 2 } });

      const actual = AQL_EXECUTE("UPSERT @what INSERT { } UPDATE { } IN " + cn1 + " RETURN NEW", { what: { c: { a: 1, b: 2 } } });
      
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
      const actual = AQL_EXECUTE("UPSERT { 'c' : { a: 1 } } INSERT { } UPDATE { } IN " + cn1 + " RETURN NEW", {});

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
      const actual = AQL_EXECUTE("UPSERT @what INSERT { } UPDATE { } IN " + cn1 + " RETURN NEW", { what: { c: { a: 1 } } });

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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT { bart: 'bark' } INSERT { bar: 'baz', _key: 'testfoo' } UPDATE { bark: 'barx' } IN " + cn1, {});

      assertEqual(2, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      const doc = c1.document("testfoo");
      assertEqual("testfoo", doc._key);
      assertFalse(doc.hasOwnProperty("a"));
      assertEqual("baz", doc.bar);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert with non existing search document
////////////////////////////////////////////////////////////////////////////////

    testUpsertNonExistingBind : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT @what INSERT { bar: 'baz', _key: 'testfoo' } UPDATE { bark: 'barx' } IN " + cn1, { what: { bart: "bark" } } );

      assertEqual(2, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      const doc = c1.document("testfoo");
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

      const queries = [
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
        const params = query[1];
        params["@cn"] = cn1;
        assertQueryError(errors.ERROR_QUERY_PARSE.code, query[0], params, query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInvalidDocumentsInsert : function () {
      c1.truncate({ compact: false });

      const queries = [
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
        const params = query[1];
        params["@cn"] = cn1;
        assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, query[0], params, query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInvalidDocumentsUpdate : function () {
      const queries = [
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
        const params = query[1];
        params["@cn"] = cn1;
        assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, query[0], params, query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertFollowedByDocumentAccess : function () {
      const queries = [
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn RETURN DOCUMENT('foo')", 
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn RETURN PASSTHRU(DOCUMENT('foo'))", 
        "UPSERT { } INSERT { } UPDATE { } INTO @@cn RETURN COLLECTION_COUNT(@@cn)", 
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
      const queries = [
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

      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT { value1: 0 } INSERT { value1: 0, value2: 0 } UPDATE { value2: 1 } IN " + cn1, {});

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

      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = AQL_EXECUTE("UPSERT { value1: 999999 } INSERT { _key: 'test999999', value1: 999999, value2: 0 } UPDATE { value2: 1 } IN " + cn1, {});

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

      const expected = { writesExecuted: 2000, writesIgnored: 0 };
      const actual = AQL_EXECUTE("FOR i IN 1..2000 UPSERT { value1: 0 } INSERT { value1: 0, value2: 0 } UPDATE { value2: 1 } IN " + cn1, {});

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

      const expected = { writesExecuted: 2000, writesIgnored: 0 };
      const actual = AQL_EXECUTE("FOR i IN 1..2000 UPSERT { value1: 999999 } INSERT { _key: 'test999999', value1: 999999, value2: 0 } UPDATE { value2: 1 } IN " + cn1, {});

      assertEqual(2002, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
      assertEqual([ ], actual.json);

      assertEqual(999999, c1.document("test999999").value1);
      assertEqual(1, c1.document("test999999").value2);
    }

  };
}

jsunity.run(ahuacatlModifySuite);
return jsunity.done();
