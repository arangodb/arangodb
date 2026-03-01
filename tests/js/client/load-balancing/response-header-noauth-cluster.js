/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, assertNotUndefined */

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
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");

const db = require("internal").db;
const request = require("@arangodb/request");
const url = require('url');
const _ = require("lodash");
const getCoordinatorEndpoints = require('@arangodb/test-helper').getCoordinatorEndpoints;

const servers = getCoordinatorEndpoints();

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
      coordinators = getCoordinatorEndpoints();
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
      let url = '/_api/cursor';
      const body = { query: 'RETURN 1' };
      const headers = {
        "X-Arango-Async": "store"
      };
      let result = sendRequest('POST', url, headers, body, true);

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
          break;
        }
        require("internal").wait(1.0, false);
      }
      assertEqual(result.status, 200);
      assertNotUndefined(result.headers["x-arango-async-id"]);
    },

    testForwardingPut: function() {
      const createUrl = '/_api/cursor';
      const createBody = { query: 'FOR i IN 1..1000 RETURN i', batchSize: 1 };

      let create = sendRequest('POST', createUrl, {}, createBody, true);
      assertFalse(create === undefined || create === {});
      assertEqual(create.status, 201);
      assertTrue(create.body.hasMore === true);
      assertNotUndefined(create.body.id);

      const cursorId = create.body.id;
      let url = `/_api/cursor/${cursorId}`;
      const headers = { "X-Arango-Async": "store" };

      let result = sendRequest('PUT', url, headers, {}, true);

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
          break;
        }
        require("internal").wait(1.0, false);
      }
      assertEqual(result.status, 200);
      assertNotUndefined(result.headers["x-arango-async-id"]);

      try {
        sendRequest('DELETE', `/_api/cursor/${cursorId}`, {}, {}, true);
      } catch (e) {}
    },
    
    testForwardingNoConnectionHeader: function() {
      let url = '/_api/cursor';
      const body = { query: 'RETURN 1' };
      const headers = {
        "X-Arango-Async": "store",
      };
      let result = sendRequest('POST', url, headers, body, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);
      assertEqual("Keep-Alive", result.headers["connection"]);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      let tries = 0;
      while (++tries < 30) {
        result = sendRequest('PUT', url, {}, {}, false);

        if (result.status === 200) {
          break;
        }
        require("internal").wait(0.5, false);
      }
      assertEqual(result.status, 200);
      assertNotUndefined(result.headers["x-arango-async-id"]);
      assertEqual("Keep-Alive", result.headers["connection"]);
    },
    
    testForwardingConnectionHeaderClose: function() {
      let url = '/_api/cursor';
      const body = { query: 'RETURN 1' };
      const headers = {
        "X-Arango-Async": "store",
        "Connection": "Close"
      };
      let result = sendRequest('POST', url, headers, body, true);

      assertFalse(result === undefined || result === {});
      assertEqual(result.body, {});
      assertEqual(result.status, 202);
      assertFalse(result.headers["x-arango-async-id"] === undefined);
      assertTrue(result.headers["x-arango-async-id"].match(/^\d+$/).length > 0);
      assertEqual("Close", result.headers["connection"]);

      const jobId = result.headers["x-arango-async-id"];
      url = `${baseJobUrl}/${jobId}`;
      let tries = 0;
      while (++tries < 30) {
        result = sendRequest('PUT', url, {"Connection": "Close"}, {}, false);

        if (result.status === 200) {
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
