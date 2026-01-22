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

  const explainAbac = function (query, bindVars, options) {
    const explainRes = db._createStatement({query: query, bindVars: bindVars || {}, options: options || {}}).explain();
    return explainRes.abacAccesses || [];
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
      const plan = db._createStatement({query: query}).explain().plan;

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
    },

    testAbac_IndexSimpleProjection: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5
          RETURN doc.name
      `;
      const accesses = explainAbac(query);
      print(accesses);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("name"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexMultipleAttributes: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5
          RETURN {name: doc.name, category: doc.category}
      `;
      const accesses = explainAbac(query);
      print(accesses);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("name"));
      assertTrue(accesses[0].read.attributes.includes("category"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    // Using _id/_key explicitly is still attribute access.
    testAbac_IndexReturnIdAndKey: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5
          RETURN {id: doc._id, key: doc._key}
      `;
      const accesses = explainAbac(query);
      print(accesses);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("_id"));
      assertTrue(accesses[0].read.attributes.includes("_key"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexFullDocumentAccess: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5
          RETURN doc
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexFilterOnAttributeReturnFullDocument: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5
          RETURN doc
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexFilterWithComputation: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5 OR doc.value > 7
          RETURN doc.value
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collectionName);

      assertTrue(accesses[0].read.attributes.includes("value"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexFilterAndReturnDifferentAttributes1: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value IN [1, 5, 7]
          RETURN doc.category
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("category"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexFilterAndReturnDifferentAttributes2_SelfJoin: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          FOR q IN ${cn}
            FILTER q.value == p.value
            RETURN q.name
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("name"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexFilterAndReturnDifferentAttributes3_SelfJoinReturnDoc: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          FOR q IN ${cn}
            FILTER q.value == p.value
            RETURN q
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexCalculationWithAttributeAccess1: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          LET v2 = p.value * 2
          RETURN { name: p.name, v2 }
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("name"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexSortWithAttributes: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          SORT p.value DESC
          RETURN p.name
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("name"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexLimitDoesNotChangeProjection: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          LIMIT 2
          RETURN p.name
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("name"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexCollectByAttribute: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          COLLECT c = p.category
          RETURN c
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("category"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexCollectAggregateUsesAttribute: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          COLLECT AGGREGATE maxV = MAX(p.value)
          RETURN maxV
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_IndexReturnDistinctAttribute: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value IN [1, 5, 7]
          RETURN DISTINCT p.category
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("category"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_DynamicCollectionAccessWithBindParameters1: function () {
      const query = `
        FOR p IN @@coll
          FILTER p.value == 5
          RETURN p.name
      `;
      const accesses = explainAbac(query, {"@coll": cn});

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("name"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_DynamicAttributeAccessWithBindParameters1: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          RETURN p[@attr]
      `;
      const accesses = explainAbac(query, {attr: "name"});

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("category"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_DynamicAttributeAccessWithBindParameters2: function () {
      const query = `
        LET a = "category"
        FOR p IN ${cn}
          FILTER p.value == 5
          RETURN p[a]
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("category"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_DynamicAttributeAccessWithBindParameters3: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p[@attr] >= 5
          RETURN { name: p.name, category: p.category }
      `;
      const accesses = explainAbac(query, {attr: "value"});

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("category"));
      assertTrue(accesses[0].read.attributes.includes("name"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_MissingCollectionThrowsWithCorrectErrorCode: function () {
      // JS version: explain should throw TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
      let threw = false;
      try {
        explainAbac(`
          FOR p IN UnitTestsAttributeDetectorIndex_Missing
            FILTER p.value == 5
            RETURN p.name
        `);
      } catch (err) {
        threw = true;
        // Error shape differs depending on shell; be permissive but still check code.
        assertTrue(err.errorNum !== undefined);
        assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
      assertTrue(threw);
    },

    testAbac_MissingAttributeStillRecorded1: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value == 5
          RETURN p.attrNotExist
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("attrNotExist"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_MissingAttributeStillRecorded2: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value == 5 AND p.attrNotExist == 1
          RETURN p.name
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.attributes.includes("value"));
      assertTrue(accesses[0].read.attributes.includes("name"));
      assertTrue(accesses[0].read.attributes.includes("attrNotExist"));

      assertFalse(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    },

    testAbac_MissingAttributeStillRecorded3_ReturnDoc: function () {
      const query = `
        FOR p IN ${cn}
          FILTER p.value == 5 AND p.attrNotExist == 1
          RETURN p
      `;
      const accesses = explainAbac(query);

      assertEqual(1, accesses.length);
      assertEqual(cn, accesses[0].collection);

      assertTrue(accesses[0].read.requiresAll);
      assertFalse(accesses[0].write.requiresAll);
    }
  };
}

jsunity.run(attributeDetectorIndexStoredValuesTestSuite);

return jsunity.done();