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

const VectorIndexTrainingState = Object.freeze({
    kUnusable: "unusable",
    kTraining: "training",
    kIngesting: "ingesting",
    kReady: "ready",
});

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

const sleepIntervalSec = 0.1;

// Checks whether a single vector index has the expected training state.
// Single server: the index has a top-level `trainingState` field.
// Cluster: the index has a `shards` object where each shard has a `trainingState` field.
function indexMatchesState(idx, state) {
    if (idx.trainingState !== undefined) {
        // Single server: top-level trainingState.
        return idx.trainingState === state;
    }
    if (idx.shards !== undefined) {
        // Cluster: every shard must match.
        const shardEntries = Object.values(idx.shards);
        return shardEntries.length > 0 &&
            shardEntries.every(s => s.trainingState === state);
    }
    return false;
}

// Checks whether the given vector indexes match the expected state.
// If indexName is provided, only that index is checked; otherwise all vector indexes.
function vectorIndexesMatchState(indexes, state, indexName) {
    const vectorIndexes = indexes.filter(idx => idx.type === 'vector');
    if (indexName !== undefined) {
        const idx = vectorIndexes.find(ix => ix.name === indexName);
        return idx !== undefined && indexMatchesState(idx, state);
    }
    return vectorIndexes.length > 0 &&
        vectorIndexes.every(idx => indexMatchesState(idx, state));
}

// Waits until vector index(es) on the collection reach the given state.
// Uses collection.indexes(true, true) to include per-shard details in cluster mode.
// If indexName is provided, only that single index is checked.
function waitForState(collection, state, timeoutSec, indexName) {
    const internal = require("internal");
    const iterations = Math.floor(timeoutSec / sleepIntervalSec);
    for (let i = 0; i < iterations; i++) {
        const indexes = collection.indexes(false, true);
        if (vectorIndexesMatchState(indexes, state, indexName)) {
            return true;
        }
        internal.sleep(sleepIntervalSec);
    }
    return false;
}

/**
 * Inserts docs in batches, calling ensureIndex() at a random batch slot
 * determined by the seed. This tests that index creation works regardless of
 * whether it happens before, during, or after data insertion. After all docs
 * are inserted, waits for the vector index to reach the ready state.
 *
 * @param {object} opts
 * @param {ArangoCollection} opts.collection
 * @param {Array} opts.docs - documents to insert
 * @param {number} opts.seed - random seed (used to pick ensureIndex slot)
 * @param {object} opts.indexDef - index definition passed to ensureIndex()
 * @param {string} opts.indexName - name of the vector index to assert ready
 * @param {number} [opts.batchSize=100]
 * @param {function} [opts.onBatchInserted] - called with insert result per batch
 * @param {number} [opts.readyTimeoutSec=60] - timeout for waiting until the
 *   vector index reaches the ready state
 */
function insertDocsAndAssertIndex({collection, docs, seed, indexDef,
                                    indexName, batchSize = 100,
                                    onBatchInserted,
                                    readyTimeoutSec = 60}) {
    const numBatches = Math.ceil(docs.length / batchSize);
    const ensureIndexSlot = Math.abs(seed) % (numBatches + 1);
    const fullIndexDef = {name: indexName, ...indexDef};

    const tryEnsureIndex = () => {
        try {
            collection.ensureIndex(fullIndexDef);
        } catch (e) {
            // Index creation may fail if not enough data is present yet
            // (e.g. vector index training threshold not met on a shard).
            // The index still exists in unusable state and the background
            // build manager will retry once enough data arrives.
        }
    };

    for (let i = 0; i < numBatches; i++) {
        if (i === ensureIndexSlot) {
            tryEnsureIndex();
        }
        const start = i * batchSize;
        const end = Math.min(start + batchSize, docs.length);
        const result = collection.insert(docs.slice(start, end));
        if (onBatchInserted) {
            onBatchInserted(result);
        }
    }
    if (ensureIndexSlot === numBatches) {
        collection.ensureIndex(fullIndexDef);
    }
    const ready = waitForVectorIndexState(
        collection, indexName, VectorIndexTrainingState.kReady,
        readyTimeoutSec);
    if (!ready) {
        throw new Error(
            `Vector index '${indexName}' on collection ` +
            `'${collection.name()}' did not reach ready state ` +
            `within ${readyTimeoutSec}s`);
    }
}

/**
 * Waits until a single named vector index on the collection reaches the
 * expected training state. Handles both cluster and single-server modes.
 *
 * @param {ArangoCollection} collection
 * @param {string} indexName - name of the vector index to wait for
 * @param {string} expectedState - ready, training, ingesting, ready
 * @param {number} [timeoutSec=60]
 * @returns {boolean}
 */
function waitForVectorIndexState(collection, indexName, expectedState, timeoutSec = 60) {
    return waitForState(collection, expectedState, timeoutSec, indexName);
}

/**
 * Waits until all vector indexes on the collection reach the expected training
 * state. Handles both cluster and single-server modes.
 *
 * @param {ArangoCollection} collection
 * @param {string} expectedState - ready, training, ingesting, ready
 * @param {number} [timeoutSec=60]
 * @returns {boolean}
 */
function waitForAllVectorIndexesState(collection, expectedState, timeoutSec = 60) {
    return waitForState(collection, expectedState, timeoutSec);
}

/**
 * Asserts that the value returned by collection.ensureIndex() for a synchronous
 * vector-index creation reflects a permanent training failure: trainingState
 * is `unusable` and a non-empty errorMessage is present. Throws on mismatch so
 * the surrounding jsunity test fails.
 *
 * @param {object} result - the value returned by collection.ensureIndex()
 * @param {string} [context] - optional context string included in failure messages
 */
function assertEnsureIndexResultUnusable(result, context) {
    const suffix = context ? ` (${context})` : "";
    if (result.trainingState !== VectorIndexTrainingState.kUnusable) {
        throw new Error(
            `Expected ensureIndex result to report 'unusable' trainingState but got '` +
            result.trainingState + `'` + suffix);
    }
    if (!result.errorMessage || result.errorMessage.length === 0) {
        throw new Error(
            `Unusable ensureIndex result should carry a non-empty errorMessage` + suffix);
    }
}

/**
 * Generates simple documents each containing a random vector field.
 *
 * @param {function} gen - random float generator (e.g. from randomNumberGeneratorFloat)
 * @param {number} count - number of documents to generate
 * @param {number} dimension - vector dimension
 * @returns {Array} array of {vector: [...]} documents
 */
function generateDocs(gen, count, dimension) {
    const docs = [];
    for (let i = 0; i < count; ++i) {
        docs.push({vector: Array.from({length: dimension}, () => gen())});
    }
    return docs;
}

exports.generateDocs = generateDocs;
exports.createVectorGenerator = createVectorGenerator;
exports.DistanceFunctions = DistanceFunctions;
exports.VectorIndexTrainingState = VectorIndexTrainingState;
exports.waitForVectorIndexState = waitForVectorIndexState;
exports.waitForAllVectorIndexesState = waitForAllVectorIndexesState;
exports.insertDocsAndAssertIndex = insertDocsAndAssertIndex;
exports.assertEnsureIndexResultUnusable = assertEnsureIndexResultUnusable;
