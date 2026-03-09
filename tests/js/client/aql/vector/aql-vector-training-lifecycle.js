/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse, print */

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

const jsunity = require("jsunity");
const db = require("internal").db;
const {
    randomNumberGeneratorFloat,
    randomInteger,
} = require("@arangodb/testutils/seededRandom");
const {
    waitForVectorIndexState,
    waitForAllVectorIndexesTrainingStateOnDBServers,
} = require("@arangodb/testutils/vector-index-common");
const isCluster = require("internal").isCluster();

const dbName = "vectorTrainingLifecycleDB";
const collName = "coll";
const dimension = 100;

function generateDocs(gen, count) {
    let docs = [];
    for (let i = 0; i < count; ++i) {
        docs.push({vector: Array.from({length: dimension}, () => gen())});
    }
    return docs;
}

function waitForState(collection, state, timeoutSec) {
    if (isCluster) {
        return waitForAllVectorIndexesTrainingStateOnDBServers(
            db, collection, state, timeoutSec);
    }
    return waitForVectorIndexState(collection, "vec_l2", state, timeoutSec);
}

function VectorIndexTrainingLifecycleSuite() {
    let collection;
    const seed = randomInteger();
    const insertCountFactor = isCluster ? 3 : 1;

    return {
        setUp: function () {
            print("Using seed: " + seed);
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);
            collection = db._create(collName, {numberOfShards: 3});

            collection.ensureIndex({
                name: "vec_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {metric: "l2", dimension, nLists: 1},
            });
        },

        tearDown: function () {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testIndexStartsUntrained: function () {
            const idx = collection.indexes().find(i => i.name === "vec_l2");
            assertFalse(idx.isTrained, "Index should be untrained on empty collection");
        },

        testStaysUntrainedBelowThreshold: function () {
            let gen = randomNumberGeneratorFloat(seed);
            collection.insert(generateDocs(gen, 500));
            assertEqual(500, collection.count());

            assertTrue(
                waitForState(collection, "untrained", 5),
                "Index should remain untrained with only 500 docs (threshold ~1000)"
            );
        },

        testTrainingTriggeredAfterThreshold: function () {
            assertFalse(
                collection.indexes().find(i => i.name === "vec_l2").isTrained,
                "Index should start untrained"
            );

            let gen = randomNumberGeneratorFloat(seed);
            const insertCount = 1500 * insertCountFactor;
            collection.insert(generateDocs(gen, insertCount));
            assertEqual(insertCount, collection.count());

            assertTrue(
                waitForState(collection, "ready", 60),
                "Index should become trained after " + insertCount +
                " docs (threshold ~1000)"
            );
        },

        testSearchWorksAfterDeferredTraining: function () {
            let gen = randomNumberGeneratorFloat(seed);
            const insertCount = 1500 * insertCountFactor;
            let docs = [];
            let queryPoint;
            for (let i = 0; i < insertCount; ++i) {
                const vector = Array.from({length: dimension}, () => gen());
                if (i === 50) {
                    queryPoint = vector;
                }
                docs.push({vector});
            }
            collection.insert(docs);

            assertTrue(
                waitForState(collection, "ready", 60),
                "Index should become trained within 60s"
            );

            const results = db._query(
                "FOR d IN " + collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN d",
                {qp: queryPoint}
            ).toArray();

            assertEqual(5, results.length, "Should return 5 nearest neighbors");
        },

        testTrainingTriggeredByIncrementalBatches: function () {
            let gen = randomNumberGeneratorFloat(seed);
            const batchSize = 200;
            const numBatches = Math.ceil((1500 * insertCountFactor) / batchSize);

            for (let batch = 0; batch < numBatches; ++batch) {
                collection.insert(generateDocs(gen, batchSize));
            }

            assertEqual(batchSize * numBatches, collection.count());

            assertTrue(
                waitForState(collection, "ready", 60),
                "Index should become trained after " + (batchSize * numBatches) +
                " docs inserted in " + numBatches + " batches"
            );
        },

        testTruncatePreventsSpuriousTraining: function () {
            let gen = randomNumberGeneratorFloat(seed);

            collection.insert(generateDocs(gen, 900));
            assertEqual(900, collection.count());

            collection.truncate();
            assertEqual(0, collection.count());

            collection.insert(generateDocs(gen, 100));
            assertEqual(100, collection.count());

            assertTrue(
                waitForState(collection, "untrained", 5),
                "Index should remain untrained after truncate + 100 docs " +
                "(total seen 1000 but only 100 exist)"
            );
        },

        testRangeDeleteTruncateResetsTrainingState: function () {
            let gen = randomNumberGeneratorFloat(seed);
            const largeCount = 33000;
            const batchSize = 1000;

            for (let i = 0; i < largeCount; i += batchSize) {
                const count = Math.min(batchSize, largeCount - i);
                collection.insert(generateDocs(gen, count));
            }
            assertEqual(largeCount, collection.count());

            assertTrue(
                waitForState(collection, "ready", 120),
                "Index should become trained after " + largeCount + " docs"
            );

            collection.truncate();
            assertEqual(0, collection.count());

            collection.insert(generateDocs(gen, 100));
            assertEqual(100, collection.count());

            assertTrue(
                waitForState(collection, "untrained", 5),
                "Index should be untrained after range-delete truncate + 100 docs"
            );
        },
    };
}

jsunity.run(VectorIndexTrainingLifecycleSuite);

return jsunity.done();
