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
  for (let index in documents) {
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
  for (let index in results) {
    if (results.hasOwnProperty(index)){
      assertTrue(isEqual(collection.document(results[index]._key), results[index]));
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlInsertSuite () {
  var errors = internal.errors;
  const cn1 = "UnitTestsAhuacatlInsert1";
  const cn2 = "UnitTestsAhuacatlInsert2";
  let c1;
  let c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      c1.insert(docs);
      docs = [];
      for (let i = 0; i < 50; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      c2.insert(docs);
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
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 INSERT { foxx: true } IN " + cn1, {});
    
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertNothingWhat : function () {
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " FILTER d.value1 < 0 INSERT { foxx: true } IN " + cn1 + " LET inserted = NEW RETURN inserted", {});
    
      assertEqual(0, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertReturnDoc : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: true } IN " + cn1 + " RETURN NEW", {});
    
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: true } IN " + cn1 + " RETURN { NEW }", {});
    
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: true } IN " + cn1 + " RETURN NEW._key", {});
    
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: 1, value: 42 } IN " + cn1 + " RETURN CONCAT(NEW.foxx, '/', NEW.value)", {});
    
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN " + cn1 + " INSERT { foxx: 1, value: 42 } IN " + cn1 + " LET a = NEW.foxx LET b = NEW.value LET c = CONCAT(a, '-', b) RETURN c", {});
    
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
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 INSERT { foxx: true } IN @@cn", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertNothingBindWhat : function () {
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d.value1 < 0 INSERT { foxx: true } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });

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
      const expected = { writesExecuted: 0, writesIgnored: 100 };
      const actual = getModifyQueryResults("FOR d IN @@cn INSERT d IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore1What : function () {
      const expected = { writesExecuted: 0, writesIgnored: 100 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn INSERT d IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn1 });

      assertEqual(0, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore2 : function () {
      const expected = { writesExecuted: 1, writesIgnored: 50 };
      const actual = getModifyQueryResults("FOR i IN 50..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(101, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore2What : function () {
      const expected = { writesExecuted: 1, writesIgnored: 50 };
      const actual = getModifyQueryResultsRaw("FOR i IN 50..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn1 });

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
      const expected = { writesExecuted: 51, writesIgnored: 50 };
      const actual = getModifyQueryResults("FOR i IN 0..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn2 });

      assertEqual(101, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore3What : function () {
      const expected = { writesExecuted: 51, writesIgnored: 50 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn2 });

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
      const expected = { writesExecuted: 0, writesIgnored: 100 };
      const actual = getModifyQueryResults("FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore4What : function () {
      const expected = { writesExecuted: 0, writesIgnored: 100 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn1 });

      assertEqual(0, actual.json.length);
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore5 : function () {
      const expected = { writesExecuted: 50, writesIgnored: 50 };
      const actual = getModifyQueryResults("FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn2 });

      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore5What : function () {
      const expected = { writesExecuted: 50, writesIgnored: 50 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true } LET inserted = NEW RETURN inserted", { "@cn": cn2 });

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn INSERT { } IN @@cn", { "@cn": cn1 });

      assertEqual(200, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEmptyWhat : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn INSERT { } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });

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
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR i IN 50..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn2 });

      assertEqual(100, c1.count());
      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertCopyWhat : function () {
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 50..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn2 });

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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResults("INSERT { value: 'foobar', _key: 'test' } IN @@cn", { "@cn": cn1 });

      assertEqual(101, c1.count());
      assertEqual("foobar", c1.document("test").value);
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testSingleInsertWhat : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("INSERT { value: 'foobar', _key: 'test' } IN @@cn LET inserted = NEW RETURN inserted", { "@cn": cn1 });

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
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR i IN 1..50 INSERT { value: i } INTO @@cn OPTIONS { waitForSync: true }", { "@cn": cn2 });

      assertEqual(100, c1.count());
      assertEqual(100, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertWaitForSyncWhat : function () {
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 1..50 INSERT { value: i } INTO @@cn OPTIONS { waitForSync: true } LET inserted = NEW RETURN inserted", { "@cn": cn2 });

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

      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR i IN 1..50 INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlInsert2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } INTO @@cn", { "@cn": edge.name() });

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

      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 1..50 INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlInsert2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } INTO @@cn LET inserted = NEW RETURN inserted", { "@cn": edge.name() });

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


jsunity.run(ahuacatlInsertSuite);
return jsunity.done();
