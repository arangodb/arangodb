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
const time = require("internal").time;

const graphs = require('@arangodb/general-graph');

let { instanceRole } = require('@arangodb/testutils/instance');

const gn = "UnitTestsGraph";
const vn = "UnitTestsVertex";
const en = "UnitTestsEdge";
const IM = global.instanceManager;

function testSuite() {
  const jwtSecret = 'haxxmann';

  return {
    tearDownAll : function() {
      try {
        graphs._drop(gn, true);
      } catch (err) {}
      // Need to restart without authentication for other tests to succeed:
      let coordinators = IM.getInstancesRole(instanceRole.coordinator);
      let coordinator = coordinators[0];
      coordinator.shutdownArangod(false);
      coordinator.waitForInstanceShutdown(30);
      coordinator.pid = null;
      console.warn("Cleaning up and restarting coordinator without authentication...", coordinator);
      coordinator.restartOneInstance({
        "server.authentication": "false"
      });

      coordinator.pingUntilReady(IM.httpJWTAuthOptions, 30);
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

      let coordinators = IM.getInstancesRole(instanceRole.coordinator);
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      coordinator.shutdownArangod(false);
      coordinator.waitForInstanceShutdown(30);
      coordinator.exitStatus = null;
      coordinator.pid = null;

      coordinator.restartOneInstance({
        "server.jwt-secret": jwtSecret
      });
        
      coordinator.pingUntilReady(IM.httpJWTAuthOptions, 30);
      
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
