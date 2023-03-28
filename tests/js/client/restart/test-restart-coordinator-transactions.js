/*jshint globalstrict:false, strict:false */
/* global assertTrue, assertEqual, assertMatch, arango, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');
const crypto = require('@arangodb/crypto');
const request = require("@arangodb/request");
const time = require("internal").time;
const db = require("internal").db;
const errors = require("internal").errors;

const {
  getCtrlCoordinators,
} = require('@arangodb/test-helper');

const cn = "UnitTestsCollection";

function testSuite() {
  let waitForAlive = function (timeout, baseurl, data) {
    let tries = 0, res;
    let all = Object.assign(data || {}, { method: "get", timeout: 1, url: baseurl + "/_api/version" }); 
    const end = time() + timeout;
    while (time() < end) {
      res = request(all);
      if (res.status === 200 || res.status === 401 || res.status === 403) {
        break;
      }
      console.warn("waiting for server response from url " + baseurl);
      require('internal').sleep(0.5);
    }
    return res.status;
  };

  let sendRequest = function (method, endpoint, body, headers) {
    const envelope = {
      json: true,
      method,
      url: endpoint,
      headers,
    };
    if (method !== 'GET') {
      envelope.body = body;
    }
    let res = request(envelope);

    if (typeof res.body === "string") {
      if (res.body === "") {
        res.body = {};
      } else {
        res.body = JSON.parse(res.body);
      }
    }
    return res;
  };

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

      let coordinators = getCtrlCoordinators();
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
        
        waitForAlive(30, coordinator.url, {});
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
        let result = sendRequest('PUT', coordinators[i].url + "/_api/transaction/" + encodeURIComponent(trx._id), {}, {});
        assertEqual(errors.ERROR_TRANSACTION_NOT_FOUND.code, result.body.errorNum);
        assertMatch(/cannot find target server/, result.body.errorMessage);
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
          let result = sendRequest('POST', coordinators[i].url + "/_api/document/" + encodeURIComponent(cn), { _key: "coord" + i }, { "x-arango-trx-id" : trx._id });
          assertEqual(202, result.status);
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
