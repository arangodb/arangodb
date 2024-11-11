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

  function queryDocuments(coll, query, fullCount, rules = null) {
    let options = { fullCount: fullCount};
    if (rules === null) {
      // Optimize all by default
      options["optimizer"] = { rules: ["+replace-entries-with-object-iteration", "+move-filters-into-enumerate"]};
    } else {
      options["optimizer"] = { "rules": rules}; 
    }

    // Check wheter 'replace-entries-with-object-iteration' is used. Check plan afterwards
    let isOptimized = options["optimizer"]["rules"].some((e) => e === "+replace-entries-with-object-iteration");
    const stmt = prepareStatement(query, null, options);
    checkPlan(stmt.explain().plan, isOptimized);

    return stmt.execute();
  }

  function queryDocumentsWithLimit(coll, offset, count, fullCount, rules = null) {
    const query = `
    FOR doc IN ${coll}
      FOR [key, value] IN ENTRIES(doc)
      SORT doc._key, key
      LIMIT ${offset}, ${count}
      RETURN [key, value]
    `;

    return queryDocuments(coll, query, fullCount, rules);
  }

  function queryDocumentsNoLimit(coll, fullCount, rules) {
    const query = `
    FOR doc IN ${coll}
      FOR [key, value] IN ENTRIES(doc)
      SORT doc._key, key
      RETURN [key, value]
    `;

    return queryDocuments(coll, query, fullCount, rules);
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

      db[edgeColl].insert({ _from: docCollection + "/key1", _to: docCollection + "/key2", "field": { "ab": 12 } });
      db[edgeColl].insert({ _from: docCollection + "/key1", _to: docCollection + "/key3", "field": { "ac": 13 } });
      db[edgeColl].insert({ _from: docCollection + "/key1", _to: docCollection + "/key4", "field": { "ad": 14 } });

      db[edgeColl].insert({ _from: docCollection + "/key3", _to: docCollection + "/key1", "field": { "ca": 31 } });
      db[edgeColl].insert({ _from: docCollection + "/key3", _to: docCollection + "/key2", "field": { "cb": 32 } });
      db[edgeColl].insert({ _from: docCollection + "/key3", _to: docCollection + "/key4", "field": { "cd": 34 } });

      db[edgeColl].insert({ _from: docCollection + "/key5", _to: docCollection + "/key6", "field": { "ef": 56 } });
      db[edgeColl].insert({ _from: docCollection + "/key5", _to: docCollection + "/key7", "field": { "eg": 57 } });
      db[edgeColl].insert({ _from: docCollection + "/key5", _to: docCollection + "/key8", "field": { "eh": 58 } });
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
      const actual = queryDocumentsNoLimit(docCollection, false, ["+replace-entries-with-object-iteration"]).toArray();

      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      assertEqual(systemFields.length, 3 * 8); // 3 system fields * 8 documents in collection
      let actualArr = actual.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
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
      assertEqual(actualArr, expected);
    },

    testOutputEdgeCollection: function () {
      // skip 6 documents: 6 * 6 == 36
      // return 3 documents: 3 * 6 = 18
      const actual = queryDocumentsWithLimit(edgeColl, 36, 18, true, ["+replace-entries-with-object-iteration"]);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[edgeColl].count() * 6);

      let actualArr = actual.toArray();
      // Check that system fields are present
      let systemFields = actualArr.filter((e) => ["_key", "_rev", "_id", "_from", "_to"].includes(e[0]));
      assertEqual(systemFields.length, 5 * 3); // 5 system fields * 9 documents in collection
      actualArr = actualArr.filter((e) => !["_key", "_rev", "_id", "_from", "_to"].includes(e[0])).flat();

      let expected = [
        "field",
        { "ef": 56 },
        "field",
        { "eg": 57 },
        "field",
        { "eh": 58 }
      ];
      assertEqual(actualArr, expected);
    },

    testOutputDocumentCollectionWithLimit1: function () {
      // skip 12 first values (therefore 3 complete documents will be skipped), return 40.
      // In our case, the count will be more than left documents in a collection
      const actual = queryDocumentsWithLimit(docCollection, 12, 40, true, ["+replace-entries-with-object-iteration"]);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[docCollection].count() * 4);

      let actualArr = actual.toArray();
      // Check that system fields are present
      let systemFields = actualArr.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      // First 3 documents were skipped (offset == 12)
      assertEqual(systemFields.length, 3 * 5); // 3 system fields * 5 non-skipped documents in collection
      actualArr = actualArr.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
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
      assertEqual(actualArr, expected);
    },

    testOutputDocumentCollectionWithLimit2: function () {
      // skip 4 first values (therefore 1 complete document will be skipped), return 4.
      const actual = queryDocumentsWithLimit(docCollection, 4, 4, true, ["+replace-entries-with-object-iteration"]);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[docCollection].count() * 4);

      let actualArr = actual.toArray();
      // Check that system fields are present
      let systemFields = actualArr.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      // First 3 documents were skipped (offset == 12)
      assertEqual(systemFields.length, 3 * 1); // 3 system fields * 1 non-skipped documents in collection
      actualArr = actualArr.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
      let expected = [
        "key2",
        "b"
      ];
      assertEqual(actualArr, expected);
    },

    testOutputDocumentCollectionWithLimit3: function () {
      // skip 5 (odd number) first values (therefore 1 complete document will be skipped. 
      // The first system field of the second document is also skipped), return 4.
      const actual = queryDocumentsWithLimit(docCollection, 5, 4, true, ["+replace-entries-with-object-iteration"]);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[docCollection].count() * 4);

      let actualArr = actual.toArray();
      // Check that system fields are present
      let systemFields = actualArr.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      assertEqual(systemFields.length, 3); // 3 system fields 
      actualArr = actualArr.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
      let expected = [
        "key2",
        "b"
      ];
      assertEqual(actualArr, expected);
    },

    testOutputDocumentCollectionWithLimit4: function () {
      // skip 400 first values (therefore all documents will be skipped), return 4.
      const actual = queryDocumentsWithLimit(docCollection, 400, 4, true, ["+replace-entries-with-object-iteration"]);

      // Check fullCount value
      assertEqual(actual.getExtra()["stats"]["fullCount"], db[docCollection].count() * 4);

      let actualArr = actual.toArray();
      assertTrue(actualArr.length === 0);
    },

    testOptimizationFilter: function () {
      const query = `
      FOR doc IN ${docCollection}
        FOR [key, value] IN ENTRIES(doc)
        FILTER key == 'key1' and value == 'a'
        return [key, value]
      `;
  
      const actual_optimized = queryDocuments(docCollection, query, /*fullCount=*/true, ["+replace-entries-with-object-iteration"]);
      const actual_not_optimized = queryDocuments(docCollection, query, /*fullCount=*/true, ["-replace-entries-with-object-iteration"]);

      assertEqual(actual_optimized.getExtra()["stats"]["fullCount"], actual_not_optimized.getExtra()["stats"]["fullCount"]);
      assertEqual(actual_optimized.toArray(), actual_not_optimized.toArray());
    },

    testOptimizationFilterAndLimit: function () {
      const query = `
      FOR doc IN ${docCollection}
        FOR [key, value] IN ENTRIES(doc)
        FILTER key == '_key' or value == 'a'
        Limit 3,3
        return [key, value]
      `;
  
      const actual_optimized = queryDocuments(docCollection, query, /*fullCount=*/true, ["+replace-entries-with-object-iteration"]);
      const actual_not_optimized = queryDocuments(docCollection, query, /*fullCount=*/true, ["-replace-entries-with-object-iteration"]);

      assertEqual(actual_optimized.getExtra()["stats"]["fullCount"], actual_not_optimized.getExtra()["stats"]["fullCount"]);
      assertEqual(actual_optimized.toArray(), actual_not_optimized.toArray());
    },

    testOptimizationFilterSkipAll: function () {
      const query = `
      FOR doc IN ${docCollection}
        FOR [key, value] IN ENTRIES(doc)
        FILTER key == '_key' or key == '_id'
        Limit 300,3
        return [key, value]
      `;
  
      const actual_optimized = queryDocuments(docCollection, query, /*fullCount=*/true, ["+replace-entries-with-object-iteration"]);
      const actual_not_optimized = queryDocuments(docCollection, query, /*fullCount=*/true, ["-replace-entries-with-object-iteration"]);

      assertEqual(actual_optimized.getExtra()["stats"]["fullCount"], actual_not_optimized.getExtra()["stats"]["fullCount"]);
      assertEqual(actual_optimized.toArray(), actual_not_optimized.toArray());
    },

    testOptimizedAndNonOptimized: function () {
      const query = `
      FOR doc IN ${docCollection}
        FOR [key, value] IN ENTRIES(doc)
        FILTER key == '_key' or key == '_id'
        Limit 4,3
        return [key, value]
      `;
  
      const actual_optimized = queryDocuments(docCollection, query, /*fullCount=*/true, ["+replace-entries-with-object-iteration", "+move-filters-into-enumerate"]);
      const actual_not_optimized = queryDocuments(docCollection, query, /*fullCount=*/true, ["-replace-entries-with-object-iteration", "-move-filters-into-enumerate"]);

      assertEqual(actual_optimized.getExtra()["stats"]["fullCount"], actual_not_optimized.getExtra()["stats"]["fullCount"]);
      assertEqual(actual_optimized.toArray(), actual_not_optimized.toArray());
    }
  };
}

function enumerateObjectLimitTestSuite() {

  const database = "ObjectEnumLimitDB";
  const collection = "c";
  const numDocuments = 100;
  const keysString = "abcdefghijklmnopqrstuvwxyz";

  const totalPairs = [];
  for (let k = 0; k < numDocuments; k++) {
    for (const c of keysString) {
      totalPairs.push([c, k]);
    }
  }

  let testSuite = {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);

      db._create(collection);

      const docs = [];
      for (let k = 0; k < numDocuments; k++) {
        const doc = Object.fromEntries(Array.from(keysString, (c) => [c, k]));
        docs.push({_key: `D${k}`, obj: doc});
      }
      db[collection].save(docs);
    },
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    }
  };

  const offsets = [0, 1, 5, 12, 78, 300, 345, 1003];
  const counts = [1, 5, 80, 501, 1005, 2000, 6000];

  for (const offset of offsets) {
    for (const count of counts) {

      testSuite[`testObjectEnumLimit_${offset}_${count}`] = function () {
        const query = `
          FOR doc IN ${collection}
            FOR [k, v] IN ENTRIES(doc.obj)
              LIMIT ${offset}, ${count}
              RETURN [k, v]
        `;

        const actualResult = db._query(query).toArray();
        const expectedResult = totalPairs.slice(offset, offset + count);
        assertEqual(actualResult, expectedResult);
      };
    }
  }

  return testSuite;
}

jsunity.run(enumerateObjectTestSuite);
jsunity.run(enumerateObjectLimitTestSuite);

return jsunity.done();
