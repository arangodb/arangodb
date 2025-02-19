/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertNull, assertNotNull, assertTrue, 
  assertNotEqual, assertUndefined, fail */

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
/// @brief check whether the documents inserted are equal on the db.
////////////////////////////////////////////////////////////////////////////////

let validateModifyResultInsert = function (collection, results) {
  for (let index in results) {
    if (results.hasOwnProperty(index)) {
      assertTrue(isEqual(collection.document(results[index]._key), results[index]));
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief convert flat document database to an associative array with the keys
///        as object
////////////////////////////////////////////////////////////////////////////////

let wrapToKeys = function (results) {
  var keyArray = {};
  for (let index in results) {
    if (results.hasOwnProperty(index)){
      keyArray[results[index]._key] = results[index];
    }    
  }
  return keyArray;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlUpdateSuite () {
  const cn1 = "UnitTestsAhuacatlUpdate1";
  const cn2 = "UnitTestsAhuacatlUpdate2";
  const cn3 = "UnitTestsAhuacatlUpdate3";
  let c1;
  let c2;
  let c3;

  return {

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = db._create(cn1);
      c2 = db._create(cn2);
      c3 = db._create(cn3);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      c1.insert(docs);
      c2.insert(docs.slice(0, 50));
      
      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({name: `test${i}`});
      }
      c3.insert(docs);
    },

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = null;
      c2 = null;
      c3 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateNothing : function () {
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 UPDATE { foxx: true } IN " + cn1, {});
    
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateNothingBind : function () {
      const expected = { writesExecuted: 0, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 UPDATE { foxx: true } IN @@cn", { "@cn": cn1 });

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
      c1.ensureIndex({ type: "hash", fields: ["value1"], unique: true });
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn UPDATE d._key WITH { value1: 1 } IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateUniqueConstraint2 : function () {
      c1.ensureIndex({ type: "hash", fields: ["value3"], unique: true, sparse: true });
      assertQueryError(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, "FOR d IN @@cn UPDATE d._key WITH { value3: 1 } IN @@cn", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateIgnore1 : function () {
      c1.ensureIndex({ type: "hash", fields: ["value3"], unique: true, sparse: true });
      const expected = { writesExecuted: 1, writesIgnored: 99 };
      const actual = getModifyQueryResults("FOR d IN @@cn UPDATE d WITH { value3: 1 } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateIgnore2 : function () {
      c1.ensureIndex({ type: "hash", fields: ["value1"], unique: true, sparse: true });
      const expected = { writesExecuted: 0, writesIgnored: 51 };
      const actual = getModifyQueryResults("FOR i IN 50..100 UPDATE { _key: CONCAT('test', TO_STRING(i)), value1: 1 } IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateEmpty1 : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn UPDATE { _key: d._key } IN @@cn", { "@cn": cn1 });

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key } IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);
      var keyArray = wrapToKeys(res);
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn UPDATE d IN @@cn", { "@cn": cn1 });

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);
      var keyArray = wrapToKeys(res);
      for (var i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);

        assertTrue(keyArray.hasOwnProperty(doc._key));
        assertEqual("test" + i, keyArray[doc._key].value2);
      }
    },

///////////////////////////////////////////////////////////////////////////////
/// @brief test update with empty object
////////////////////////////////////////////////////////////////////////////////

    testUpdateEmpty3: function () {
      const actual = db._query(`FOR doc IN ${cn3} UPDATE doc WITH {} IN ${cn3} RETURN {old: OLD, new: NEW}`);
      let res = actual.toArray();
      assertEqual(res.length, 1000);
      for (let i = 0; i < res.length; ++i) {
        assertEqual(res[i].old._rev, res[i].new._rev);
        assertEqual(res[i].old.name, res[i].new.name);
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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResults("UPDATE { value: 'foobar', _key: 'test17' } IN @@cn", { "@cn": cn1 });

      assertEqual("foobar", c1.document("test17").value);
      assertEqual(expected, sanitizeStats(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testSingleUpdateWhatNew : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("UPDATE { value: 'foobar', _key: 'test17' } IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual("foobar", c1.document("test17").value);
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testSingleUpdateWhatOld : function () {
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("UPDATE { value: 'foobar', _key: 'test17' } IN @@cn LET old = OLD RETURN old", { "@cn": cn1 });

      let res = actual.toArray();
      assertFalse(res[0].hasOwnProperty('foobar'));
      assertEqual("test17", res[0]._key);
      assertEqual(17, res[0].value1);

      assertEqual("foobar", c1.document("test17").value);
      assertEqual(17, c1.document("test17").value1);
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateOldValue : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn UPDATE { _key: d._key, value1: d.value2, value2: d.value1, value3: d.value1 + 5 } IN @@cn", { "@cn": cn1 });

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn RETURN OLD", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
        assertEqual(doc._key.substr(4), doc.value1);
        assertEqual(doc._key, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnOldKey : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn RETURN OLD._key", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        assertEqual("test", res[i].substr(0, 4));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnCalculated : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn RETURN { newValue: NEW.value2, newKey: NEW._key }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        assertEqual("overwrite!", res[i].newValue);
        assertEqual("test", res[i].newKey.substr(0, 4));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnMultiCalculated : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn LET newValue = NEW.value2 LET newKey = NEW._key RETURN { newValue: newValue, newKey: newKey }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        assertEqual("overwrite!", res[i].newValue);
        assertEqual("test", res[i].newKey.substr(0, 4));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnNew : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value2: 'overwrite!' } IN @@cn RETURN NEW", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
        assertEqual(doc._key.substr(4), doc.value1);
        assertEqual("overwrite!", doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithNothing : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPDATE d WITH { } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertEqual(i, doc["new"].value1);
        assertEqual("test" + i, doc.old.value2);
        assertEqual("test" + i, doc["new"].value2);
        assertEqual(doc.old._rev, doc["new"]._rev); // _rev must have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithNothingReturnObjectLiteral : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPDATE d WITH { } IN @@cn RETURN { OLD, NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
        assertEqual("test" + i, doc.OLD._key);
        assertEqual("test" + i, doc.NEW._key);
        assertEqual(i, doc.OLD.value1);
        assertEqual(i, doc.NEW.value1);
        assertEqual("test" + i, doc.OLD.value2);
        assertEqual("test" + i, doc.NEW.value2);
        assertEqual(doc.OLD._rev, doc.NEW._rev); // _rev must have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithSomething : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPDATE d WITH { value1: d.value1, value2: d.value2 } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPDATE d WITH { value1: d.value1, value2: d.value2 } IN @@cn RETURN { OLD, NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value1: d.value2, value2: d.value1, value3: d.value1 + 5 } IN @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);
      var keyArray = wrapToKeys(res);

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE { _key: d._key, value1: d.value2, value2: d.value1, value3: d.value1 + 5 } IN @@cn LET old = OLD RETURN old", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);
      var keyArray = wrapToKeys(res);

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
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR i IN 1..50 UPDATE { _key: CONCAT('test', TO_STRING(i)) } INTO @@cn OPTIONS { waitForSync: true }", { "@cn": cn1 });
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
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 1..50 UPDATE { _key: CONCAT('test', TO_STRING(i)) } INTO @@cn OPTIONS { waitForSync: true } LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(50, res.length);
      var keyArray = wrapToKeys(res);
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null, a: { b: null } } INTO @@cn", { "@cn": cn1 });
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);
      var keyArray = wrapToKeys(res);

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null, a: { b: null } } INTO @@cn OPTIONS { keepNull: true }", { "@cn": cn1 });
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn OPTIONS { keepNull: true } LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);
      var keyArray = wrapToKeys(res);

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null, a: { b: null } } INTO @@cn OPTIONS { keepNull: false }", { "@cn": cn1 });
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null, a: { b: null } } INTO @@cn OPTIONS { keepNull: false } LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);
      var keyArray = wrapToKeys(res);

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn OPTIONS { keepNull: false } LET updated = OLD RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);
      var keyArray = wrapToKeys(res);

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
      let actual = db._query(q, {});
      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertTrue(res[0].hasOwnProperty("foo"));
      assertFalse(res[0].foo.hasOwnProperty("a"));
      assertFalse(res[0].foo.hasOwnProperty("foxx"));
      assertTrue(res[0].foo.hasOwnProperty("bark"));
      assertEqual("bart", res[0].foo.bark);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertTrue(doc.hasOwnProperty("foo"));
      assertFalse(doc.foo.hasOwnProperty("a"));
      assertFalse(doc.foo.hasOwnProperty("foxx"));
      assertEqual("bart", doc.foo.bark);

      actual = db._query(q, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));

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
      let actual = db._query(q, {});
      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertTrue(res[0].hasOwnProperty("foo"));
      assertFalse(res[0].foo.hasOwnProperty("a"));
      assertFalse(res[0].foo.hasOwnProperty("foxx"));
      assertFalse(res[0].foo.hasOwnProperty("bark"));
      assertEqual("bart", res[0].foo[0].bark);

      var doc = c1.toArray()[0];
      assertEqual("foo", doc._key);
      assertTrue(doc.hasOwnProperty("foo"));
      assertFalse(doc.foo.hasOwnProperty("a"));
      assertFalse(doc.foo.hasOwnProperty("foxx"));
      assertEqual("bart", doc.foo[0].bark);

      actual = db._query(q, {});

      assertEqual(1, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));

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
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));

      var doc = c1.document("something");
      assertEqual({ foo: 1, bar: 42, baz: 3, bumm : 23 }, doc.values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsDefaultWhatNew : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(1, res.length);
      
      var doc = c1.document("something");
      assertEqual({ foo: 1, bar: 42, baz: 3, bumm : 23 }, doc.values);
      assertTrue(isEqual(doc, res[0]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsTrue : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn OPTIONS { mergeObjects: true }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));
      
      var doc = c1.document("something");
      assertEqual({ foo: 1, bar: 42, baz: 3, bumm : 23 }, doc.values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsTrueWhatNew : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn OPTIONS { mergeObjects: true } LET updated = NEW RETURN updated", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(1, res.length);
      
      var doc = c1.document("something");
      assertEqual({ foo: 1, bar: 42, baz: 3, bumm : 23 }, doc.values);
      assertTrue(isEqual(doc, res[0]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsFalse : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn OPTIONS { mergeObjects: false }", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual));
      
      var doc = c1.document("something");
      assertEqual({ bar: 42, bumm: 23 }, doc.values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjectsFalseWhatNew : function () {
      c1.save({ _key: "something", values: { foo: 1, bar: 2, baz: 3 } });
      const expected = { writesExecuted: 1, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d._key == 'something' UPDATE d._key WITH { values: { bar: 42, bumm: 23 } } INTO @@cn OPTIONS { mergeObjects: false } LET updated = NEW RETURN updated", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(1, res.length);
      
      var doc = c1.document("something");
      assertEqual({ bar: 42, bumm: 23 }, doc.values);
      assertTrue(isEqual(doc, res[0]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateFilter : function () {
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 % 2 == 0 UPDATE d._key WITH { value2: 100 } INTO @@cn", { "@cn": cn1 });
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
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d.value1 % 2 == 0 UPDATE d._key WITH { value2: 100 } INTO @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(50, res.length);
      var keyArray = wrapToKeys(res);
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
      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn FILTER d.value1 % 2 == 0 UPDATE d._key WITH { value2: 100 } INTO @@cn LET updated = OLD RETURN updated", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(50, res.length);
      var keyArray = wrapToKeys(res);
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        const actual = getModifyQueryResults("FOR d IN @@cn UPDATE d._key WITH { counter: HAS(d, 'counter') ? d.counter + 1 : 1 } INTO @@cn", { "@cn": cn1 });
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual=[];

      for (j = 0; j < 5; ++j) {
        actual[j] = getModifyQueryResultsRaw("FOR d IN @@cn UPDATE d._key WITH { counter: HAS(d, 'counter') ? d.counter + 1 : 1 } INTO @@cn LET updated = NEW RETURN updated", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual[j].getExtra().stats));
        actual[j] = actual[j].toArray();
        assertEqual(100, actual[j].length);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(5, doc.counter);
      }

      for (j = 0; j < 5; ++j) {
        for (i = 0; i < 100; ++i) {
          assertEqual(j + 1, actual[j][i].counter);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace1 : function () {
      var i;
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        const actual = getModifyQueryResults("FOR d IN @@cn REPLACE d._key WITH { value4: 12 } INTO @@cn", { "@cn": cn1 });
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual=[];
      for (j = 0; j < 5; ++j) {
        actual[j] = getModifyQueryResultsRaw("FOR d IN @@cn REPLACE d._key WITH { value4: 12 } INTO @@cn LET replaced = NEW RETURN replaced", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual[j].getExtra().stats));
        actual[j] = actual[j].toArray();
        assertEqual(100, actual[j].length);
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
          assertFalse(actual[j][i].hasOwnProperty("value1"));
          assertFalse(actual[j][i].hasOwnProperty("value2"));
          assertFalse(actual[j][i].hasOwnProperty("value3"));
          assertEqual(12, actual[j][i].value4);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace2 : function () {
      var i;
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        const actual = getModifyQueryResults("FOR d IN @@cn REPLACE { _key: d._key, value4: 13 } INTO @@cn", { "@cn": cn1 });
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual=[];
      for (j = 0; j < 5; ++j) {
        actual[j] = getModifyQueryResultsRaw("FOR d IN @@cn REPLACE { _key: d._key, value4: " + j + " } INTO @@cn LET replaced = NEW RETURN replaced", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual[j].getExtra().stats));
        actual[j] = actual[j].toArray();
        assertEqual(100, actual[j].length);
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
          assertFalse(actual[j][i].hasOwnProperty("value1"));
          assertFalse(actual[j][i].hasOwnProperty("value2"));
          assertFalse(actual[j][i].hasOwnProperty("value3"));
          assertEqual(j, actual[j][i].value4);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceReplace : function () {
      var i;
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        const actual = getModifyQueryResults("FOR d IN @@cn REPLACE d._key WITH { value1: d.value1 + 1 } INTO @@cn", { "@cn": cn1 });
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      for (i = 0; i < 5; ++i) {
        const actual = getModifyQueryResultsRaw("FOR d IN @@cn REPLACE d._key WITH { value1: d.value1 + 1 } INTO @@cn LET replaced = NEW RETURN replaced", { "@cn": cn1 });
        assertEqual(expected, sanitizeStats(actual.getExtra().stats));
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = db._query("FOR d IN @@cn REPLACE d WITH { value3: d.value1 + 5 } IN @@cn LET previous = OLD RETURN previous", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      
      let res = actual.toArray().sort(function(l, r) {
        return l.value1 - r.value1;
      });
      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } UPDATE { } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertEqual(i, doc["new"].value1);
        assertEqual("test" + i, doc.old.value2);
        assertEqual("test" + i, doc["new"].value2);
        assertEqual(doc.old._rev, doc["new"]._rev); // _rev must have changed
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateWithNothingReturnObjectLiteral : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } UPDATE { } IN @@cn RETURN { OLD, NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
        assertEqual("test" + i, doc.OLD._key);
        assertEqual("test" + i, doc.NEW._key);
        assertEqual(i, doc.OLD.value1);
        assertEqual(i, doc.NEW.value1);
        assertEqual("test" + i, doc.OLD.value2);
        assertEqual("test" + i, doc.NEW.value2);
        assertEqual(doc.OLD._rev, doc.NEW._rev); // _rev must have changed
      }
    },

///////////////////////////////////////////////////////////////////////////////
/// @brief test replace with empty object
/// replaces all documents with empty object, hence, the old documents would
/// have the "name" key, as the new documents wouldn't
////////////////////////////////////////////////////////////////////////////////

    testReplaceEmpty: function () {
      const actual = db._query(`FOR doc IN ${cn3} REPLACE doc WITH {} IN ${cn3} RETURN {old: OLD, new: NEW}`);
      const res = actual.toArray();
      assertEqual(res.length, 1000);
      for (let i = 0; i < res.length; ++i) {
        assertNotEqual(res[i].old._rev, res[i].new._rev);
        assertFalse(res[i].new.hasOwnProperty("name"));
        assertTrue(res[i].old.hasOwnProperty("name"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReplaceWithNothing : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } REPLACE { } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
        assertEqual("test" + i, doc.old._key);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i, doc.old.value1);
        assertFalse(doc["new"].hasOwnProperty("value1"));
        assertEqual("test" + i, doc.old.value2);
        assertFalse(doc["new"].hasOwnProperty("value2"));
        assertNotEqual(doc.old._rev, doc["new"]._rev);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertUpdateWithSomething : function () {
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } UPDATE { value1: OLD.value1, value2: OLD.value2 } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn SORT d.value1 UPSERT { _key: d._key } INSERT { } REPLACE { value1: OLD.value1, value2: OLD.value2, value3: OLD.value1 } IN @@cn RETURN { old: OLD, new: NEW }", { "@cn": cn1 });

      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      let res = actual.toArray();
      assertEqual(100, res.length);

      for (var i = 0; i < 100; ++i) {
        var doc = res[i];
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

/////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert replace with empty replace object
/// in 5 iterations, starting with size 1000:
/// i = 0, document with name "test1500" doesn't exist, so inserts it size = 1001
/// i = 1, document exists so replaces it with empty document  size = 1001
/// i = 2, document with name "test1500" doesn't exist, so inserts it size = 1002
/// i = 3, document exists so replaces it with empty document  size = 1002
/// i = 4, document with name "test1500" doesn't exist, so inserts it size = 1003
/////////////////////////////////////////////////////////////////////////////////

    testUpsertReplaceEmpty: function () {
      for (let i = 0; i < 5; ++i) {
        const actual = db._query(`UPSERT {name: "test1500"} INSERT {name: "test1500"} REPLACE {} IN ${cn3} OPTIONS { } RETURN { new: NEW, old: OLD }`);
      const res = actual.toArray()[0];
        if (i % 2 !== 0) {
          assertNotEqual(res.old._rev, res.new._rev);
          assertEqual(res.old.name, "test1500");
          assertEqual(4, Object.keys(res.old).length);
          assertEqual(3, Object.keys(res.new).length);
          assertFalse(res.new.hasOwnProperty("name"));
          assertTrue(res.old.hasOwnProperty("name"));
        } else {
          assertEqual(4, Object.keys(res.new).length);
        }
      }
      assertEqual(1003, c3.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertCopy : function () {
      var i;
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR doc IN @@cn1 UPSERT { _key: doc._key } INSERT { _key: doc._key, value1: doc.value1, value2: doc.value2, new: true } UPDATE { new: false } INTO @@cn2", { "@cn1": cn1, "@cn2": cn2 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR d IN @@cn UPSERT { } INSERT d UPDATE { } IN @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsert : function () {
      var i;
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { eule: i } INSERT { _key: CONCAT('owl', i), x: i } UPDATE { y: i } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('owl', i), x: i } UPDATE { y: i } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));

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
      const expected = { writesExecuted: 100, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('owl', i), x: i } REPLACE { y: i } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));

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
      const expected = { writesExecuted: 200, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..199 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), x: i } UPDATE { y: i } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));

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
      const expected = { writesExecuted: 200, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 0..199 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn", { "@cn": cn1 });
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));

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
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN { value: OLD.value1, doc: OLD }", { "@cn": cn1 });
      let res = actual.toArray();

      for (var i = 0; i < res.length; ++i) {
        var doc = res[i];
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
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN { value: OLD.value1, doc: NEW }", { "@cn": cn1 });
      let res = actual.toArray();

      for (var i = 0; i < res.length; ++i) {
        var doc = res[i];
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
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('floxx', i) } INSERT { new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN OLD", { "@cn": cn1 });
      let res = actual.toArray();

      for (var i = 0; i < res.length; ++i) {
        assertNull(res[i]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnNewEmpty : function () {
      const actual = getModifyQueryResultsRaw("FOR i IN 0..99 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test' , i), new: true } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN NEW", { "@cn": cn1 });
      let res = actual.toArray();

      for (var i = 0; i < res.length; ++i) {
        var doc = res[i];
        assertEqual(i * 2, doc.value1);
        assertFalse(doc["new"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnOldNew : function () {
      const actual = getModifyQueryResultsRaw("FOR i IN 0..199 UPSERT { _key: CONCAT('test', i) } INSERT { _key: CONCAT('test', i), new: true, value1: 4 * i } UPDATE { value1: OLD.value1 * 2, new: false } INTO @@cn RETURN { value: i, old: OLD, new: NEW }", { "@cn": cn1 });
      let res = actual.toArray();

      for (var i = 0; i < res.length; ++i) {
        var doc = res[i];
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
      const actual = getModifyQueryResultsRaw("UPSERT { _key: 'quux' } INSERT { _key: 'f0xx' } UPDATE { gotIt : true } INTO @@cn RETURN NEW", { "@cn": cn1 });
      let res = actual.toArray();

      assertEqual(1, res.length);
      var doc = res[0];
      assertEqual("f0xx", doc._key);
      assertEqual([ "_id", "_key", "_rev" ], Object.keys(doc).sort()); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertFilterConditionHit1 : function () {
      c1.save({ foo: { bar: { baz: "a" }, foobar: [ 1, 2 ] } });

      const actual = getModifyQueryResultsRaw("UPSERT { foo: { bar: { baz: 'a' } } } INSERT { _key: 'f0xx' } UPDATE { gotIt : true } INTO @@cn RETURN NEW", { "@cn": cn1 });
      let res = actual.toArray();

      assertEqual(1, res.length);
      var doc = res[0];
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

      const actual = getModifyQueryResultsRaw("UPSERT { foo: { foobar: [ 1, 2 ], bar: { baz: 'a' } } } INSERT { _key: 'f0xx' } UPDATE { gotIt : true } INTO @@cn RETURN NEW", { "@cn": cn1 });
      let res = actual.toArray();

      assertEqual(1, res.length);
      var doc = res[0];
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

      const actual = getModifyQueryResultsRaw("UPSERT { foo: { bar: { baz: 'a' } } } INSERT { _key: 'f0xx' } UPDATE { gotIt : true } INTO @@cn RETURN NEW", { "@cn": cn1 });
      let res = actual.toArray();

      assertEqual(1, res.length);
      var doc = res[0];
      assertFalse(doc.hasOwnProperty("gotIt"));
      assertFalse(doc.hasOwnProperty("foo"));
      assertEqual("f0xx", doc._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertReturnConditional : function () {
      const actual = getModifyQueryResultsRaw("FOR i IN 98..100 LET a = PASSTHRU(CONCAT('test', i)) UPSERT { _key: a, value1: i, value2: a } INSERT { _key: a, wasInserted: true } UPDATE { value5: OLD.value1 + 1 } INTO @@cn RETURN { doc: NEW, type: OLD ? 'update' : 'insert'}", { "@cn": cn1 });
      let res = actual.toArray();

      assertEqual(3, res.length);
      var doc = res[0];
      assertEqual("update", doc.type);
      assertEqual("test98", doc.doc._key);
      assertEqual(98, doc.doc.value1);
      assertEqual("test98", doc.doc.value2);
      assertEqual(99, doc.doc.value5);

      doc = res[1];
      assertEqual("update", doc.type);
      assertEqual("test99", doc.doc._key);
      assertEqual(99, doc.doc.value1);
      assertEqual("test99", doc.doc.value2);
      assertEqual(100, doc.doc.value5);
      
      doc = res[2];
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

      const actual = getModifyQueryResultsRaw("FOR i IN 98..100 LET a = CONCAT('test', i) UPSERT { _key: a, value1: i, value2: a } INSERT @insert UPDATE @update INTO @@cn RETURN { doc: NEW, type: OLD ? 'update' : 'insert'}", { "@cn": cn1, insert: insert, update: update });
      let res = actual.toArray();

      assertEqual(3, res.length);
      var doc = res[0];
      assertEqual("update", doc.type);
      assertEqual("test98", doc.doc._key);
      assertEqual(98, doc.doc.value1);
      assertEqual("far", doc.doc.boo);
      assertEqual({ foo: { bar: 1 } }, doc.doc.value2);

      doc = res[1];
      assertEqual("update", doc.type);
      assertEqual("test99", doc.doc._key);
      assertEqual(99, doc.doc.value1);
      assertEqual("far", doc.doc.boo);
      assertEqual({ foo: { bar: 1 } }, doc.doc.value2);
      
      doc = res[2];
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

      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResults("FOR i IN 1..50 UPSERT { foo: 1 } INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlUpdate1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlUpdate2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } UPDATE { } INTO @@cn", { "@cn": edge.name() });

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

      const expected = { writesExecuted: 50, writesIgnored: 0 };
      const actual = getModifyQueryResultsRaw("FOR i IN 1..50 UPSERT { foo: 1 } INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlUpdate1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlUpdate2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } UPDATE { } INTO @@cn LET inserted = NEW RETURN inserted", { "@cn": edge.name() });
      let res = actual.toArray();

      validateModifyResultInsert(edge, res);
      validateDocuments(res, true);
      assertEqual(50, res.length);
      assertEqual(expected, sanitizeStats(actual.getExtra().stats));
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

jsunity.run(ahuacatlUpdateSuite);
return jsunity.done();
