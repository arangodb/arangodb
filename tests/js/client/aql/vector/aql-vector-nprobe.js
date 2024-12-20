/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual */

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
const arangodb = require("@arangodb");
const helper = require("@arangodb/aql-helper");
const aql = arangodb.aql;
const getQueryResults = helper.getQueryResults;
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const db = internal.db;
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");

const isCluster = require("internal").isCluster();
const dbName = "vectorDB";
const collName = "vectorColl";
const indexName = "vectorIndex";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexL2NprobeTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const seed = 12132390894;
    const defaultNProbe = 1;

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < 1000; ++i) {
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

            // big number of nLists causes that results are spread across
            // multiple nLists and makes probability of returning all the results with 1 nProbe lower
            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 300,
                    trainingIterations: 10,
                },
            });
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
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN {key: d._key}";
            const queryWithNProbe =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp, {nProbe: 100}) " +
                " LIMIT 5 RETURN {key: d._key}";

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
