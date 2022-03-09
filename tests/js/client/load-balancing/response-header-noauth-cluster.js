/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, assertNotUndefined, require*/

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
/// @author Jan Steemann
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

function ResponseHeadersSuite () {
  'use strict';
  let cs = [];
  let coordinators = [];
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

      require("internal").wait(2);
    },

    tearDown: function() {
      const url = `${baseJobUrl}/all`;
      const result = sendRequest('DELETE', url, {}, {}, true);
      assertFalse(result === undefined || result === {});
      assertEqual(result.status, 200);
      coordinators = [];
    },

    testForwardingGet: function() {
      let url = '/_api/version';
      const headers = {
        "X-Arango-Async": "store"
      };
      let result = sendRequest('GET', url, headers, null, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      let tries = 0;
      while (++tries < 30) {
        result = sendRequest('PUT', url, {}, {}, false);

        if (result.status === 200) {
          // jobs API may return HTTP 204 until job is ready
          break;
        }
        require("internal").wait(1.0, false);
      }
      assertEqual(result.status, 200);
      assertNotUndefined(result.headers["x-arango-async-id"]);
    },

    testForwardingPost: function() {
      let url = '/_api/version';
      const headers = {
        "X-Arango-Async": "store"
      };
      let result = sendRequest('POST', url, headers, null, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      let tries = 0;
      while (++tries < 30) {
        result = sendRequest('PUT', url, {}, {}, false);

        if (result.status === 200) {
          // jobs API may return HTTP 204 until job is ready
          break;
        }
        require("internal").wait(1.0, false);
      }
      assertEqual(result.status, 200);
      assertNotUndefined(result.headers["x-arango-async-id"]);
    },

    testForwardingPut: function() {
      let url = '/_api/version';
      const headers = {
        "X-Arango-Async": "store"
      };
      let result = sendRequest('PUT', url, headers, null, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      let tries = 0;
      while (++tries < 30) {
        result = sendRequest('PUT', url, {}, {}, false);

        if (result.status === 200) {
          // jobs API may return HTTP 204 until job is ready
          break;
        }
        require("internal").wait(1.0, false);
      }
      assertEqual(result.status, 200);
      assertNotUndefined(result.headers["x-arango-async-id"]);
    },
    
    testForwardingNoConnectionHeader: function() {
      let url = '/_api/version';
      const headers = {
        "X-Arango-Async": "store",
      };
      let result = sendRequest('POST', url, headers, null, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);
      // connection header should be filtered out by cluster-internal 
      // request forwarding, but not from responses given to end users
      assertEqual("Keep-Alive", result.headers["connection"]);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      let tries = 0;
      while (++tries < 30) {
        result = sendRequest('PUT', url, {}, {}, false);

        if (result.status === 200) {
          // jobs API may return HTTP 204 until job is ready
          break;
        }
        require("internal").wait(0.5, false);
      }
      assertEqual(result.status, 200);
      assertNotUndefined(result.headers["x-arango-async-id"]);
      assertEqual("Keep-Alive", result.headers["connection"]);
    },
    
    testForwardingConnectionHeaderClose: function() {
      let url = '/_api/version';
      const headers = {
        "X-Arango-Async": "store",
        "Connection": "Close"
      };
      let result = sendRequest('POST', url, headers, null, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);
      // connection header should be filtered out by cluster-internal 
      // request forwarding, but not from responses given to end users
      assertEqual("Close", result.headers["connection"]);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      let tries = 0;
      while (++tries < 30) {
        result = sendRequest('PUT', url, {"Connection": "Close"}, {}, false);

        if (result.status === 200) {
          // jobs API may return HTTP 204 until job is ready
          break;
        }
        require("internal").wait(0.5, false);
      }
      assertEqual(result.status, 200);
      assertNotUndefined(result.headers["x-arango-async-id"]);
      assertEqual("Close", result.headers["connection"]);
    },
  };
}

jsunity.run(ResponseHeadersSuite);

return jsunity.done();
