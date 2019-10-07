/*jshint globalstrict:false, strict:false, strict: false, maxlen: 500 */
/*global assertEqual, assertFalse, assertNull, assertTrue, AQL_EXECUTE */

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

var db = require("@arangodb").db;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var internal = require("internal");
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlClusterUpsertKeySuite () {
  var cn1 = "UnitTestsAhuacatlUpsert1";
  var c1;

  var sorter = function (l, r) {
    if (l["new"].value2 !== r["new"].value2) {
      return l["new"].value2 - r["new"].value2;
    }
    return 0;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      c1 = db._create(cn1, { numberOfShards: 5 });
      let docs = [];
      for (var i = 0; i < 20; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      c1.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      c1 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertOnlyUpdate : function () {
      var actual = AQL_EXECUTE("FOR i IN 0..9 UPSERT { _key: CONCAT('test', i) } INSERT { new: true, value2: i } UPDATE { value2: i, new: false, value: OLD.value + 1 } IN @@cn1 RETURN { old: OLD, new: NEW }", { "@cn1": cn1 });

      actual.json.sort(sorter);
      assertEqual(10, actual.json.length);
      for (var i = 0; i < 10; ++i) {
        var doc = actual.json[i];

        assertEqual(i, doc["new"].value2);
        assertEqual("test" + i, doc.old._key);
        assertEqual(i, doc.old.value);
        assertEqual("test" + i, doc["new"]._key);
        assertEqual(i + 1, doc["new"].value);
        assertFalse(doc["new"]["new"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertOnlyInsert : function () {
      var actual = AQL_EXECUTE("FOR i IN 50..59 UPSERT { _key: CONCAT('test', i) } INSERT { new: true, value2: i, _key: CONCAT('test', i) } UPDATE { value2: i, new: false, value: OLD.value + 1 } IN @@cn1 RETURN { old: OLD, new: NEW }", { "@cn1": cn1 });
      actual.json.sort(sorter);

      assertEqual(10, actual.json.length);
      for (var i = 0; i < 10; ++i) {
        var doc = actual.json[i];

        assertEqual(50 + i, doc["new"].value2);
        assertNull(doc.old);
        assertEqual("test" + (50 + i), doc["new"]._key);
        assertTrue(doc["new"]["new"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertMixed : function () {
      var actual = AQL_EXECUTE("FOR i IN 0..39 UPSERT { _key: CONCAT('test', i) } INSERT { new: true, value2: i, _key: CONCAT('test', i) } UPDATE { value2: i, new: false, value: OLD.value + 1 } IN @@cn1 RETURN { old: OLD, new: NEW }", { "@cn1": cn1 });
      actual.json.sort(sorter);

      assertEqual(40, actual.json.length);
      for (var i = 0; i < 40; ++i) {
        var doc = actual.json[i];

        assertEqual(i, doc["new"].value2);
        assertEqual("test" + i, doc["new"]._key);

        if (i < 20) {
          assertEqual("test" + i, doc.old._key);
          assertEqual(i, doc.old.value);
          assertEqual(i + 1, doc["new"].value);
          assertFalse(doc["new"]["new"]);
        }
        else {
          assertNull(doc.old);
          assertTrue(doc["new"]["new"]);
        }
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlClusterUpsertNonKeySuite () {
  var cn1 = "UnitTestsAhuacatlUpsert1";
  var c1;
  var errors = internal.errors;

  var sorter = function (l, r) {
    if (l["new"].value2 !== r["new"].value2) {
      return l["new"].value2 - r["new"].value2;
    }
    return 0;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      c1 = db._create(cn1, { numberOfShards: 5, shardKeys: [ "value" ] });

      let docs = [];
      for (var i = 0; i < 20; ++i) {
        docs.push({ value: i });
      }
      c1.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      c1 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertOnlyUpdateNonKey : function () {
      var actual = AQL_EXECUTE("FOR i IN 0..9 UPSERT { value : i } INSERT { new: true, value2: i, value: i } UPDATE { value2: i, new: false } IN @@cn1 RETURN { old: OLD, new: NEW }", { "@cn1": cn1 });

      actual.json.sort(sorter);
      assertEqual(10, actual.json.length);
      for (var i = 0; i < 10; ++i) {
        var doc = actual.json[i];

        assertEqual(i, doc["new"].value2);
        assertEqual(i, doc.old.value);
        assertEqual(i, doc["new"].value);
        assertFalse(doc["new"]["new"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertOnlyInsertNonKey : function () {
      var actual = AQL_EXECUTE("FOR i IN 50..59 UPSERT { value: i } INSERT { new: true, value2: i, value: i } UPDATE { value2: i, new: false } IN @@cn1 RETURN { old: OLD, new: NEW }", { "@cn1": cn1 });
      actual.json.sort(sorter);

      assertEqual(10, actual.json.length);
      for (var i = 0; i < 10; ++i) {
        var doc = actual.json[i];

        assertNull(doc.old);
        assertEqual(50 + i, doc["new"].value2);
        assertEqual(50 + i, doc["new"].value);
        assertTrue(doc["new"]["new"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertMixedNonKey : function () {
      var actual = AQL_EXECUTE("FOR i IN 0..39 UPSERT { value: i } INSERT { new: true, value2: i, value: i } UPDATE { value2: i, new: false } IN @@cn1 RETURN { old: OLD, new: NEW }", { "@cn1": cn1 });
      actual.json.sort(sorter);

      assertEqual(40, actual.json.length);
      for (var i = 0; i < 40; ++i) {
        var doc = actual.json[i];

        assertEqual(i, doc["new"].value2);

        if (i < 20) {
          assertEqual(i, doc.old.value);
          assertEqual(i, doc["new"].value);
          assertFalse(doc["new"]["new"]);
        }
        else {
          assertNull(doc.old);
          assertEqual(i, doc["new"].value);
          assertTrue(doc["new"]["new"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertInsertWithKeyNonKey : function () {
      assertQueryError(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, "FOR i IN 20..29 UPSERT { value: i } INSERT { _key: CONCAT('test', i) } UPDATE { } IN @@cn1 RETURN { old: OLD, new: NEW }", { "@cn1": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upsert
////////////////////////////////////////////////////////////////////////////////

    testUpsertShardKeyChangeNonKey : function () {
      assertQueryError(errors.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, "FOR i IN 0..19 UPSERT { value: i } INSERT { } UPDATE { value: OLD.value + 1 } IN @@cn1 RETURN { old: OLD, new: NEW }", { "@cn1": cn1 });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlClusterUpsertKeySuite);
jsunity.run(ahuacatlClusterUpsertNonKeySuite);

return jsunity.done();

