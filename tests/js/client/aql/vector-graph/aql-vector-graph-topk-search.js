/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

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
const db = internal.db;
const isEnterprise = internal.isEnterprise();

const dbName = "vectorGraphDB";
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorGraphIndexTopKSearchTestSuite() {
    let collection;

    const search = function(query, k) {
        const query_ = "FOR d IN " + collName +
            " SORT APPROX_NEAR_L2(d.vector, @q) LIMIT " + k + " RETURN d._key";
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

        testTopKReturnsExactlyKDocuments: function() {
            assertEqual(10, search(point(20), 10).length);
        },

        testQueryingAnIndexedPointReturnsItAsTheNearest: function() {
            assertEqual(["p5"], search(point(5), 1));
        },

        testTopKReturnsTheKNearestNeighbours: function() {
            // point(5)'s neighbours are p4 and p6 (both at distance 1), then p3/p7.
            const keys = search(point(5), 3);
            assertEqual(3, keys.length);
            assertEqual(["p4", "p5", "p6"], keys.slice().sort());
        },

        testLimitLargerThanCollectionReturnsEveryDocument: function() {
            assertEqual(docCount, search(point(0), docCount * 10).length);
        },
    };
}

if (isEnterprise) {
    jsunity.run(VectorGraphIndexTopKSearchTestSuite);
}

return jsunity.done();
