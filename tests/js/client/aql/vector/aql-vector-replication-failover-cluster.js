/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print */

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
// / @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const aql = arangodb.aql;
const db = internal.db;
const {
    getDBServers,
    waitForShardsInSync
} = require("@arangodb/test-helper");
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");

const dbName = "vectorReplicationDb";
const collName = "vectorReplicationColl";


// COR-255 vector replication not working
function VectorIndexReplicationFailoverTest() {
    const dimension = 4;
    const numberOfDocs = 10;
    const seed = 42;
    const nLists = 2;

    return {
        setUp: function () {
            db._createDatabase(dbName);
            db._useDatabase(dbName);
        },

        tearDown: function () {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testVectorIndexWorksAfterLeaderFailover: function () {
            let collection = db._create(collName, {
                numberOfShards: 1,
                replicationFactor: 1
            });

            let gen = randomNumberGeneratorFloat(seed);
            let docs = [];
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({length: dimension}, () => gen());
                docs.push({vec: vector, value: i});
            }
            collection.insert(docs);
            assertEqual(collection.count(), numberOfDocs);

            collection.ensureIndex({
                name: "vec_idx",
                type: "vector",
                fields: ["vec"],
                params: {
                    dimension: dimension,
                    metric: "l2",
                    nLists: nLists,
                }
            });

            // Verify vector search works on the leader
            const queryVector = Array.from({length: dimension}, () => gen());
            const vectorQuery = aql`
                FOR doc IN ${collection}
                    SORT APPROX_NEAR_L2(doc.vec, ${queryVector})
                    LIMIT 5
                    RETURN doc.value
            `;
            const resultsBefore = db._query(vectorQuery).toArray();
            assertEqual(resultsBefore.length, 5);

            // Increase replicationFactor to 2
            collection.properties({replicationFactor: 2});

            // Wait for the new follower to fully sync
            
            // Get the leader after sync
            waitForShardsInSync(collName, 120, 1);
            let shards = collection.shards(true);
            let [shardName, servers] = Object.entries(shards)[0];
            assertTrue(servers.length >= 2,
                       "Expected at least 2 servers for shard after replication factor change");
            let leaderServerId = servers[0];

            // Shutdown the leader
            let dbServers = getDBServers();
            let leaderServer = dbServers.find(s => s.id === leaderServerId);
            assertTrue(leaderServer !== undefined,
                       "Could not find leader server " + leaderServerId);
            leaderServer.suspend();

            // Wait for failover and shards to stabilize
            waitForShardsInSync(collName, 120, 1);

            // Query the vector index on the new leader (former follower)
            let resultsAfter = db._query(vectorQuery).toArray();

            // Verify results
            assertEqual(resultsAfter.length, 5,
                        "Expected 5 results from vector search after failover");
            // The results are no longer the same as before failover
            assertEqual(resultsBefore.length, resultsAfter.length);

            leaderServer.resume();
        },
    };
}

jsunity.run(VectorIndexReplicationFailoverTest);

return jsunity.done();
