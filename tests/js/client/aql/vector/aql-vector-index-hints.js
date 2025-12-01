/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

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
/// @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const helper = require("@arangodb/aql-helper");
const errors = internal.errors;
const db = internal.db;
const {
  randomNumberGeneratorFloat,
  randomInteger,
} = require("@arangodb/testutils/seededRandom");

const dbName = "vectorIndexHintDb";
const collName = "vectorIndexHintColl";

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to get the used vector index from a query plan
////////////////////////////////////////////////////////////////////////////////
function getVectorIndexName(query, bindVars = {}) {
  const explain = db._createStatement({ query, bindVars }).explain();
  const vectorNodes = explain.plan.nodes.filter(
    (node) => node.type === "EnumerateNearVectorNode"
  );
  if (vectorNodes.length === 0) {
    return null;
  }
  assertEqual(1, vectorNodes.length, "Expected exactly one EnumerateNearVectorNode");
  return vectorNodes[0].index ? vectorNodes[0].index.name : null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test suite for vector index hints
////////////////////////////////////////////////////////////////////////////////
function VectorIndexHintsSuite() {
  let collection;
  let randomPoint;
  const dimension = 128;
  const numberOfDocs = 100;
  const seed = randomInteger();

  return {
    setUpAll: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      collection = db._create(collName, { numberOfShards: 3 });

      // Generate random vectors
      let docs = [];
      let gen = randomNumberGeneratorFloat(seed);
      for (let i = 0; i < numberOfDocs; ++i) {
        const vector = Array.from({ length: dimension }, () => gen());
        const vectorCosine = Array.from({ length: dimension }, () => gen());
        const vectorInnerProduct = Array.from({ length: dimension }, () => gen());
        
        if (i === Math.floor(numberOfDocs / 2)) {
          randomPoint = vector;
        }
        
        docs.push({
          _key: `doc${i}`,
          vector: vector,
          vectorCosine: vectorCosine,
          vectorInnerProduct: vectorInnerProduct,
          value: i,
        });
      }
      collection.insert(docs);

      collection.ensureIndex({
        name: "vector_l2",
        type: "vector",
        fields: ["vector"],
        params: {
          metric: "l2",
          dimension: dimension,
          nLists: 5,
        },
      });

      collection.ensureIndex({
        name: "vector_l2_secondary",
        type: "vector",
        fields: ["vector"],
        params: {
          metric: "l2",
          dimension: dimension,
          nLists: 3,
        },
      });
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testVectorL2HintFirstIndex: function () {
      const query = `
        FOR doc IN ${collName} OPTIONS { indexHint: "vector_l2" }
          SORT APPROX_NEAR_L2(doc.vector, @qp)
          LIMIT 5
          RETURN doc
      `;
      const bindVars = { qp: randomPoint };
      const indexName = getVectorIndexName(query, bindVars);
      
      assertEqual("vector_l2", indexName);
    },

    testVectorL2HintSecondIndex: function () {
        const query = `
          FOR doc IN ${collName} OPTIONS { indexHint: "vector_l2_secondary" }
            SORT APPROX_NEAR_L2(doc.vector, @qp)
            LIMIT 5
            RETURN doc
        `;
        const bindVars = { qp: randomPoint };
        const indexName = getVectorIndexName(query, bindVars);
        
        assertEqual("vector_l2_secondary", indexName);
      },

      testVectorL2HintNonExistingIndex: function () {
        const query = `
          FOR doc IN ${collName} OPTIONS { indexHint: "nonexistent_index" }
            SORT APPROX_NEAR_L2(doc.vector, @qp)
            LIMIT 5
            RETURN doc
        `;
        const bindVars = { qp: randomPoint };
        const indexName = getVectorIndexName(query, bindVars);
        
        assertTrue(["vector_l2_secondary", "vector_l2"].includes(indexName));
      },
  
    testVectorL2WithHintForced: function () {
      const query = `
        FOR doc IN ${collName} OPTIONS { indexHint: "vector_l2_secondary", forceIndexHint: true }
          SORT APPROX_NEAR_L2(doc.vector, @qp)
          LIMIT 5
          RETURN doc
      `;
      const bindVars = { qp: randomPoint };
      const indexName = getVectorIndexName(query, bindVars);
      
      assertEqual("vector_l2_secondary", indexName);
    },

    testVectorL2WithHintForcedNonExistingIndex: function () {
        const query = `
          FOR doc IN ${collName} OPTIONS { indexHint: "nonexistent_index", forceIndexHint: true }
            SORT APPROX_NEAR_L2(doc.vector, @qp)
            LIMIT 5
            RETURN doc
        `;
        const bindVars = { qp: randomPoint };
        
        try {
            db._createStatement({ query, bindVars }).explain();
            fail();
        } catch (err) {
            assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum);
        }
    },

    testVectorL2HintMultipleIndexes: function () {
        const query = `
          FOR doc IN ${collName} OPTIONS { indexHint: ["non_existing_index", "vector_l2_secondary"] }
            SORT APPROX_NEAR_L2(doc.vector, @qp)
            LIMIT 5
            RETURN doc
        `;
        const bindVars = { qp: randomPoint };
        const indexName = getVectorIndexName(query, bindVars);
        
        assertEqual("vector_l2_secondary", indexName);
    },
  };
}

jsunity.run(VectorIndexHintsSuite);

return jsunity.done();
