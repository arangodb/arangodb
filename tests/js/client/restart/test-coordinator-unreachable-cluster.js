let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let { instanceRole } = require('@arangodb/testutils/instance');
let db = arangodb.db;
let IM = global.instanceManager;

const {
  getCtrlCoordinators
} = require('@arangodb/test-helper');

function createCoordinatorReachableSuite() {
  'use strict';
  const databaseName = 'UnitTestsDuplicateCollectionNameDatabase';

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
      db._dropDatabase(databaseName);
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
      }

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
    
  };
}

jsunity.run(createCoordinatorReachableSuite);
return jsunity.done();
