/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, arango */

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
// / @author Koushal Kawade
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let { instanceRole } = require('@arangodb/testutils/instance');
let db = arangodb.db;
let IM = global.instanceManager;

const {
  getCtrlCoordinators
} = require('@arangodb/test-helper');

function createCoordinatorUnreachableSuite() {
  'use strict';
  const databaseName = 'UnitTestsDatabaseName';
  const collectionName = 'UnitTestsCollectionName';

  let suspend = function (servers) {
    servers.forEach(function(server) {
      server.suspend();
    });
  };

  let resume = function (servers) {
    servers.forEach(function(server) {
      server.resume();
    });
  };

  return {
    tearDown: function() {
      let dbExists = db._databases().includes(databaseName);
      if (dbExists) {
        db._dropDatabase(databaseName);
      }

      let c = db._collection(collectionName);
      if (c !== null) {
        c.drop();
      }
    },

    testCoordinatorUnreachableDuringCreateDatabase: function () {

      let waitForAsyncJob = function (asyncJobId, timeoutInSeconds) {
        let count = 0;
        let code = -1;
        while (true) {
          let s = arango.PUT(`/_api/job/${asyncJobId}`,{});
          code = s.code;

          if (s.code !== 204 || count > timeoutInSeconds)
            break;

          internal.wait(1);
        }
        return [code, count];
      };

      // 1. First test with an unreachable coordinator
      IM.debugSetFailAt("CreateDatabase::delay", instanceRole.dbServer);
      let createDb = arango.POST_RAW("/_api/database", {name:databaseName}, {"x-arango-async":"store"});

      let coordinators = getCtrlCoordinators();

      // Suspend coordinators for 15 sec which will make
      // the agency remove the database from the plan.
      suspend(coordinators);
      internal.wait(15);
      resume(coordinators);

      const timeoutSeconds = 20;
      let result = waitForAsyncJob(createDb.headers["x-arango-async-id"], timeoutSeconds);
      let exitCode = result[0];
      let count = result[1];

      assertTrue(count < timeoutSeconds, "Coordinator stuck endlessly waiting for CreateDatabase to finish");
      assertEqual(400, exitCode, "Expected exit code 400, received: " + exitCode);

      //  2. Now test with a reachable coordinator
      IM.debugRemoveFailAt("CreateDatabase::delay");
      createDb = arango.POST_RAW("/_api/database",{name:databaseName},{"x-arango-async":"store"});
      result = waitForAsyncJob(createDb.headers["x-arango-async-id"], timeoutSeconds);
      exitCode = result[0];
      count = result[1];

      assertTrue(count < timeoutSeconds, "Coordinator stuck endlessly waiting for CreateDatabase to finish");
      assertEqual(201, exitCode, "Expected exit code 201, received: " + exitCode);
    },

    testCoordinatorUnreachableDuringCreateCollection: function () {

      let waitForAsyncJob = function (asyncJobId, timeoutInSeconds) {
        let count = 0;
        let code = -1;
        while (true) {
          let s = arango.PUT(`/_api/job/${asyncJobId}`,{});
          code = s.code;

          if (s.code !== 204 || count > timeoutInSeconds)
            break;

          internal.wait(1);
        }
        return [code, count];
      };

      // 1. First test with an unreachable coordinator
      IM.debugSetFailAt("DelayCreateShard15", instanceRole.dbServer);
      let createColl = arango.POST_RAW("/_api/collection", {name:collectionName}, {"x-arango-async":"store"});

      let coordinators = getCtrlCoordinators();

      // Suspend coordinators for 15 sec which will make
      // the agency remove the collection from the plan.
      suspend(coordinators);
      internal.wait(15);
      resume(coordinators);

      const timeoutSeconds = 20;
      let result = waitForAsyncJob(createColl.headers["x-arango-async-id"], timeoutSeconds);
      let exitCode = result[0];
      let count = result[1];

      assertTrue(count < timeoutSeconds, "Coordinator stuck endlessly waiting for CreateCollection to finish");
      assertEqual(500, exitCode, "Expected exit code 500, received: " + exitCode);

      // //  2. Now test with a reachable coordinator
      IM.debugRemoveFailAt("DelayCreateShard15");
      createColl = arango.POST_RAW("/_api/collection", {name:collectionName}, {"x-arango-async":"store"});
      result = waitForAsyncJob(createColl.headers["x-arango-async-id"], timeoutSeconds);
      exitCode = result[0];
      count = result[1];

      assertTrue(count < timeoutSeconds, "Coordinator stuck endlessly waiting for CreateCollection to finish");
      assertEqual(200, exitCode, "Expected exit code 200, received: " + exitCode);
    },

  };
}

jsunity.run(createCoordinatorUnreachableSuite);
return jsunity.done();
