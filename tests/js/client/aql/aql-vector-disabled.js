/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, getOptions, assertEqual, assertTrue */

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
//
// @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const db = internal.db;
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");

const dbName = "vectorDB";
const collName = "vectorColl";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexFeatureDisabled() {
    let collection;
    const dimension = 500;
    const seed = 12132390894;

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName);

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < 500; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                docs.push({
                    vector
                });
            }
            collection.insert(docs);
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testCreatingVectorIndex: function() {
            try {
                let result = collection.ensureIndex({
                    name: "vector_l2",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2",
                        dimension: dimension,
                        nLists: 10,
                        trainingIterations: 10,
                    },
                });
              fail(); // feature with dependency
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code,
                    e.errorNum);
            }
        },
    };
}

jsunity.run(VectorIndexFeatureDisabled);

return jsunity.done();
