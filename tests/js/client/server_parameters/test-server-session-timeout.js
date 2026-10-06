/*jshint globalstrict:false, strict:false */
/* global getOptions */

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const arangodb = require('@arangodb');
const arango = arangodb.arango;
const request = require("@arangodb/request");
let IM = global.instanceManager;

if (getOptions === true) {
  return {
    'server.session-timeout': '5',
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}

function testSuite() {
  return {
    testSessionTimeout: function() {
      let result = request.get(IM.url + "/_api/version");
      // no access
      assertEqual(401, result.statusCode);

      result = request.post({
        url: IM.url + "/_open/auth", 
        body: {
          username: "root",
          password: ""
        },
        json: true
      });

      assertEqual(200, result.statusCode);
      const jwt = result.json.jwt;
      
      result = request.get({
        url: IM.url + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });

      // access granted
      assertEqual(200, result.statusCode);

      require("internal").sleep(6);

      result = request.get({
        url: IM.url + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });

      // JWT is still valid
      assertEqual(401, result.statusCode);
    },

    testCustomExpiryTime: function() {
      const internal = require("internal");
      
      // Request JWT with custom 3-second expiry
      let result = request.post({
        url: IM.url + "/_open/auth", 
        body: {
          username: "root",
          password: "",
          expiryTime: 10
        },
        json: true
      });

      assertEqual(200, result.statusCode);
      const jwt = result.json.jwt;

      // Token should work immediately
      result = request.get({
        url: IM.url + "/_api/version",
        auth: { bearer: jwt }
      });
      assertEqual(200, result.statusCode);

      internal.sleep(8);

      result = request.get({
        url: IM.url + "/_api/version",
        auth: { bearer: jwt }
      });
      
      // JWT is still valid
      assertEqual(200, result.statusCode);

      internal.sleep(4);

      result = request.get({
        url: IM.url + "/_api/version",
        auth: { bearer: jwt }
      });
      
      // JWT expired, here is without renewal
      assertEqual(401, result.statusCode);
    },
  };
}

function arangoshTokenRenewalSuite() {
  'use strict';
  
  return {
    setUpAll() {
      IM.rememberConnection();
    },
    tearDownAll() {
      IM.reconnectMe();
    },
    testArangoshAutomaticRenewal: function() {
      const internal = require("internal");
      
      // Reconnect with username/password - gets JWT token with 5-second expiry
      arango.reconnect(IM.endpoint, "_system", "root", "");
      
      // Make requests over 12 seconds - token expires after 5 seconds
      // Automatic renewal should keep requests working
      for (let i = 0; i < 6; i++) {
        let result = arango.GET_RAW("/_api/version");
        assertEqual(200, result.code);
        internal.sleep(2);
      }
    },

    testArangoshRenewalAfterExpiry: function() {
      const internal = require("internal");
      
      arango.reconnect(IM.endpoint, "_system", "root", "");
      
      let result = arango.GET_RAW("/_api/version");
      assertEqual(200, result.code);
      
      // Wait past token expiry (5 seconds)
      internal.sleep(6);
      
      // Request should still work (auto-renewed)
      result = arango.GET_RAW("/_api/version");
      assertEqual(200, result.code);
    },
  };
}

function arangoshProvidedTokenRenewalSuite() {
  'use strict';
  const runArangosh = require("@arangodb/testutils/client-tools").run.arangoshCmd;

  const fetchToken = function(username, password) {
    return request.post({
      url: IM.url + "/_open/auth",
      body: { username, password },
      json: true
    }).json.jwt;
  };

  // starts arangosh with the token and runs the script in it
  const runArangoshWithToken = function(token, script, renewalThresholdSeconds = 300) {
    // the helper would otherwise add --server.username, which is not allowed
    // together with --server.jwt-token
    const { username, password, ...options } = IM.options;
    return runArangosh(options, IM, {
      "server.jwt-token": token,
      "server.jwt-renewal-threshold": String(renewalThresholdSeconds),
      "javascript.execute-string": script,
    }, "");
  };

  // the token lives 5 seconds; the script keeps sending requests for 12
  const requestsFor12Seconds = `
    for (let i = 0; i < 6; ++i) {
      const result = arango.GET_RAW("/_api/version");
      if (result.code !== 200) {
        throw new Error("request failed with HTTP " + result.code);
      }
      require("internal").sleep(2);
    }`;

  return {
    testArangoshRenewsProvidedToken: function() {
      const res = runArangoshWithToken(fetchToken("root", ""), requestsFor12Seconds, 2);
      assertTrue(res.status, JSON.stringify(res));
    },

    // with a threshold of 0 the token is only renewed once it has expired,
    // which the server refuses
    testArangoshAbortsWithoutRenewal: function() {
      const res = runArangoshWithToken(fetchToken("root", ""), requestsFor12Seconds, 0);
      assertFalse(res.status, JSON.stringify(res));
    },

    // the threshold can be changed at runtime; the background renewal must
    // use the new value, otherwise the token expires after 5 seconds
    testArangoshAppliesChangedRenewalThreshold: function() {
      const res = runArangoshWithToken(fetchToken("root", ""), `
        arango.jwtRenewalThreshold(2);
        if (arango.jwtRenewalThreshold() !== 2) {
          throw new Error("threshold is " + arango.jwtRenewalThreshold());
        }
        ${requestsFor12Seconds}`, 0);
      assertTrue(res.status, JSON.stringify(res));
    },

    testArangoshReportsTheTokenUser: function() {
      const users = require("@arangodb/users");
      users.save("tokenuser", "tokenpw");
      users.grantDatabase("tokenuser", "_system", "rw");
      try {
        const res = runArangoshWithToken(fetchToken("tokenuser", "tokenpw"), `
          if (arango.connectedUser() !== "tokenuser") {
            throw new Error("connected user is " + arango.connectedUser());
          }`);
        assertTrue(res.status, JSON.stringify(res));
      } finally {
        users.remove("tokenuser");
      }
    },
  };
}

jsunity.run(testSuite);
jsunity.run(arangoshTokenRenewalSuite);
jsunity.run(arangoshProvidedTokenRenewalSuite);
return jsunity.done();
