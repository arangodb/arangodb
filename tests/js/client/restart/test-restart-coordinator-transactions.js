/*jshint globalstrict:false, strict:false */
/* global assertTrue, assertEqual, assertMatch, arango, fail */

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
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const _ = require('lodash');
const crypto = require('@arangodb/crypto');
const time = require("internal").time;
const db = require("internal").db;
const errors = require("internal").errors;
let { instanceRole } = require('@arangodb/testutils/instance');

const IM = global.instanceManager;
const cn = "UnitTestsCollection";

function testSuite() {

  return {
    setUp : function() {
      db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testRestartCoordinatorsDuringTransaction : function() {
      let trx = db._createTransaction({ 
        collections: { write: cn }
      });

      let tc = trx.collection(cn);
      tc.insert({ _key: "test1" });

      let coordinators = IM.getInstancesRole(instanceRole.coordinator);
      assertTrue(coordinators.length > 1);

      for (let i = 0; i < coordinators.length; ++i) {
        let coordinator = coordinators[i];
        coordinator.shutdownArangod(false);
        coordinator.waitForInstanceShutdown(30);
        coordinator.exitStatus = null;
        coordinator.pid = null;
        console.warn("Restarting coordinator...", coordinator.getStructure());

        coordinator.restartOneInstance({
          "server.authentication": "false"
        });
        
        coordinator.pingUntilReady(IM.httpJWTAuthOptions, 30);
      }

      // connection to server was closed, so next request may fail.
      try {
        // issue a dummy request to the server. if that fails because of
        // a broken connection, we don't care.
        tc.count();
        // all following requests should be fine again
      } catch (err) {}
      
      // will fail, because the coordinator owning the
      // transaction got restarted
      try {
        tc.insert({ _key: "test2" });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_TRANSACTION_NOT_FOUND.code, err.errorNum);
      }
   
      // contact all coordinators - all the requests must fail everywhere
      for (let i = 0; i < coordinators.length; ++i) {
        coordinators[i].toThisInstance(() => {
          let result = arango.PUT_RAW("/_api/transaction/" + encodeURIComponent(trx._id), {});
          assertEqual(errors.ERROR_TRANSACTION_NOT_FOUND.code, result.parsedBody.errorNum);
          assertMatch(/cannot find target server/, result.parsedBody.errorMessage);
        });
      }
      
      // we should be able to start a new transaction, however
      trx = db._createTransaction({ 
        collections: { write: cn }
      });
      
      try {
        tc = trx.collection(cn);
        tc.insert({ _key: "test1" });

        assertEqual(1, tc.count()); 
      
        // contact all coordinators - all these requests must succeed
        for (let i = 0; i < coordinators.length; ++i) {
          coordinators[i].toThisInstance(() => {
            let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), { _key: "coord" + i }, { "x-arango-trx-id" : trx._id });
            assertEqual(202, result.code);
          });
        }
        
        assertEqual(1 + coordinators.length, tc.count()); 
      } finally {
        trx.abort();
      }
    },
    
  };
}
jsunity.run(testSuite);
return jsunity.done();
