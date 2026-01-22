/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = require("@arangodb").db;
const helper = require("@arangodb/aql-helper");

function attributeDetectorIndexStoredValuesTestSuite() {
  const cn = "UnitTestsAttributeDetectorIndex";
  let collection;

  const explain = function (query, bindVars, options) {
    return helper.removeClusterNodes(
      helper.getCompactPlan(
        db._createStatement({query: query, bindVars: bindVars || {}, options: options || {}}).explain()
      ).map(function(node) { return node.type; })
    );
  };

  const findIndexNode = function (plan) {
    const nodes = plan.nodes || [];
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].type === "IndexNode") {
        return nodes[i];
      }
    }
    return null;
  };

  return {
    setUp: function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      for (let i = 0; i < 10; ++i) {
        collection.insert({
          _key: `doc${i}`,
          value: i,
          name: `name${i}`,
          category: `cat${i % 3}`,
          data: {nested: i, extra: `extra${i}`}
        });
      }

      collection.ensureIndex({
        type: "persistent",
        fields: ["value"],
        storedValues: ["name", "category"]
      });
    },

    tearDown: function () {
      internal.db._drop(cn);
    },

    testPersistentIndexWithStoredValuesCovering: function () {
      const query = `FOR doc IN ${cn} FILTER doc.value == 5 RETURN {name: doc.name, category: doc.category}`;
      const explainRes = db._createStatement({query: query, options: {includeAbacAccesses: true}}).explain()
      const plan = explainRes.plan;

      print(JSON.stringify(explainRes, null, 2));

      print(explainRes.extras.abacAccesses);

      const indexNode = findIndexNode(plan);
      assertTrue(indexNode !== null, "IndexNode should exist");

      if (indexNode.indexes && indexNode.indexes.length > 0) {
        const usedIndex = indexNode.indexes[0];
        assertEqual(usedIndex.type, "persistent", "Should use persistent index");
      }
    },

    testPersistentIndexWithStoredValuesPartialCovering: function () {
      const query = `FOR doc IN ${cn} FILTER doc.value == 5 RETURN {name: doc.name, value: doc.value}`;
      const plan = db._createStatement({query: query}).explain().plan;

      const indexNode = findIndexNode(plan);
      assertTrue(indexNode !== null, "IndexNode should exist");
    },

    testPersistentIndexWithStoredValuesNonCovering: function () {
      const query = `FOR doc IN ${cn} FILTER doc.value == 5 RETURN {name: doc.name, data: doc.data}`;
      const plan = db._createStatement({query: query}).explain().plan;

      const indexNode = findIndexNode(plan);
      assertTrue(indexNode !== null, "IndexNode should exist");
    },

    testPersistentIndexStoredValuesOnly: function () {
      const query = `FOR doc IN ${cn} FILTER doc.value == 5 RETURN doc.name`;
      const plan = db._createStatement({query: query}).explain().plan;

      const indexNode = findIndexNode(plan);
      assertTrue(indexNode !== null, "IndexNode should exist");
    },

    testPersistentIndexWithStoredValuesAndFilter: function () {
      const query = `FOR doc IN ${cn} FILTER doc.value == 5 && doc.category == "cat1" RETURN doc.name`;
      const plan = db._createStatement({query: query}).explain().plan;

      const indexNode = findIndexNode(plan);
      assertTrue(indexNode !== null, "IndexNode should exist");
    },

    testPersistentIndexStoredValuesMultipleAttributes: function () {
      collection.ensureIndex({
        type: "persistent",
        fields: ["value", "category"],
        storedValues: ["name", "data"]
      });

      const query = `FOR doc IN ${cn} FILTER doc.value == 5 && doc.category == "cat1" RETURN {name: doc.name, data: doc.data}`;
      const plan = db._createStatement({query: query}).explain().plan;

      const indexNode = findIndexNode(plan);
      assertTrue(indexNode !== null, "IndexNode should exist");
    }
  };
}

jsunity.run(attributeDetectorIndexStoredValuesTestSuite);

return jsunity.done();
