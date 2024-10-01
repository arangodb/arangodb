/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertNull, assertNotNull, assertTrue, 
  assertNotEqual, assertUndefined, fail, assertNotUndefined */

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

const internal = require("internal");
const db = require("@arangodb").db;
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getModifyQueryResults = helper.getModifyQueryResults;
const getModifyQueryResultsRaw = helper.getModifyQueryResultsRaw;
const sanitizeStats = helper.sanitizeStats;
const isEqual = helper.isEqual;
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;

let validateDocuments = function (documents, isEdgeCollection) {
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
/// @brief check whether the documents reported deleted are really gone
////////////////////////////////////////////////////////////////////////////////

let validateDeleteGone = function (collection, results) {
  for (let index in results) {
    if (results.hasOwnProperty(index)) {
      try {
        assertEqual(collection.document(results[index]._key), {});
        fail();
      } catch (e) {
        assertNotUndefined(e.errorNum, "unexpected error format while calling checking for deleted entry");
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, e.errorNum, "unexpected error code (" + e.errorMessage + "): ");
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlRemoveSuite () {
  const cn1 = "UnitTestsAhuacatlRemove1";
  const cn2 = "UnitTestsAhuacatlRemove2";
  let c1;
  let c2;

  return {

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
      c2.insert(docs.slice(0, 50));
    },

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
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 REMOVE d IN " + cn1, {});
    
      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothingWhat : function () {
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 REMOVE d IN " + cn1 + " LET removed = OLD RETURN removed", {});

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveReturnDoc : function () {
      const actual = db._query("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET removed = OLD RETURN OLD", {}).toArray();

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
      const actual = db._query("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET removed = OLD RETURN { OLD }", {}).toArray();

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
      const actual = db._query("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET removed = OLD RETURN removed._key", {}).toArray();

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
      const actual = db._query("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET removed = OLD RETURN CONCAT('--', removed._key)", {}).toArray();

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
      const actual = db._query("FOR d IN " + cn1 + " REMOVE d IN " + cn1 + " LET a = CONCAT('--', OLD._key) LET b = CONCAT('--', a) LET c = CONCAT('xx', b) RETURN SUBSTRING(c, 4)", {}).toArray();

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
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 REMOVE d IN @@cn", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothingBindWhat : function () {
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 REMOVE d IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

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
      const expected = { writesExecuted: 0, writesIgnored: 100 };
      const actual = getModifyQueryResults("FOR d IN @@cn REMOVE 'foo' IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore1What : function () {
      const expected = { writesExecuted: 0, writesIgnored: 100 };
      const actual = getModifyQueryResults("FOR d IN @@cn REMOVE 'foo' IN @@cn OPTIONS { ignoreErrors: true } LET removed = OLD RETURN removed", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore2 : function () {
      const expected = { writesExecuted: 100, writesIgnored: 101 };
      const actual = getModifyQueryResults("FOR i IN 0..200 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore2What : function () {
      const expected = { writesExecuted: 100, writesIgnored: 101 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..200 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn OPTIONS { ignoreErrors: true } LET removed = OLD RETURN removed ", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(100, res.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll1 : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn REMOVE d IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll1What : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE d IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(100, res.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll2 : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn REMOVE d._key IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll2What : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE d._key IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(100, res.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll3 : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn REMOVE { _key: d._key } IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll3What : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE { _key: d._key } IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(100, res.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll4 : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR i IN 0..99 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll4What : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(100, res.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll5 : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn REMOVE d INTO @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll5What : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE d INTO @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(100, res.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveHalf : function () {
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR i IN 0..99 FILTER i % 2 == 0 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn1 });

      assertEqual(50, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveHalfWhat : function () {
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 FILTER i % 2 == 0 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(50, res.length);
      assertEqual(50, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResults("REMOVE 'test0' IN @@cn", { "@cn": cn1 });

      assertEqual(99, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testSingleWhat : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("REMOVE 'test0' IN @@cn LET removed = OLD RETURN removed", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(1, res.length);
      assertEqual(99, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
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
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn1 FILTER d.value1 < 50 REMOVE { _key: d._key } IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin1What : function () {
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn1 FILTER d.value1 < 50 REMOVE { _key: d._key } IN @@cn2 LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c2, res);
      assertEqual(50, res.length);
      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin2 : function () {
      const expected = { writesExecuted: 48, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn1 FILTER d.value1 >= 2 && d.value1 < 50 REMOVE { _key: d._key } IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(2, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin2What : function () {
      const expected = { writesExecuted: 48, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn1 FILTER d.value1 >= 2 && d.value1 < 50 REMOVE { _key: d._key } IN @@cn2 LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c2, res);
      assertEqual(48, res.length);
      assertEqual(100, c1.count());
      assertEqual(2, c2.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors1 : function () {
      const expected = { writesExecuted: 50, writesIgnored: 50 };
      const actual = getModifyQueryResults("FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2 OPTIONS { ignoreErrors: true }", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors1What : function () {
      const expected = { writesExecuted: 50, writesIgnored: 50 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2 OPTIONS { ignoreErrors: true } LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c2, res);
      assertEqual(50, res.length);
      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors2 : function () {
      const expected = { writesExecuted: 0, writesIgnored: 100 };
      const actual = getModifyQueryResults("FOR d IN @@cn1 REMOVE { _key: CONCAT('foo', d._key) } IN @@cn2 OPTIONS { ignoreErrors: true }", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(50, c2.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors2What : function () {
      const expected = { writesExecuted: 0, writesIgnored: 100 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn1 REMOVE { _key: CONCAT('foo', d._key) } IN @@cn2 OPTIONS { ignoreErrors: true } LET removed = OLD RETURN removed", { "@cn1": cn1, "@cn2": cn2 });

      let res = actual.toArray();
      assertEqual(0, res.length);
      assertEqual(100, c1.count());
      assertEqual(50, c2.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveWaitForSync : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn REMOVE d IN @@cn OPTIONS { waitForSync: true }", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove - return what
////////////////////////////////////////////////////////////////////////////////

    testRemoveWaitForSyncWhat : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn REMOVE d IN @@cn OPTIONS { waitForSync: true } LET removed = OLD RETURN removed", { "@cn": cn1 });

      let res = actual.toArray();
      validateDocuments(res, false);
      validateDeleteGone(c1, res);
      assertEqual(100, res.length);
      assertEqual(0, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
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
      const expected = { writesExecuted: 10, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR i IN 0..9 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn", { "@cn": edge.name() });

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
      const expected = { writesExecuted: 10, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..9 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn LET removed = OLD RETURN removed", { "@cn": edge.name() });

      let res = actual.toArray();
      validateDocuments(res, true);
      validateDeleteGone(edge, res);
      assertEqual(10, res.length);
      assertEqual(100, c1.count());
      assertEqual(90, edge.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      db._drop("UnitTestsAhuacatlEdge");
    }

  };
}

jsunity.run(ahuacatlRemoveSuite);
return jsunity.done();
