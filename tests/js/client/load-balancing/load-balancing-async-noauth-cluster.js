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

function AsyncSuite () {
  'use strict';
  const cns = ["animals", "fruits"];
  const keys = [
    ["ant", "bird", "cat", "dog"],
    ["apple", "banana", "coconut", "date"]
  ];
  let cs = [];
  let coordinators = [];
  const baseCursorUrl = `/_api/cursor`;
  const baseJobUrl = `/_api/job`;

  function sendRequest(method, endpoint, headers, body, usePrimary) {
    let res;
    const i = usePrimary ? 0 : 1;
    try {
      const envelope = {
        headers,
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
      for (let i = 0; i < cns.length; i++) {
        db._drop(cns[i]);
        cs.push(db._create(cns[i]));
        assertTrue(cs[i].name() === cns[i]);
        for (let key in keys[i]) {
          cs[i].save({ _key: key });
        }
      }

      require("internal").wait(2);
    },

    tearDown: function() {
      const url = `${baseJobUrl}/all`;
      const result = sendRequest('DELETE', url, {}, {}, true);
      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 200);

      db._drop(cns[0]);
      db._drop(cns[1]);
      cs = [];
      coordinators = [];
    },

    testAsyncCursorForwardingBasic: function() {
      let url = baseCursorUrl;
      const headers = {
        "X-Arango-Async": "store"
      };
      const query = {
        query: `FOR i IN 1..10 LET x = sleep(1.0) FILTER i == 5 RETURN 42`,
      };
      let result = sendRequest('POST', url, headers, query, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      result = sendRequest('PUT', url, {}, {}, false);

      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 204);
      assertEqual(result.headers["x-arango-async-id"], undefined);

      require('internal').wait(11.0, false);

      url = `${baseJobUrl}/${jobId}`;
      let tries = 0;
      while (++tries < 30) {
        result = sendRequest('PUT', url, {}, {}, false);

        assertFalse(result === undefined || result === {});
        assertFalse(result.body.error);
        if (result.status === 201) {
          // jobs API may return HTTP 204 until job is ready
          break;
        }
        require("internal").wait(1.0, false);
      }
      assertEqual(result.status, 201);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertEqual(result.body.result.length, 1);
      assertEqual(result.body.result[0], 42);
      assertFalse(result.body.hasMore);
    },

    testAsyncCursorForwardingDelete: function() {
      let url = baseCursorUrl;
      const headers = {
        "X-Arango-Async": "store"
      };
      const query = {
        query: `FOR i IN 1..10 LET x = sleep(1.0) FILTER i == 5 RETURN 42`,
      };
      let result = sendRequest('POST', url, headers, query, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      result = sendRequest('PUT', url, {}, {}, false);

      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 204);
      assertEqual(result.headers["x-arango-async-id"], undefined);

      url = `${baseJobUrl}/${jobId}`;
      result = sendRequest('DELETE', url, {}, {}, false);

      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 200);
      assertEqual(result.headers["x-arango-async-id"], undefined);
    },

    testAsyncCursorForwardingCancel: function() {
      let url = baseCursorUrl;
      const headers = {
        "X-Arango-Async": "store"
      };
      const query = {
        query: `FOR i IN 1..10 LET x = sleep(1.0) FILTER i == 5 RETURN 42`,
      };
      let result = sendRequest('POST', url, headers, query, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      result = sendRequest('PUT', url, {}, {}, false);

      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 204);
      assertEqual(result.headers["x-arango-async-id"], undefined);

      url = `${baseJobUrl}/${jobId}/cancel`;
      result = sendRequest('PUT', url, {}, {}, false);

      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 200);
      assertEqual(result.headers["x-arango-async-id"], undefined);
    },

  };
}

jsunity.run(AsyncSuite);

return jsunity.done();
