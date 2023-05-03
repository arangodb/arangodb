/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");

const base64Encode = require('internal').base64Encode;
const db = require("internal").db;
const request = require("@arangodb/request");
const url = require('url');
const userModule = require("@arangodb/users");
const _ = require("lodash");
const getCoordinatorEndpoints = require('@arangodb/test-helper').getCoordinatorEndpoints;
let pregelTestHelpers = require("@arangodb/graph/pregel-test-helpers");

const servers = getCoordinatorEndpoints();

function PregelAuthSuite () {
  'use strict';
  let cs = [];
  let coordinators = [];
  const users = [
    { username: 'alice', password: 'pass1' },
    { username: 'bob', password: 'pass2' },
  ];
  const baseUrl = `/_api/control_pregel`;
  const pregelSystemCollection = '_pregel_queries';

  function sendRequest(auth, method, endpoint, body, usePrimary) {
    let res;
    const i = usePrimary ? 0 : 1;

    try {
      const envelope = {
        headers: {
          authorization:
            `Basic ${base64Encode(auth.username + ':' + auth.password)}`
        },
        json: true,
        method,
        url: `${coordinators[i]}${endpoint}`
      };
      if (method !== 'GET') {
        envelope.body = body;
      }
      res = request(envelope);
    } catch(err) {
      console.error(`Exception processing ${method} ${endpoint}`, err.stack);
      return {};
    }

    if (typeof res.body === "string") {
      if (res.body === "") {
        res.body = {};
      } else {
        res.body = JSON.parse(res.body);
      }
    }
    return res;
  }

  return {
    setUp: function() {
      coordinators = getCoordinatorEndpoints();
      if (coordinators.length < 2) {
        throw new Error('Expecting at least two coordinators');
      }

      cs = [];
      cs.push(db._create("vertices", {numberOfShards: 8}));
      cs.push(db._createEdgeCollection("edges", {shardKeys: ["vertex"], distributeShardsLike: "vertices", numberOfShards: 8}));
      for (let i = 1; i <= 250; ++i) {
        cs[0].save({ _key: `v_${i}` });
        const edges = [];
        for (let j = 1; j < i; ++j) {
          edges.push({ _from: `vertices/v_${j}`, _to: `vertices/v_${i}` });
        }
        cs[1].save(edges);
      }

      try {
        userModule.remove(users[0].username);
        userModule.remove(users[1].username);
      } catch (err) {}
      userModule.save(users[0].username, users[0].password);
      userModule.save(users[1].username, users[1].password);

      userModule.grantDatabase(users[0].username, '_system', 'rw');
      userModule.grantDatabase(users[1].username, '_system', 'rw');
      userModule.grantCollection(users[0].username, '_system', 'vertices', 'rw');
      userModule.grantCollection(users[1].username, '_system', 'vertices', 'rw');
      userModule.grantCollection(users[0].username, '_system', 'edges', 'rw');
      userModule.grantCollection(users[1].username, '_system', 'edges', 'ro');

      require("internal").wait(5.0);
    },

    tearDown: function() {
      db._drop("edges");
      db._drop("vertices");
      userModule.remove(users[0].username);
      userModule.remove(users[1].username);
      cs = [];
      coordinators = [];
    },

    testPregelForwardingSameUser: function() {
      const task = {
        algorithm: "pagerank",
        vertexCollections: ["vertices"],
        edgeCollections: ["edges"],
        params: {
          resultField: "result"
        }
      };
      const postResponse = sendRequest(users[0], 'POST', baseUrl, task, true);
      assertFalse(postResponse === undefined || postResponse === {});
      assertEqual(postResponse.status, 200);
      const pid = postResponse.body;
      assertTrue(pid > 1);

      // directly cancel pregel run (before it finishes)
      const deleteResponse = sendRequest(users[0], 'DELETE',  `${baseUrl}/${pid}`, {}, {}, false);
      assertFalse(deleteResponse === undefined || deleteResponse === {});
      assertFalse(deleteResponse.body.error);
      assertEqual(deleteResponse.status, 200);

      // check that pregel status state is canceled
      // need to wait for short time until pregel run was canceled
      var statusResponse;
      const sleepIntervalSeconds = 0.2;
      const maxWaitSeconds = 10;
      let wakeupsLeft = maxWaitSeconds / sleepIntervalSeconds;
      do {
        require("internal").sleep(sleepIntervalSeconds);
        statusResponse = sendRequest(users[0], 'GET', `${baseUrl}/${pid}`, {}, false);
        assertFalse(statusResponse === undefined || statusResponse === {});
        if (wakeupsLeft-- === 0) {
          assertTrue(false, "Pregel run was not canceled successfully but is in state " + statusResponse.body.state);
          return;
        }
      } while (!pregelTestHelpers.runCanceled(statusResponse.body));


    },

    testPregelForwardingDifferentUser: function() {
      const runCreator = users[0];
      const differentUser = users[1];
      const task = {
        algorithm: "pagerank",
        vertexCollections: ["vertices"],
        edgeCollections: ["edges"],
        params: {
          resultField: "result"
        }
      };
      const postResponse = sendRequest(runCreator, 'POST', baseUrl, task, true);
      assertFalse(postResponse === undefined || postResponse === {});
      assertEqual(postResponse.status, 200);
      const pid = postResponse.body;
      assertTrue(pid > 1);

      // pregel run cannot be deleted by different user
      const deleteResponseDifferentUser = sendRequest(differentUser, 'DELETE', `${baseUrl}/${pid}`, {}, {}, false);
      assertFalse(deleteResponseDifferentUser === undefined || deleteResponseDifferentUser === {});
      assertTrue(deleteResponseDifferentUser.body.error);
      assertEqual(deleteResponseDifferentUser.status, 404);

      // need to wait for short time until pregel run was definitly created (check that with runCreator user)
      const sleepIntervalSeconds = 0.2;
      const maxWaitSeconds = 10;
      let wakeupsLeft = maxWaitSeconds / sleepIntervalSeconds;
      var statusResponse;
      do {
        require("internal").sleep(sleepIntervalSeconds);
        statusResponse = sendRequest(runCreator, 'GET', `${baseUrl}/${pid}`, {}, false);
        assertFalse(statusResponse === undefined || statusResponse === {});
        if (wakeupsLeft-- === 0) {
          assertTrue(false, "Cannot retrieve status of pregel run, got response code " + statusResponse.status);
          return;
        }
      } while (statusResponse.status !== 200);

      // pregel status of run cannot be retrieved with different user
      const statusResponseDifferentUser = sendRequest(differentUser, 'GET', `${baseUrl}/${pid}`, {}, false);
      assertFalse(statusResponseDifferentUser === undefined || statusResponseDifferentUser === {});
      assertTrue(statusResponseDifferentUser.body.error);
      assertEqual(statusResponseDifferentUser.status, 404);

    },

  };
}

jsunity.run(PregelAuthSuite);

return jsunity.done();
