/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global arango, assertEqual */

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
const { aql } = require("@arangodb");
const db = internal.db;
const isEnterprise = internal.isEnterprise();

const dbName = "vectorGraphConsolidationDB";
const collName = "vectorGraphColl";
const dimension = 32;
const docCount = 1000;
const updatedCount = 300;
const timeoutSeconds = 120;

// Each document lives at a distinct position on the first axis, so the L2
// nearest neighbour of point(i) is document p<i>.
function point(i) {
    const vector = new Array(dimension).fill(0.0);
    vector[0] = i * 1.0;
    return vector;
}

function VectorGraphIndexConsolidationTestSuite() {
    let collection;

    // Flushed segments score by PQ distance, which cannot tell neighbouring
    // points apart, so the exact rerank needs a wider candidate pool than a
    // LIMIT 1 query gives it.
    const search = function(query, k) {
        const options = { rerank: true, searchListSize: Math.max(64, k) };
        return db._query(aql`
            FOR d IN ${collection}
              SORT APPROX_NEAR_L2(d.vector, ${query}, ${options})
              LIMIT ${k}
              RETURN d._key`).toArray();
    };

    const flush = function() {
        const response = arango.POST(
            "/_admin/vector-graph/flush?collection=" + collName, {});
        assertEqual(1, response.result.flushed.length,
                    JSON.stringify(response));
    };

    const figures = function() {
        const index = collection.indexes(true)
            .find((idx) => idx.type === "vector-graph");
        return index.figures;
    };

    return {
        setUp: function() {
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
        },

        tearDown: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testUpdatesAreConsolidatedIntoOneSegment: function() {
            let docs = [];
            for (let i = 0; i < docCount; ++i) {
                docs.push({ _key: "p" + i, vector: point(i) });
            }
            collection.insert(docs);
            flush();
            assertEqual(1, figures().segmentCount);

            // An update is a remove plus an insert with a new document id: it
            // tombstones the old record and buffers a new one.
            db._query(aql`
                FOR d IN ${collection}
                  FILTER TO_NUMBER(SUBSTRING(d._key, 1)) < ${updatedCount}
                  UPDATE d WITH { updated: true } IN ${collection}`);
            flush();

            // The consolidator drops the tombstoned records and merges the
            // small segment of updated documents into the other one.
            const deadline = internal.time() + timeoutSeconds;
            let current = figures();
            while ((current.segmentCount !== 1 || current.tombstones !== 0) &&
                   internal.time() < deadline) {
                internal.sleep(0.5);
                current = figures();
            }
            assertEqual(1, current.segmentCount, JSON.stringify(current));
            assertEqual(0, current.tombstones, JSON.stringify(current));

            for (let i = 0; i < docCount; ++i) {
                assertEqual(["p" + i], search(point(i), 1), "point " + i);
            }
            assertEqual(docCount, search(point(0), docCount * 2).length);
        },
    };
}

if (isEnterprise) {
    jsunity.run(VectorGraphIndexConsolidationTestSuite);
}

return jsunity.done();
