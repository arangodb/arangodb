/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue */

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

const internal = require("internal");
const jsunity = require("jsunity");
const db = internal.db;
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");
const {
    waitForVectorIndexState,
    waitForAllVectorIndexesBuildStateOnDBServers,
} = require("@arangodb/testutils/vector-generator");
const isCluster = require("internal").isCluster();
const dbName = "vectorDB";
const collName = "vectorColl";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexL2NprobeTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const seed = 12132390894;

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            // Use 1 shard so the single shard has enough docs (>= 1000) to trigger training;
            const numberOfShards = 3;
            collection = db._create(collName, {
                numberOfShards
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            const numberOfDocs = isCluster ? 12000 * numberOfShards + 100 : 12000;
            for (let i = 0; i < 12000; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === 0) {
                    randomPoint = vector;
                }
                docs.push({
                    vector
                });
            }
            collection.insert(docs);

            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 300,
                },
            });
            const buildState = "ready";
            const waitTimeoutSec = 60;
            if (isCluster) {
                assertTrue(
                    waitForAllVectorIndexesBuildStateOnDBServers(db, collection, buildState, waitTimeoutSec),
                    "Expected vector index to become trained on DB servers"
                );
            } else {
                assertTrue(waitForVectorIndexState(collection, buildState, waitTimeoutSec),
                    "Expected vector index to become trained");
            }
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2WithOneProbeSameAsDefault: function() {
            const queryWithoutNProbe =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN {key: d._key}";
            const queryWithNProbe =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp, {nProbe: 1}) " +
                "LIMIT 5 RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint,
            };
            const plan = db
                ._createStatement({
                    query: queryWithNProbe,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);
            assertEqual(1, indexNodes[0].searchParameters.nProbe);

            const resultsWithoutNProbe = db._query(queryWithoutNProbe, bindVars).toArray();
            const resultsWithNProbe = db._query(queryWithoutNProbe, bindVars).toArray();
            assertEqual(resultsWithoutNProbe, resultsWithNProbe);
        },

        testApproxL2WithNProbe: function() {
            const queryWithoutNProbe =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp, {nProbe: 1}) LIMIT 300 RETURN {key: d._key}";
            const queryWithNProbe =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp, {nProbe: 100}) " +
                " LIMIT 300 RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            const plan = db
                ._createStatement({
                    query: queryWithNProbe,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);
            assertEqual(100, indexNodes[0].searchParameters.nProbe);

            const resultsWithoutNProbe = db._query(queryWithoutNProbe, bindVars).toArray();
            const resultsWithNProbe = db._query(queryWithNProbe, bindVars).toArray();
            assertNotEqual(resultsWithoutNProbe, resultsWithNProbe);
        },
    };
}

jsunity.run(VectorIndexL2NprobeTestSuite);

return jsunity.done();
