/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
const dbName = "vectorDb";
const collName = "vectorColl";
const indexName = "vectorIndex";
let {
    agency,
    getMetric,
    getEndpointById
} = require('@arangodb/test-helper');

let IM = global.instanceManager;
const { versionHas } = require("@arangodb/test-helper");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexCorrectDefinitionInAgencyTest() {
    let collection;
    const dimension = 500;
    const seed = 12132390894;
    const nLists = 1;
    const metric = "l2";

    return {
        setUp: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < 100; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                docs.push({
                    vector
                });
            }
            collection.insert(docs);
        },

        tearDown: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testVectorIndexCreationCorrect: function() {
            let beforeVersion = IM.agencyMgr.get("Current/Version").arango.Current.Version;
            collection.ensureIndex({
                "name": "test",
                "params": {
                    "dimension": dimension,
                    "metric": metric,
                    "nLists": nLists,
                },
                "fields": ["vector"],
                "type": "vector"
            });

            // wait for servers to pick up the changes
            internal.sleep(5);

            let afterVersion = IM.agencyMgr.get("Current/Version").arango.Current.Version;

          // If the index is created we do not expect too much version change
          // Unfortunately this is the only indicator that we have not entered
          assertTrue(afterVersion - beforeVersion < 10);
        },

        testVectorIndexCreationCorrectWithFactory: function() {
            let beforeVersion = IM.agencyMgr.get("Current/Version").arango.Current.Version;
            collection.ensureIndex({
                "name": "test",
                "params": {
                    "dimension": dimension,
                    "metric": metric,
                    "nLists": nLists,
                    "factory": `IVF${nLists},Flat`
                },
                "fields": ["vector"],
                "type": "vector"
            });

            // wait for servers to pick up the changes
            internal.sleep(5);

            let afterVersion = IM.agencyMgr.get("Current/Version").arango.Current.Version;

          // If the index is created we do not expect too much version change
          // Unfortunately this is the only indicator that we have not entered
          assertTrue(afterVersion - beforeVersion < 10);
        },
    };
}

// This test confirms that even though there might be an incomplete definition of
// vector index it will still not cause the loop of incorrect comparison between
// indexes.
// The relevant code for this is in IndexFactory::equal
//
// The failure that we are testing against is entering loop:
// 1. Agency sends incorrect version of vector index
// 2. The index gets created on dbServer
// 3. Comparions of index definition on dbServer and agency fails
// 4. Index gets dropped
// 5. Back to step 1.
// The only way  to detect something went wrong here if the version
// changed drastically in short period
function VectorIndexInvalidDefinitionInAgencyTest() {
    let collection;
    const dimension = 500;
    const seed = 12132390894;
    const nLists = 1;
    const metric = "l2";

    return {
        setUp: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < 100; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                docs.push({
                    vector
                });
            }
            collection.insert(docs);
        },

        tearDown: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testVectorIndexCreationWithMissingDefaultNProbe: function() {
            const indexRequestWithoutNProbes = [{
                "id": "1214",
                "type": "primary",
                "name": "primary",
                "fields": [
                    "_key"
                ],
                "sparse": false,
                "unique": true
            }, {
                "estimates": false,
                "fields": [
                    "vector"
                ],
                "id": "8020010",
                "inBackground": false,
                "name": "test",
                "params": {
                    "dimension": dimension,
                    "metric": metric,
                    "nLists": nLists,
                    "trainingIterations": 25
                },
                "sparse": false,
                "type": "vector"
            }];
            let beforeVersion = IM.agencyMgr.get("Current/Version").arango.Current.Version;
            IM.agencyMgr.set(`Plan/Collections/${dbName}/${collection._id}/indexes`, indexRequestWithoutNProbes);
            IM.agencyMgr.increaseVersion("Plan/Version");

            // wait for servers to pick up the changes
            internal.sleep(5);

            let afterVersion = IM.agencyMgr.get("Current/Version").arango.Current.Version;

            assertTrue(afterVersion - beforeVersion < 10);
        },

        testVectorIndexCreationWithMissingDefaultNProbeWithFactory: function() {
            const indexRequestWithoutNProbes = [{
                "id": "1214",
                "type": "primary",
                "name": "primary",
                "fields": [
                    "_key"
                ],
                "sparse": false,
                "unique": true
            }, {
                "estimates": false,
                "fields": [
                    "vector"
                ],
                "id": "8020010",
                "inBackground": false,
                "name": "test",
                "params": {
                    "dimension": dimension,
                    "metric": metric,
                    "nLists": nLists,
                    "factory": `IVF${nLists},Flat`,
                    "trainingIterations": 25
                },
                "sparse": false,
                "type": "vector"
            }];
            let beforeVersion = IM.agencyMgr.get("Current/Version").arango.Current.Version;
            IM.agencyMgr.set(`Plan/Collections/${dbName}/${collection._id}/indexes`, indexRequestWithoutNProbes);
            IM.agencyMgr.increaseVersion("Plan/Version");

            // wait for servers to pick up the changes
            internal.sleep(5);

            let afterVersion = IM.agencyMgr.get("Current/Version").arango.Current.Version;

            assertTrue(afterVersion - beforeVersion < 10);
        },
    };
}

jsunity.run(VectorIndexCorrectDefinitionInAgencyTest);
jsunity.run(VectorIndexInvalidDefinitionInAgencyTest);

return jsunity.done();
