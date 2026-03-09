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

function VectorIndexRemainsUntrainedSuite() {
    let collection;
    const dimension = 100;
    const seed = randomInteger();
    // nLists=1 gives threshold = max(1*39, 1000) = 1000
    // Insert fewer than threshold so training never triggers.
    const insertCount = 500;

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
            let docs = [];
            for (let i = 0; i < insertCount; ++i) {
                docs.push({vector: Array.from({length: dimension}, () => gen())});
            }
            collection.insert(docs);

            assertEqual(insertCount, collection.count());

            const trainingState = "untrained";
            const waitTimeoutSec = 5;
            if (isCluster) {
                assertTrue(
                    waitForAllVectorIndexesTrainingStateOnDBServers(db, collection, trainingState, waitTimeoutSec),
                    "Index should remain untrained with only " + insertCount + " docs (threshold ~1000)"
                );
            } else {
                const trained = waitForVectorIndexState(collection, "vec_l2", trainingState, waitTimeoutSec);
                assertTrue(trained,
                    "Index should remain untrained with only " + insertCount + " docs (threshold ~1000)");
            }
        },
    };
}

function VectorIndexDeferredTrainingSuite() {
    let collection;
    const dimension = 100;
    const seed = randomInteger();
    // nLists=1 gives threshold = max(1*39, 1000) = 1000
    // Insert well above threshold so training triggers.
    const insertCountFactor = isCluster ? 3 : 1;
    const insertCount = 1500 * insertCountFactor;

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

        testTrainingTriggeredAfterThreshold: function () {
            const idxBefore = collection.indexes().find(i => i.name === "vec_l2");
            assertFalse(idxBefore.isTrained, "Index should start untrained");

            let gen = randomNumberGeneratorFloat(seed);
            let docs = [];
            for (let i = 0; i < insertCount; ++i) {
                docs.push({vector: Array.from({length: dimension}, () => gen())});
            }
            collection.insert(docs);

            assertEqual(insertCount, collection.count());

            const trainingState = "ready";
            const waitTimeoutSec = 60;
            if (isCluster) {
                assertTrue(
                    waitForAllVectorIndexesTrainingStateOnDBServers(db, collection, trainingState, waitTimeoutSec),
                    "Index should become trained after inserting " + insertCount +
                    " docs (threshold ~1000), waited up to 60s"
                );
            } else {
                const trained = waitForVectorIndexState(collection, "vec_l2", trainingState, waitTimeoutSec);
                assertTrue(trained,
                    "Index should become trained after inserting " + insertCount +
                    " docs (threshold ~1000), waited up to 60s");
            }
        },

        testSearchWorksAfterDeferredTraining: function () {
            let gen = randomNumberGeneratorFloat(seed);
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

            const trainingState = "ready";
            const waitTimeoutSec = 60;
            if (isCluster) {
                assertTrue(
                    waitForAllVectorIndexesTrainingStateOnDBServers(db, collection, trainingState, waitTimeoutSec),
                    "Index should become trained within 60s"
                );
            } else {
                const trained = waitForVectorIndexState(collection, "vec_l2", trainingState, waitTimeoutSec);
                assertTrue(trained, "Index should become trained within 60s");
            }

            const results = db._query(
                "FOR d IN " + collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN d",
                {qp: queryPoint}
            ).toArray();

            assertEqual(5, results.length, "Should return 5 nearest neighbors");
        },
    };
}

function VectorIndexBatchInsertTrainingSuite() {
    let collection;
    const dimension = 100;
    const seed = randomInteger();
    const batchSize = 200;
    const totalDocsFactor = isCluster ? 3 : 1;
    const numBatches = Math.ceil((1500 * totalDocsFactor) / 200); // 4500 cluster / 1500 single

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

        testTrainingTriggeredByIncrementalBatches: function () {
            let gen = randomNumberGeneratorFloat(seed);

            for (let batch = 0; batch < numBatches; ++batch) {
                let docs = [];
                for (let i = 0; i < batchSize; ++i) {
                    docs.push({vector: Array.from({length: dimension}, () => gen())});
                }
                collection.insert(docs);
            }

            assertEqual(batchSize * numBatches, collection.count());

            const trainingState = "ready";
            const waitTimeoutSec = 60;
            if (isCluster) {
                assertTrue(
                    waitForAllVectorIndexesTrainingStateOnDBServers(db, collection, trainingState, waitTimeoutSec),
                    "Index should become trained after " + (batchSize * numBatches) +
                    " docs inserted in " + numBatches + " batches"
                );
            } else {
                const trained = waitForVectorIndexState(collection, "vec_l2", trainingState, waitTimeoutSec);
                assertTrue(trained,
                    "Index should become trained after " + (batchSize * numBatches) +
                    " docs inserted in " + numBatches + " batches");
            }
        },
    };
}

jsunity.run(VectorIndexRemainsUntrainedSuite);
jsunity.run(VectorIndexDeferredTrainingSuite);
jsunity.run(VectorIndexBatchInsertTrainingSuite);

return jsunity.done();
