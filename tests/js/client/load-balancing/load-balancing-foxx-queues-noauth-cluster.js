/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, require*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const db = require("internal").db;
const request = require("@arangodb/request");
const _ = require("lodash");
const getCoordinatorEndpoints = require('@arangodb/test-helper').getCoordinatorEndpoints;

const servers = getCoordinatorEndpoints();

function FoxxQueuesSuite () {
  'use strict';
  let coordinators = [];

  const databaseName = "UnitTestsDatabase";
  const qn = "FoxxQueue";
  const adminPath = "_admin/execute?returnBodyAsJSON=true";
 
  function sendRequest(method, endpoint, body, usePrimary) {
    let res;
    const i = usePrimary ? 0 : 1;

    try {
      const envelope = {
        body,
        method,
        url: `${coordinators[i]}${endpoint}`
      };
      res = request(envelope);
    } catch(err) {
      console.error(`Exception processing ${method} ${endpoint}`, err.stack);
      return {};
    }
    let resultBody = res.body;
    if (typeof resultBody === "string") {
      resultBody = JSON.parse(resultBody);
    }
    return resultBody;
  }

  return {
    
    setUpAll: function() {
      coordinators = getCoordinatorEndpoints();
      if (coordinators.length < 2) {
        throw new Error('Expecting at least two coordinators');
      }

      db._createDatabase(databaseName + "1");
      db._createDatabase(databaseName + "2");
      
      // create queues
      let req, result;
      req = `require("@arangodb/foxx/queues").create("${qn}"); return "ok";`;
      result = sendRequest('POST', `/_db/${databaseName}1/${adminPath}`, req, true);
      assertEqual("ok", result);
      result = sendRequest('POST', `/_db/${databaseName}2/${adminPath}`, req, true);
      assertEqual("ok", result);

      // this is required. we must give the coordinators a bit of time so that
      // they start processing Foxx queue jobs for the first time before we move on
      require("internal").sleep(10);
    },

    tearDownAll: function() {
      db._useDatabase("_system");
      try {
        db._dropDatabase(databaseName + "1");
      } catch (err) {}
      try {
        db._dropDatabase(databaseName + "2");
      } catch (err) {}
    },
    
    testPushIntoDBQueuesOnNonFoxxmaster: function() {
      // determine Foxxmaster
      let primaryIsFoxxmaster = sendRequest('GET', `/_admin/status`, "", true).coordinator.isFoxxmaster;
      if (!primaryIsFoxxmaster) {
        assertTrue(sendRequest('GET', `/_admin/status`, "", false).coordinator.isFoxxmaster, "unable to determine Foxxmaster");
      }

      let req, result;

      // create queue jobs
      for (let i = 0; i < 5; ++i) {
        req = `require("@arangodb/foxx/queues").get("${qn}").push({ name: 'nonmastertest${i}', mount: '/nonmastertest${i}' }, {}, { delay: 1 }); return "ok";`;
        result = sendRequest('POST', `/_db/${databaseName}1/${adminPath}`, req, !primaryIsFoxxmaster);
        assertEqual("ok", result);
        result = sendRequest('POST', `/_db/${databaseName}2/${adminPath}`, req, !primaryIsFoxxmaster);
        assertEqual("ok", result);
      }

      const maxTries = 90;
      let tries = 0;
      while (tries++ < maxTries) {
        let pending = 0;
        try {
          [databaseName + "1", databaseName + "2"].forEach((name) => {
            db._useDatabase(name);
            let res = db._query("FOR doc IN _jobs FILTER doc.type.name >= 'nonmastertest0' && doc.type.name <= 'nonmastertest4' RETURN doc").toArray();
            if (res.length && res[0].status === 'pending') {
              ++pending;
            }
          });
        } finally {
          db._useDatabase("_system");
        }
        
        if (pending === 0) {
          break;
        }

        require("internal").sleep(0.5);
      }
      
      assertTrue(tries < maxTries, "timeout waiting for Foxx queue processing");
    },
    
    testPushIntoDBQueuesOnTwoCoordinators: function() {
      let req, result;

      // create queue jobs
      let usePrimary = true;
      for (let i = 0; i < 25; ++i) {
        req = `require("@arangodb/foxx/queues").get("${qn}").push({ name: 'multitest${i}', mount: '/multitest${i}' }, {}, { delay: 1 }); return "ok";`;
        result = sendRequest('POST', `/_db/${databaseName}1/${adminPath}`, req, usePrimary);
        assertEqual("ok", result);
        result = sendRequest('POST', `/_db/${databaseName}2/${adminPath}`, req, !usePrimary);
        assertEqual("ok", result);

        // flip coordinator
        usePrimary = !usePrimary;
        require("internal").sleep(0.5);
      }

      const maxTries = 90;
      let tries = 0;
      while (tries++ < maxTries) {
        let pending = 0;
        try {
          [databaseName + "1", databaseName + "2"].forEach((name) => {
            db._useDatabase(name);
            let res = db._query("FOR doc IN _jobs FILTER doc.type.name >= 'multitest0' && doc.type.name <= 'multitest9999' RETURN doc").toArray();
            if (res.length && res[0].status === 'pending') {
              ++pending;
            }
          });
        } finally {
          db._useDatabase("_system");
        }
        
        if (pending === 0) {
          break;
        }

        require("internal").sleep(0.5);
      }
      
      assertTrue(tries < maxTries, "timeout waiting for Foxx queue processing");
    },
    
  };
}

jsunity.run(FoxxQueuesSuite);

return jsunity.done();
