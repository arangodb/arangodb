/* global fail */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const { assertTrue, assertFalse, assertEqual } = jsunity.jsUnity.assertions;
const _ = require("lodash");

const database = "ObjectEnumTestDB";
const docCollection = "coll";
const edgeColl = "edgeColl";

function enumerateObjectTestSuite() {

  function checkPlan(plan, isOptimized = true) {
    const nodes = plan.nodes;

    // get the EnumerateListNode
    const enumerate = nodes.filter(x => x.type === "EnumerateListNode");
    assertEqual(enumerate.length, 1);

    if (isOptimized) {
      // check that the mode is set to object
      assertEqual(enumerate[0].mode, "object");
      // test that the optimizer rule replace-entries-with-object-iteration triggered
      assertTrue(plan.rules.includes("replace-entries-with-object-iteration"));
    } else {
      assertEqual(enumerate[0].mode, undefined);
    }
  };

  function prepareStatement(query, bindVars, options) {
    return db._createStatement({ query: query, bindVars: bindVars, options: options });
  }

  function queryDocuments(coll, query, fullCount, isOptimized = true) {
    let options = { fullCount: fullCount, optimizer: { rules: ["+replace-entries-with-object-iteration"] } };
    if (!isOptimized) {
      options["optimizer"]["rules"] = ["-replace-entries-with-object-iteration"];
    }
    const stmt = prepareStatement(query, null, options);
    checkPlan(stmt.explain().plan, isOptimized);

    return stmt.execute();
  }

  function queryDocumentsWithLimit(coll, offset, count, fullCount, isOptimized = true) {
    const query = `
    FOR doc IN ${coll}
      FOR [key, value] IN ENTRIES(doc)
      SORT doc._key, key
      LIMIT ${offset}, ${count}
      RETURN [key, value]
    `;

    return queryDocuments(coll, query, fullCount, isOptimized);
  }

  function queryDocumentsNoLimit(coll, fullCount, isOptimized = true) {
    const query = `
    FOR doc IN ${coll}
      FOR [key, value] IN ENTRIES(doc)
      SORT doc._key, key
      RETURN [key, value]
    `;

    return queryDocuments(coll, query, fullCount, isOptimized);
  }

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      db._create(docCollection);
      db._createEdgeCollection(edgeColl);

      db[docCollection].insert({ "key1": "a" });
      db[docCollection].insert({ "key2": "b" });
      db[docCollection].insert({ "key3": "c" });
      db[docCollection].insert({ "key4": "e" });
      db[docCollection].insert({ "key5": "f" });
      db[docCollection].insert({ "key6": [1, 2, 3] });
      db[docCollection].insert({ "key7": 42 });
      db[docCollection].insert({ "key8": { "subkey": { "a": 1 } } });

      db[edgeColl].insert({ _from: docCollection + "/key1", _to: docCollection + "/key4", "field": { "a": 1 } });
      db[edgeColl].insert({ _from: docCollection + "/key3", _to: docCollection + "/key4", "field": { "b": 2 } });
      db[edgeColl].insert({ _from: docCollection + "/key5", _to: docCollection + "/key6", "field": { "c": 3 } });
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testSimpleObjectIterationPlan: function () {
      const query = `
        FOR doc IN ${docCollection}
          FOR [key, value] IN ENTRIES(doc)
          RETURN [key, value]
      `;

      // with optimization
      var stmt = prepareStatement(query, null, null);
      checkPlan(stmt.explain().plan, true);

      // without optimization
      stmt = prepareStatement(query, null, { optimizer: { rules: ["-replace-entries-with-object-iteration"] } });
      checkPlan(stmt.explain().plan, false);
    },

    testOutputDocumentCollection: function () {
      let actual = queryDocumentsNoLimit(docCollection).toArray();

      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      assertEqual(systemFields.length, 3 * 8); // 3 system fields * 8 documents in collection
      actual = actual.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
      let expected = [
        "key1",
        "a",
        "key2",
        "b",
        "key3",
        "c",
        "key4",
        "e",
        "key5",
        "f",
        "key6",
        [
          1,
          2,
          3
        ],
        "key7",
        42,
        "key8",
        { "subkey": { "a": 1 } }
      ];
      assertEqual(actual, expected);
    },

    testOutputEdgeCollection: function () {
      let actual = queryDocumentsNoLimit(edgeColl).toArray();
      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id", "_from", "_to"].includes(e[0]));
      assertEqual(systemFields.length, 5 * 3); // 5 system fields * 3 documents in collection
      actual = actual.filter((e) => !["_key", "_rev", "_id", "_from", "_to"].includes(e[0])).flat();

      let expected = [
        "field",
        { "a": 1 },
        "field",
        { "b": 2 },
        "field",
        { "c": 3 }
      ];
      assertEqual(actual, expected);
    },

    testOutputDocumentCollectionWithLimit1: function () {
      // skip 12 first values (therefore 3 complete documents will be skipped), return 40.
      // In our case, the count will be more than left documents in a collection
      let actual = queryDocumentsWithLimit(docCollection, 12, 40, true);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[docCollection].count() * 4);

      actual = actual.toArray();
      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      // First 3 documents were skipped (offset == 12)
      assertEqual(systemFields.length, 3 * 5); // 3 system fields * 5 non-skipped documents in collection
      actual = actual.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
      let expected = [
        "key4",
        "e",
        "key5",
        "f",
        "key6",
        [
          1,
          2,
          3
        ],
        "key7",
        42,
        "key8",
        { "subkey": { "a": 1 } }
      ];
      assertEqual(actual, expected);
    },

    testOutputDocumentCollectionWithLimit2: function () {
      // skip 4 first values (therefore 1 complete document will be skipped), return 4.
      let actual = queryDocumentsWithLimit(docCollection, 4, 4, true);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[docCollection].count() * 4);

      actual = actual.toArray();
      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      // First 3 documents were skipped (offset == 12)
      assertEqual(systemFields.length, 3 * 1); // 3 system fields * 1 non-skipped documents in collection
      actual = actual.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
      let expected = [
        "key2",
        "b"
      ];
      assertEqual(actual, expected);
    },

    testOutputDocumentCollectionWithLimit3: function () {
      // skip 5 (odd number) first values (therefore 1 complete document will be skipped. 
      // The first system field of the second document is also skipped), return 4.
      let actual = queryDocumentsWithLimit(docCollection, 5, 4, true);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[docCollection].count() * 4);

      actual = actual.toArray();
      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      assertEqual(systemFields.length, 3); // 3 system fields 
      actual = actual.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
      let expected = [
        "key2",
        "b"
      ];
      assertEqual(actual, expected);
    },

    testOutputDocumentCollectionWithLimit4: function () {
      // skip 400 first values (therefore all documents will be skipped), return 4.
      let actual = queryDocumentsWithLimit(docCollection, 400, 4, true);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[docCollection].count() * 4);

      actual = actual.toArray();
      assertTrue(actual.length === 0);
    }
  };
}

jsunity.run(enumerateObjectTestSuite);

return jsunity.done();
