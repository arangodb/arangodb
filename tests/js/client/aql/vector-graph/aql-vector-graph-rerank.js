/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const errors = internal.errors;
const db = internal.db;
const isEnterprise = internal.isEnterprise();

const dbName = "vectorGraphRerankDB";
const collName = "vectorGraphColl";
const dimension = 32;
const docCount = 64;

// Each document lives at a distinct position on the first axis (all other
// components are zero), so the L2 nearest neighbours of point(i) are the
// documents with the closest index -- an easy, deterministic ground truth.
function point(i) {
    const vector = new Array(dimension).fill(0.0);
    vector[0] = i * 1.0;
    return vector;
}

function VectorGraphIndexRerankTestSuite() {
    let collection;

    const search = function(query, k, options) {
        const params = options === undefined ? "" : ", " + JSON.stringify(options);
        const query_ = "FOR d IN " + collName +
            " SORT APPROX_NEAR_L2(d.vector, @q" + params + ") LIMIT " + k +
            " RETURN d._key";
        return db._query(query_, { q: query }).toArray();
    };

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            try {
                db._dropDatabase(dbName);
            } catch (e) {}
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName);
            collection.ensureIndex({
                name: "vg_idx",
                type: "vector-graph",
                fields: ["vector"],
                params: { dimension, metric: "l2" },
            });

            let docs = [];
            for (let i = 0; i < docCount; ++i) {
                docs.push({ _key: "p" + i, vector: point(i) });
            }
            collection.insert(docs);
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testRerankOnByDefaultReturnsTheNearestNeighbours: function() {
            const keys = search(point(5), 3);
            assertEqual(["p4", "p5", "p6"], keys.slice().sort());
        },

        testRerankExplicitlyOnReturnsTheNearestNeighbours: function() {
            const keys = search(point(5), 3, { rerank: true });
            assertEqual(["p4", "p5", "p6"], keys.slice().sort());
        },

        testRerankOffStillReturnsExactlyKDocuments: function() {
            assertEqual(10, search(point(20), 10, { rerank: false }).length);
        },

        testRerankOffReturnsTheNearestNeighbours: function() {
            // On this trivially separable data the approximate order equals the
            // exact one, so skipping the rerank must not change the result.
            const keys = search(point(5), 3, { rerank: false });
            assertEqual(["p4", "p5", "p6"], keys.slice().sort());
        },

        testSearchListSizeIsAccepted: function() {
            const keys = search(point(5), 3, { searchListSize: 50, rerank: false });
            assertEqual(["p4", "p5", "p6"], keys.slice().sort());
        },

        testPlanCarriesGraphVectorSearchParameters: function() {
            const query = "FOR d IN " + collName +
                " SORT APPROX_NEAR_L2(d.vector, @q, {rerank: false}) LIMIT 3 RETURN d._key";
            const plan = db._createStatement({
                query,
                bindVars: { q: point(5) },
            }).explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);
            assertEqual(false, indexNodes[0].searchParameters.rerank);
        },

        testNProbeIsRejectedForGraphVectorIndex: function() {
            // nProbe belongs to the IVF index family; the vector-graph index must
            // reject it rather than silently ignore it.
            try {
                search(point(5), 3, { nProbe: 4 });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_QUERY_PARSE.code, e.errorNum);
            }
        },
    };
}

if (isEnterprise) {
    jsunity.run(VectorGraphIndexRerankTestSuite);
}

return jsunity.done();
