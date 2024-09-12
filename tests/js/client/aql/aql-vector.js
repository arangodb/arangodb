/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue */

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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const db = require("internal").db;

const cn = "UnitTestsCollection";
const idxName = "vectorIndex";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexL2TestSuite() {
  let collection;
  let randomPoint;
  const dimension = 500;

  return {
    setUp: function () {
      db._drop(cn);
      collection = db._create(cn);

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        const vector = Array.from({ length: dimension }, () => Math.random());
        if (i == 500) {
          randomPoint = vector;
        }
        docs.push({ vector: vector });
      }
      collection.insert(docs);

      collection.ensureIndex({
        name: "vector_l2",
        type: "vector",
        fields: ["vector"],
        params: { metric: "l2", dimensions: dimension, nLists: 10 },
      });
    },

    tearDown: function () {
      internal.db._drop(cn);
    },

    testApproxNearMultipleTopK: function () {
      const query =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.vec, @qp) LIMIT @topK RETURN d.vector";

      const topKs = [1, 5, 10, 15, 50, 100];
      for (let i = 0; i < topKs.length; ++i) {
        const bindVars = { 'qp': randomPoint, 'topK': topKs[i] };
        const plan = db
          ._createStatement({
            query: query,
            bindVars: bindVars,
          })
          .explain().plan;
        const indexNodes = plan.nodes.filter(function (n) {
          return n.type === "EnumerateNearVectorNode";
        });
        assertEqual(1, indexNodes.length);

        const results = db._query(query, bindVars).toArray();
        print(results);
        assertEqual(topKs[i], results.length);
      }
    },
  };
}

jsunity.run(VectorIndexL2TestSuite);

return jsunity.done();
