/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, arango */

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');
const crypto = require('@arangodb/crypto');
const db = require("@arangodb").db;
const request = require("@arangodb/request");
const time = require("internal").time;
const { getCtrlCoordinators } = require('@arangodb/test-helper');

const graphs = require('@arangodb/general-graph');

const gn = "UnitTestsGraph";
const vn = "UnitTestsVertex";
const en = "UnitTestsEdge";

function testSuite() {
  const jwtSecret = 'haxxmann';

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

  let checkAvailability = function (servers, expectedCode) {
    require("console").warn("checking (un)availability of " + servers.map((s) => s.url).join(", "));
    servers.forEach(function(server) {
      let res = request({ method: "get", url: server.url + "/_api/version", timeout: 3 });
      assertEqual(expectedCode, res.status);
    });
  };

  return {
    tearDownAll : function() {
      try {
        graphs._drop(gn, true);
      } catch (err) {}
      // Need to restart without authentication for other tests to succeed:
      let coordinators = getCtrlCoordinators();
      let coordinator = coordinators[0];
      coordinator.shutdownArangod(false);
      coordinator.waitForInstanceShutdown(30);
      coordinator.pid = null;
      console.warn("Cleaning up and restarting coordinator without authentication...", coordinator);
      coordinator.restartOneInstance({
        "server.authentication": "false"
      });
      let aliveStatus = waitForAlive(30, coordinator.url, {});
      assertEqual(200, aliveStatus);
    },

    testRestartCoordinatorWithGraph : function() {
      graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 2, replicationFactor: 2 });
      
      let c = db._collection(vn);
      
      // insert initial documents
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ value: String(i), _key: "test" + i });
      }
      c.insert(docs);
      assertEqual(10, c.count());
      
      c = db._collection(en);
      // insert initial edges
      docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _from: vn + "/test" + i, _to: vn + "/test" + i, value: String(i), _key: "test" + i });
      }
      c.insert(docs);
      assertEqual(10, c.count());

      let coordinators = getCtrlCoordinators();
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      coordinator.shutdownArangod(false);
      coordinator.waitForInstanceShutdown(30);
      coordinator.exitStatus = null;
      coordinator.pid = null;

      coordinator.restartOneInstance({
        "server.jwt-secret": jwtSecret
      });
        
      waitForAlive(30, coordinator.url, {});
      
      // vertex collection
      c = db._collection(vn);

      // do NOT remove this! this is a workaround for Windows
      // sometimes having trouble to use the already opened connection
      // after a server restart
      let properties;
      try {
        properties = c.properties();
      } catch (err) {
        properties = c.properties();
      }

      assertFalse(properties.isSmart);
      assertEqual(2, properties.numberOfShards);
      assertEqual(2, properties.replicationFactor);

      // smoke test: insert more documents
      docs = [];
      for (let i = 10; i < 20; ++i) {
        docs.push({ value: i, _key: "test" + i });
      }
      c.insert(docs);
      assertEqual(20, c.count());
     
      // look up documents
      for (let i = 0; i < 20; ++i) {
        let doc = c.document("test" + i);
        assertEqual(i, doc.value);
      }
     
      // edge collection
      c = db._collection(en);
      properties = c.properties();
      
      assertFalse(properties.isSmart);
      assertEqual(2, properties.numberOfShards);
      assertEqual(2, properties.replicationFactor);
      
      // smoke test: insert more edges
      docs = [];
      for (let i = 10; i < 20; ++i) {
        docs.push({ _from: vn + "/test" + i, _to: vn + "/test" + i, value: i, _key: "test" + i });
      }
      c.insert(docs);
      assertEqual(20, c.count());
     
      // look up edges
      for (let i = 0; i < 20; ++i) {
        let doc = c.document("test" + i);
        assertEqual(i, doc.value);
        assertEqual(vn + "/test" + i, doc._from);
        assertEqual(vn + "/test" + i, doc._to);
      }
    },
   
  };
}
jsunity.run(testSuite);
return jsunity.done();
