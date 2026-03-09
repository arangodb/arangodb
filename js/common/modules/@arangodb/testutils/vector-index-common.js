/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global print */

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

const {
    randomNumberGeneratorFloat,
    randomInteger,
} = require("@arangodb/testutils/seededRandom");

const DistanceFunctions = {
    cosineSimilarity: function(vec1, vec2) {
        if (vec1.length !== vec2.length) {
            throw new Error("Vectors must have the same length");
        }

        let dotProduct = 0;
        let norm1 = 0;
        let norm2 = 0;

        for (let i = 0; i < vec1.length; i++) {
            dotProduct += vec1[i] * vec2[i];
            norm1 += vec1[i] * vec1[i];
            norm2 += vec2[i] * vec2[i];
        }

        norm1 = Math.sqrt(norm1);
        norm2 = Math.sqrt(norm2);

        if (norm1 === 0 || norm2 === 0) {
            return 0;
        }

        return dotProduct / (norm1 * norm2);
    },

    cosineDistance: function(vec1, vec2) {
        // This will now reference the single, correct DistanceFunctions object
        return 1 - DistanceFunctions.cosineSimilarity(vec1, vec2);
    },

    l2Distance: function(vec1, vec2) {
        if (vec1.length !== vec2.length) {
            throw new Error("Vectors must have the same length");
        }

        let sum = 0;
        for (let i = 0; i < vec1.length; i++) {
            const diff = vec1[i] - vec2[i];
            sum += diff * diff;
        }

        return Math.sqrt(sum);
    },

    dotProduct: function(vec1, vec2) {
        if (vec1.length !== vec2.length) {
            throw new Error("Vectors must have the same length");
        }

        let sum = 0;
        for (let i = 0; i < vec1.length; i++) {
            sum += vec1[i] * vec2[i];
        }

        return sum;
    }
};

// When similarity scores/distances are too close to each other,  
// vector search results can become non-deterministic due to floating-point 
// precision issues. This causes test failures where document ordering 
// changes between test runs. We ensure minimum distance separation between 
// vectors' distances function values from the query point to guarantee deterministic results.
function createVectorGenerator(options) {
    const {
        dimension = 500,
            numberOfDocs = 1000,
            seed = randomInteger(),
            floatEpsilon = 0.0000001,
            maxAttemptsPerDoc = 100,
            distanceFunction,
            randomGenerator = randomNumberGeneratorFloat
    } = options;

    if (!distanceFunction) {
        throw new Error("distanceFunction is required");
    }

    let randomPoint = null;
    const distancesFromRandomPoint = [];
    const gen = randomGenerator(seed);

    function findProximity(targetDistance) {
        return distancesFromRandomPoint.some(existingDistance => 
            Math.abs(existingDistance - targetDistance) <= floatEpsilon
        );
    }

    function insertSorted(value) {
        let low = 0;
        let high = distancesFromRandomPoint.length;
        // Binary search to find the correct insertion point
        while (low < high) {
            const mid = Math.floor((low + high) / 2);
            if (distancesFromRandomPoint[mid] < value) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        // Insert the value at the found position, maintaining sort order
        distancesFromRandomPoint.splice(low, 0, value);
    }

    function generateVector() {
        return Array.from({
            length: dimension
        }, () => gen());
    }

    function generateAllVectors() {
        const vectors = [];
        const docs = [];
        let successfulGenerations = 0;

        for (let i = 0; i < numberOfDocs; i++) {
            let attempts = 0;
            let vector = null;
            let isTooClose = false;
            let vectorGenerated = false;

            while (attempts < maxAttemptsPerDoc) {
                vector = generateVector();
                isTooClose = false;

                // Set the randomPoint (reference point for distance calculations)
                if (i === 0) {
                    randomPoint = vector;
                    // Add the distance from the random point to itself as a baseline
                    const selfDistance = distanceFunction(vector, randomPoint);
                    insertSorted(selfDistance);
                    vectors.push(vector);
                    docs.push({
                        vector: vector,
                        nonVector: i,
                        unIndexedVector: vector
                    });
                    successfulGenerations++;
                    vectorGenerated = true;
                    break;
                }

                const currentDistance = distanceFunction(vector, randomPoint);
                isTooClose = findProximity(currentDistance);

                if (!isTooClose) {
                    // If the vector is suitable, add its distance to our sorted array for future checks.
                    insertSorted(currentDistance);
                    vectors.push(vector);
                    docs.push({
                        vector: vector,
                        nonVector: i,
                        unIndexedVector: vector
                    });
                    successfulGenerations++;
                    vectorGenerated = true;
                    break; // Found a suitable vector, exit inner loop
                }
                attempts++;
            }

            if (attempts === maxAttemptsPerDoc) {
                print(`Warning: Could not generate a sufficiently unique vector in iteration ${i} after ${maxAttemptsPerDoc} attempts. Skipping this vector.`);
                // Don't add the vector to the result arrays since it doesn't meet distance requirements
                // Add the distance of the failed vector to prevent future vectors from being too close
                const currentDistance = distanceFunction(vector, randomPoint);
                insertSorted(currentDistance);
                // Don't increment successfulGenerations since this vector doesn't meet distance requirements
            }
        }

        if (successfulGenerations !== numberOfDocs) {
            print(`Warning: Only generated ${successfulGenerations} vectors with sufficient distance separation out of ${numberOfDocs} requested. Consider adjusting parameters.`);
        }

        return {
            vectors: vectors,
            docs: docs,
            randomPoint: randomPoint,
            seed: seed,
            dimension: dimension,
            numberOfDocs: vectors.length,
            floatEpsilon: floatEpsilon
        };
    }

    return {
        generateAllVectors: generateAllVectors,
        generateVector: generateVector,
        getRandomPoint: () => randomPoint,
        getSeed: () => seed,
        getDimension: () => dimension,
        getFloatEpsilon: () => floatEpsilon
    };
}

function withSuffix(suite, suffix) {
    const result = {};
    for (const [key, value] of Object.entries(suite)) {
        if (key.startsWith('test')) {
            result[key + suffix] = value;
        } else {
            result[key] = value;
        }
    }
    return result;
}

const sleepIntervalSec = 0.1;

/**
 * Waits until the named vector index on the collection reaches the given training state.
 * @param {ArangoCollection} collection - collection that has the vector index
 * @param {string} indexName - name of the vector index to wait for
 * @param {string} state - desired training state: "ready" or "untrained"
 * @param {number} timeoutSec - max time to wait in seconds
 * @returns {boolean} true if the vector index reached the state within the timeout
 */
function waitForVectorIndexState(collection, indexName, state, timeoutSec = 20) {
    const internal = require("internal");
    const iterations = Math.floor(timeoutSec / sleepIntervalSec);
    for (let i = 0; i < iterations; i++) {
        const idx = collection.indexes().find(
            ix => ix.type === 'vector' && ix.name === indexName);
        if (idx && idx.trainingState === state) {
            return true;
        }
        internal.sleep(sleepIntervalSec);
    }
    return false;
}

function waitForAllVectorIndexesTrainingState(collection, trainingState, timeoutSec = 20) {
    const internal = require("internal");
    const iterations = Math.floor(timeoutSec / sleepIntervalSec);
    for (let i = 0; i < iterations; i++) {
        const vectorIndexes = collection.indexes().filter(idx => idx.type === 'vector');
        if (vectorIndexes.length > 0 && vectorIndexes.every(idx => idx.trainingState === trainingState)) {
            return true;
        }
        internal.sleep(sleepIntervalSec);
    }
    return false;
}

/**
 * In cluster mode: waits until all vector indexes on all DB servers (shards) reach
 * the given training state by querying each DB server via the HTTP API (no reconnect).
 * @param {ArangoDatabase} db - database (e.g. internal.db)
 * @param {ArangoCollection} collection - collection that has vector index(es)
 * @param {string} trainingState - desired training state: "ready" or "untrained"
 * @param {number} timeoutSec - max time to wait in seconds
 * @returns {boolean} true if all vector indexes on all shards reached the state within the timeout
 */
function waitForAllVectorIndexesTrainingStateOnDBServers(db, collection, trainingState, timeoutSec = 20) {
    const internal = require("internal");
    const request = require("@arangodb/request");
    const testHelper = require("@arangodb/test-helper");
    const getEndpointById = testHelper.getEndpointById;

    const dbName = db._name();
    const shardMap = collection.shards(true);
    if (!shardMap || Object.keys(shardMap).length === 0) {
        return false;
    }

    const serverToShards = {};
    for (const [shard, servers] of Object.entries(shardMap)) {
        const primaryId = servers[0];
        if (!serverToShards[primaryId]) {
            serverToShards[primaryId] = [];
        }
        serverToShards[primaryId].push(shard);
    }

    const indexApiPath = "/_db/" + encodeURIComponent(dbName) + "/_api/index";

    const iterations = Math.floor(timeoutSec / sleepIntervalSec);
    for (let iter = 0; iter < iterations; iter++) {
        let allMatch = true;
        for (const [serverId, shards] of Object.entries(serverToShards)) {
            const baseUrl = getEndpointById(serverId);
            if (!baseUrl) {
                allMatch = false;
                break;
            }
            for (const shardName of shards) {
                const url = baseUrl + indexApiPath + "?collection=" + encodeURIComponent(shardName);
                let res;
                try {
                    res = request({ method: "GET", url });
                } catch (e) {
                    allMatch = false;
                    break;
                }
                if (res.status !== 200 || !res.json || !res.json.indexes) {
                    allMatch = false;
                    break;
                }
                const vectorIndexes = res.json.indexes.filter(idx => idx.type === 'vector');
                if (vectorIndexes.length === 0 || !vectorIndexes.every(idx => idx.trainingState === trainingState)) {
                    allMatch = false;
                    break;
                }
            }
            if (!allMatch) {
                break;
            }
        }
        if (allMatch) {
            return true;
        }
        internal.sleep(sleepIntervalSec);
    }
    return false;
}

/**
 * Inserts docs in batches, calling ensureIndex() at a random batch slot
 * determined by the seed. This tests that index creation works regardless of
 * whether it happens before, during, or after data insertion.
 *
 * @param {object} opts
 * @param {ArangoCollection} opts.collection
 * @param {Array} opts.docs - documents to insert
 * @param {number} opts.seed - random seed (used to pick ensureIndex slot)
 * @param {function} opts.ensureIndex - callback that creates the index(es)
 * @param {number} [opts.batchSize=100]
 * @param {function} [opts.onBatchInserted] - called with insert result per batch
 */
function insertDocsAndEnsureIndex({collection, docs, seed, ensureIndex,
                                    batchSize = 100, onBatchInserted}) {
    const numBatches = Math.ceil(docs.length / batchSize);
    const ensureIndexSlot = Math.abs(seed) % (numBatches + 1);

    for (let i = 0; i < numBatches; i++) {
        if (i === ensureIndexSlot) {
            ensureIndex();
        }
        const start = i * batchSize;
        const end = Math.min(start + batchSize, docs.length);
        const result = collection.insert(docs.slice(start, end));
        if (onBatchInserted) {
            onBatchInserted(result);
        }
    }
    if (ensureIndexSlot === numBatches) {
        ensureIndex();
    }
}

/**
 * Waits until all vector indexes on the collection reach the expected training
 * state, handling both cluster and single-server modes.
 *
 * @param {ArangoCollection} collection
 * @param {string} expectedState - "ready" or "untrained"
 * @param {number} [timeoutSec=60]
 * @returns {boolean}
 */
function waitForIndexBuild(collection, expectedState, timeoutSec = 60) {
    const internal = require("internal");
    if (internal.isCluster()) {
        const db = internal.db;
        return waitForAllVectorIndexesTrainingStateOnDBServers(
            db, collection, expectedState, timeoutSec);
    }
    return waitForAllVectorIndexesTrainingState(
        collection, expectedState, timeoutSec);
}

exports.createVectorGenerator = createVectorGenerator;
exports.DistanceFunctions = DistanceFunctions;
exports.waitForVectorIndexState = waitForVectorIndexState;
exports.waitForAllVectorIndexesTrainingState = waitForAllVectorIndexesTrainingState;
exports.waitForAllVectorIndexesTrainingStateOnDBServers = waitForAllVectorIndexesTrainingStateOnDBServers;
exports.insertDocsAndEnsureIndex = insertDocsAndEnsureIndex;
exports.waitForIndexBuild = waitForIndexBuild;
exports.withSuffix = withSuffix;
