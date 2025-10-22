/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, arango */

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
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.session-timeout': '5',
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}
const jsunity = require('jsunity');
const request = require('@arangodb/request');

function testSuite() {
  let baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  return {
    testSessionTimeout: function() {
      let result = request.get(baseUrl() + "/_api/version");
      // no access
      assertEqual(401, result.statusCode);

      result = request.post({
        url: baseUrl() + "/_open/auth", 
        body: {
          username: "root",
          password: ""
        },
        json: true
      });

      assertEqual(200, result.statusCode);
      const jwt = result.json.jwt;
      
      result = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });

      // access granted
      assertEqual(200, result.statusCode);

      require("internal").sleep(6);

      result = request.get({
        url: baseUrl() + "/_api/version",
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
        url: baseUrl() + "/_open/auth", 
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
        url: baseUrl() + "/_api/version",
        auth: { bearer: jwt }
      });
      assertEqual(200, result.statusCode);

      internal.sleep(8);

      result = request.get({
        url: baseUrl() + "/_api/version",
        auth: { bearer: jwt }
      });
      
      // JWT is still valid
      assertEqual(200, result.statusCode);

      internal.sleep(4);

      result = request.get({
        url: baseUrl() + "/_api/version",
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
    testArangoshAutomaticRenewal: function() {
      const internal = require("internal");
      
      // Reconnect with username/password - gets JWT token with 5-second expiry
      arango.reconnect(arango.getEndpoint(), "_system", "root", "");
      
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
      
      arango.reconnect(arango.getEndpoint(), "_system", "root", "");
      
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

jsunity.run(testSuite);
jsunity.run(arangoshTokenRenewalSuite);
return jsunity.done();
