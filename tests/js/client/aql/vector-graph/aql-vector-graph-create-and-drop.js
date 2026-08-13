/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse */

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

const dbName = "vectorGraphDB";
const collName = "vectorGraphColl";

// Vector-graph indexes only need the vector dimension (a multiple of 32) and a
// distance metric -- there is no nLists/training as with the IVF index.
const baseParams = { dimension: 32, metric: "l2" };

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorGraphIndexCreateAndDropTestSuite() {
    let collection;

    const ensureGraphIndex = function(params, extra) {
        return collection.ensureIndex(Object.assign({
            name: "vg_idx",
            type: "vector-graph",
            fields: ["vector"],
            params,
        }, extra));
    };

    const assertBadParameter = function(params, extra) {
        try {
            ensureGraphIndex(params, extra);
            fail();
        } catch (e) {
            assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
        }
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
        },

        tearDown: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testCreateIndexReportsVectorGraphType: function() {
            const index = ensureGraphIndex(baseParams);
            assertEqual("vector-graph", index.type);
            assertEqual(["vector"], index.fields);
        },

        testCreatedIndexAppearsInIndexList: function() {
            ensureGraphIndex(baseParams);
            const indexes = collection.getIndexes();
            assertTrue(indexes.some(
                (i) => i.type === "vector-graph" && i.name === "vg_idx"));
        },

        testDropIndexRemovesItFromCollection: function() {
            const index = ensureGraphIndex(baseParams);
            assertTrue(collection.dropIndex(index.id));
            const indexes = collection.getIndexes();
            assertFalse(indexes.some((i) => i.type === "vector-graph"));
        },

        testDimensionMustBeMultipleOf32: function() {
            assertBadParameter({ dimension: 30, metric: "l2" });
        },

        testIndexCannotBeUnique: function() {
            assertBadParameter(baseParams, { unique: true });
        },

        testMaxDegreeAndAlphaDefaultWhenOmitted: function() {
            const index = ensureGraphIndex(baseParams);
            assertEqual(64, index.params.maxDegree);
            assertTrue(Math.abs(index.params.alpha - 1.2) < 0.0001);
        },

        testCustomMaxDegreeAndAlphaRoundTrip: function() {
            const index = ensureGraphIndex(
                Object.assign({}, baseParams, { maxDegree: 32, alpha: 1.5 }));
            assertEqual(32, index.params.maxDegree);
            assertTrue(Math.abs(index.params.alpha - 1.5) < 0.0001);
        },

        testMaxDegreeMustNotExceedOnDiskBound: function() {
            assertBadParameter(Object.assign({}, baseParams, { maxDegree: 65 }));
        },

        testMaxDegreeMustBeGreaterThanZero: function() {
            assertBadParameter(Object.assign({}, baseParams, { maxDegree: 0 }));
        },

        testAlphaMustBeAtLeastOne: function() {
            assertBadParameter(Object.assign({}, baseParams, { alpha: 0.5 }));
        },

        testQuantizationDefaultsToPQ32x8: function() {
            const index = ensureGraphIndex(baseParams);
            assertEqual("PQ32x8", index.params.quantization);
        },

        testCustomQuantizationRoundTrip: function() {
            const index = ensureGraphIndex(
                Object.assign({}, baseParams, { quantization: "PQ16x8" }));
            assertEqual("PQ16x8", index.params.quantization);
        },

        testMalformedQuantizationRejected: function() {
            assertBadParameter(
                Object.assign({}, baseParams, { quantization: "PQ32" }));
        },

        testSubByteQuantizationRoundTrip: function() {
            const index = ensureGraphIndex(
                Object.assign({}, baseParams, { quantization: "PQ32x6" }));
            assertEqual("PQ32x6", index.params.quantization);
        },

        testBitsPerSubquantizerOutOfRangeRejected: function() {
            assertBadParameter(
                Object.assign({}, baseParams, { quantization: "PQ32x0" }));
            assertBadParameter(
                Object.assign({}, baseParams, { quantization: "PQ32x25" }));
        },

        testDimensionMustBeMultipleOfQuantizationM: function() {
            // dimension 32 is not divisible by M=48.
            assertBadParameter(
                Object.assign({}, baseParams, { quantization: "PQ48x8" }));
        },

        // TurboQuant's codebook is fixed for unit-length vectors, so it is only
        // offered under the cosine metric.
        testTurboQuantizationRoundTrip: function() {
            const index = ensureGraphIndex(
                { dimension: 32, metric: "cosine", quantization: "TQ8" });
            assertEqual("TQ8", index.params.quantization);
        },

        testTurboQuantizationSubByteRoundTrip: function() {
            const index = ensureGraphIndex(
                { dimension: 32, metric: "cosine", quantization: "TQ4" });
            assertEqual("TQ4", index.params.quantization);
        },

        testTurboQuantizationHasNoDivisibilityConstraint: function() {
            // TurboQuant is a per-component scalar quantizer, so any dimension
            // is valid (unlike PQ's dimension % M == 0).
            const index = ensureGraphIndex(
                { dimension: 40, metric: "cosine", quantization: "TQ8" });
            assertEqual("TQ8", index.params.quantization);
        },

        testTurboQuantizationRequiresCosineMetric: function() {
            assertBadParameter(
                Object.assign({}, baseParams, { quantization: "TQ8" }));
        },

        testTurboQuantizationUnsupportedBitsRejected: function() {
            const cosine = { dimension: 32, metric: "cosine" };
            assertBadParameter(
                Object.assign({}, cosine, { quantization: "TQ5" }));
            assertBadParameter(
                Object.assign({}, cosine, { quantization: "TQ0" }));
        },
    };
}

if (isEnterprise) {
    jsunity.run(VectorGraphIndexCreateAndDropTestSuite);
}

return jsunity.done();
