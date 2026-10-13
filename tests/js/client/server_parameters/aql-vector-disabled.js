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
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'vector-index': false
  };
}

const internal = require("internal");
const fs = require('fs');
const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const arango = arangodb.arango;
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const db = internal.db;
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");
const { executeExternalAndWaitWithSanitizer, dumpUtils } = require('@arangodb/test-helper');
const pu = require('@arangodb/testutils/process-utils');

const dbName = "vectorDB";
const collName = "vectorColl";
let IM = global.instanceManager;

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
function restoreIntegrationVectorSuite() {
  'use strict';
  const cn = 'UnitTestsVectorIndexRestore';
  const arangorestore = pu.ARANGORESTORE_BIN;

  assertTrue(fs.isFile(arangorestore), "arangorestore not found!");

  let addConnectionArgs = function (args) {
    args.push('--server.endpoint');
    args.push(IM.endpoint);
    if (args.indexOf("--all-databases") === -1 && args.indexOf("--server.database") === -1) {
      args.push('--server.database');
      args.push(arango.getDatabaseName());
    }
    args.push('--server.username');
    args.push(arango.connectedUser());
  };

  let runRestore = function (path, args, rc) {
    args.push('--input-directory');
    args.push(path);
    addConnectionArgs(args);

    const actualRc = executeExternalAndWaitWithSanitizer(arangorestore, args, 'shell-restore-integration');
    assertTrue(actualRc.hasOwnProperty("exit"), actualRc);
    assertEqual(rc, actualRc.exit, actualRc);
  };

  return {

    setUp: function () {
      IM.rememberConnection();
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
      db._databases().forEach((database) => {
        if (database !== "_system") {
          db._dropDatabase(database);
        }
      });
      IM.reconnectMe();
    },

    testRestoreVectorIndex: function () {
      let path = fs.getTempFile();
      fs.makeDirectory(path);
      let fn = fs.join(path, cn + ".structure.json");
      fs.write(fn, JSON.stringify({
        indexes: [],
        parameters: {
          indexes: [
            {id: "0", fields: ["_key"], type: "primary", unique: true},
            {id: "95", fields: ["vector"], type: "vector", params: {dimension: 4, nLists: 4, metric: "l2"}},
            {id: "295", fields: ["value"], type: "persistent", sparse: true},
          ],
          name: cn,
          numberOfShards: 3,
          type: 2
        }
      }));
      let data = [];
      for (let i = 0; i < 1000; ++i) {
        data.push({_key: "test" + i, value: i, vector: [0, i / 10, i / 100, i / 1000]});
      }
      dumpUtils.createCollectionDataFile(data, path, cn, /*split*/ false);
      
      let args = ['--collection', cn, '--import-data', 'true'];
      runRestore(path, args, 0);

      let c = db._collection(cn);
      let indexes = c.indexes();
      // Assert that the vector index was ignored
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual(["_key"], indexes[0].fields);
      assertEqual("persistent", indexes[1].type);
      assertEqual(["value"], indexes[1].fields);

      fs.removeDirectoryRecursive(path, true);
    }
  };
}

jsunity.run(VectorIndexFeatureDisabled);
jsunity.run(restoreIntegrationVectorSuite);

return jsunity.done();
