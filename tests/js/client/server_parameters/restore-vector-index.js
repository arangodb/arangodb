/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, arango, getOptions */

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
// / @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'experimental-vector-index' : 'true'
  };
}

const jsunity = require('jsunity');
const {assertTrue, assertEqual} = jsunity.jsUnity.assertions;
const arangodb = require('@arangodb');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const db = arangodb.db;
const { executeExternalAndWaitWithSanitizer, dumpUtils } = require('@arangodb/test-helper');


function restoreIntegrationVectorSuite() {
  'use strict';
  const cn = 'UnitTestsVectorIndexRestore';
  const arangorestore = pu.ARANGORESTORE_BIN;

  assertTrue(fs.isFile(arangorestore), "arangorestore not found!");

  let addConnectionArgs = function (args) {
    const endpoint = arango.getEndpoint();
    args.push('--server.endpoint');
    args.push(endpoint);
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
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
      db._databases().forEach((database) => {
        if (database !== "_system") {
          db._dropDatabase(database);
        }
      });
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
      assertEqual(3, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual(["_key"], indexes[0].fields);
      assertEqual("vector", indexes[1].type);
      assertEqual(["vector"], indexes[1].fields);
      assertEqual("persistent", indexes[2].type);
      assertEqual(["value"], indexes[2].fields);

      // test if the vector index works
      for (let i = 0; i < 100; ++i) {
        c.insert({value: i, vector: [0, 0, 0, 0]});
      }
      let result = db._query("FOR doc IN " + cn + " LET dist = APPROX_NEAR_L2(doc.vector, [0, 0, 0, 0]) SORT dist LIMIT 10 RETURN doc").toArray();
      assertEqual(10, result.length);
      fs.removeDirectoryRecursive(path, true);
    }
  };
}


jsunity.run(restoreIntegrationVectorSuite);

return jsunity.done();
