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

const dbName = "vectorGraphTombstonesDB";
const collName = "vectorGraphColl";
const dimension = 32;
const docCount = 1000;

// Documents sit on the first axis, so the L2 neighbours of point(x) are
// ordered by |i - x|. A fractional x keeps those distances free of ties.
function point(x) {
    const vector = new Array(dimension).fill(0.0);
    vector[0] = x;
    return vector;
}

function keysInRange(from, to) {
    let keys = [];
    for (let i = from; i < to; ++i) {
        keys.push("p" + i);
    }
    return keys;
}

function VectorGraphIndexTombstonesTestSuite() {
    let collection;

    // Flushed segments score by PQ distance, which cannot tell neighbouring
    // points apart, so the exact rerank needs a wide candidate pool.
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

    const insertRange = function(from, to) {
        let docs = [];
        for (let i = from; i < to; ++i) {
            docs.push({ _key: "p" + i, vector: point(i) });
        }
        collection.insert(docs);
    };

    const assertSearchSkipsDeletedNeighbours = function() {
        assertEqual(["p50", "p51", "p49"], search(point(50.4), 3));
        collection.remove(["p50", "p51"]);
        assertEqual(["p49", "p52", "p48"], search(point(50.4), 3));
    };

    const assertEveryRemainingPointFindsItself = function(deleted) {
        for (let i = 0; i < docCount; ++i) {
            if (!deleted.has("p" + i)) {
                assertEqual(["p" + i], search(point(i), 1), "point " + i);
            }
        }
        const all = search(point(0), docCount * 2);
        assertEqual(docCount - deleted.size, all.length);
        assertEqual([], all.filter((key) => deleted.has(key)));
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

        testDeletedBufferedDocumentsAreNotReturned: function() {
            insertRange(0, docCount);
            assertSearchSkipsDeletedNeighbours();
        },

        testDeletedFlushedDocumentsAreNotReturned: function() {
            insertRange(0, docCount);
            flush();
            assertSearchSkipsDeletedNeighbours();
        },

        testDeletesAcrossFlushedAndBufferedDocuments: function() {
            insertRange(0, docCount / 2);
            flush();
            insertRange(docCount / 2, docCount);

            const deleted = new Set(keysInRange(docCount / 2 - 20,
                                                docCount / 2 + 20));
            db._query(aql`
                FOR key IN ${Array.from(deleted)}
                  REMOVE key IN ${collection}`);

            assertEveryRemainingPointFindsItself(deleted);
        },

        testDeletedDocumentsStayHiddenAfterFlush: function() {
            insertRange(0, docCount);
            const deleted = new Set(keysInRange(0, docCount / 2));
            collection.remove(Array.from(deleted));
            flush();

            assertEveryRemainingPointFindsItself(deleted);
        },

        testDeletingEveryDocumentYieldsNoResults: function() {
            insertRange(0, docCount);
            flush();
            assertEqual(10, search(point(500), 10).length);

            db._query(aql`FOR d IN ${collection} REMOVE d IN ${collection}`);
            assertEqual([], search(point(500), 10));
        },

        testReinsertedDocumentIsFoundAgain: function() {
            insertRange(0, docCount);
            flush();
            collection.remove("p50");
            assertEqual(["p51", "p49"], search(point(50.4), 2));

            collection.insert({ _key: "p50", vector: point(50) });
            assertEqual(["p50", "p51", "p49"], search(point(50.4), 3));
        },
    };
}

if (isEnterprise) {
    jsunity.run(VectorGraphIndexTombstonesTestSuite);
}

return jsunity.done();
