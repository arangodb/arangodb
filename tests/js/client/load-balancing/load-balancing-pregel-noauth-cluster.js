/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, require*/

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

const db = require("internal").db;
const request = require("@arangodb/request");
const url = require('url');
const _ = require("lodash");

function getCoordinators() {
  const isCoordinator = (d) => (_.toLower(d.role) === 'coordinator');
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    var pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const instanceInfo = JSON.parse(require('internal').env.INSTANCEINFO);
  return instanceInfo.arangods.filter(isCoordinator)
                              .map(toEndpoint)
                              .map(endpointToURL);
}

const servers = getCoordinators();

function PregelSuite () {
  'use strict';
  let cs = [];
  let coordinators = [];
  const baseUrl = `/_api/control_pregel`;

  function sendRequest(method, endpoint, body, usePrimary) {
    let res;
    const i = usePrimary ? 0 : 1;
    try {
      const envelope = {
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
      coordinators = getCoordinators();
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

      require("internal").wait(5.0);
    },

    tearDown: function() {
      db._drop("edges");
      db._drop("vertices");
      cs = [];
      coordinators = [];
    },

    testPregelForwarding: function() {
      let url = baseUrl;
      const task = {
        algorithm: "pagerank",
        vertexCollections: ["vertices"],
        edgeCollections: ["edges"],
        params: {
          resultField: "result"
        }
      };
      let result = sendRequest('POST', url, task, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 200);
      assertTrue(result.body > 1);

      const taskId = result.body;
      url = `${baseUrl}/${taskId}`;
      result = sendRequest('GET', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 200);
      assertEqual("running", result.body.state);

      require('internal').wait(5.0, false);

      url = `${baseUrl}/${taskId}`;
      result = sendRequest('DELETE', url, {}, {}, false);

      assertFalse(result === undefined || result === {});
      assertFalse(result.body.error);
      assertEqual(result.status, 200);
    },

  };
}

jsunity.run(PregelSuite);

return jsunity.done();
