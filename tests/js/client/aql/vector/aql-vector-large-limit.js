/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

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

const dbName = "vectorLargeLimitDB";
const collName = "vectorColl";
const numberOfDocs = 4500;
const dimension = 128;
const nLists = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for fetching more than 1000 documents with max nProbe
////////////////////////////////////////////////////////////////////////////////

function VectorIndexLargeLimitTestSuite() {
    let collection;
    let randomPoint;
    const seed = 98765432;

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            let gen = randomNumberGeneratorFloat(seed);
            const batchSize = 1000;
            for (let batch = 0; batch < numberOfDocs; batch += batchSize) {
                let docs = [];
                const end = Math.min(batch + batchSize, numberOfDocs);
                for (let i = batch; i < end; ++i) {
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
            }

            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: nLists,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testFetchLargeNumberOfDocsWithMaxNProbe: function() {
            const limits = [1500, 3000, 4000];
            const bindVars = {
                qp: randomPoint,
            };

            for (const limit of limits) {
                const query =
                    "FOR d IN " +
                    collection.name() +
                    " SORT APPROX_NEAR_L2(d.vector, @qp, " +
                    "{nProbe: " + nLists + "}) " +
                    "LIMIT " + limit + " RETURN d._key";

                const results = db._query(query, bindVars).toArray();
                assertEqual(limit, results.length,
                    "Expected " + limit + " results");

                const uniqueResults = new Set(results);
                assertEqual(limit, uniqueResults.size,
                    "All " + limit + " returned documents should be unique");
            }
        },
    };
}

jsunity.run(VectorIndexLargeLimitTestSuite);

return jsunity.done();
