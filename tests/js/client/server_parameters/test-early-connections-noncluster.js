/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jwtSecret = 'haxxmann';

if (getOptions === true) {
  return {
    'server.jwt-secret': jwtSecret,
    'server.authentication': 'true',
    'server.early-connections': 'true',
    'server.failure-point': 'startListeningEarly',
  };
}
const jsunity = require('jsunity');
const request = require('@arangodb/request').request;
const crypto = require('@arangodb/crypto');

function testSuite() {
  const jwtRoot = crypto.jwtEncode(jwtSecret, {
    "server_id": "test",
    "iss": "arangodb",
    "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');
  
  let baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };
      
  return {
    tearDownAll: function() {
      let result = request({ url: baseUrl() + "/_admin/debug/failat/startListeningEarly", method: "delete", auth: { bearer: jwtRoot } });
      assertEqual(200, result.status);

      // wait until normal REST API responds normally
      let iterations = 0;
      while (iterations++ < 60) {
        let result = request({ url: baseUrl() + "/_api/collection", method: "get", auth: { bearer: jwtRoot } });
        if (result.status === 200) {
          break;
        }

        require("internal").sleep(0.5);
      }
    },

    testForbiddenWithoutJWT: function() {
      ["/_api/version", "/_admin/version", "/_admin/status", "/_api/collection", "/_admin/aardvark"].forEach((url) => {
        let result = request({ url: baseUrl() + url, method: "get" });
        assertEqual(401, result.status);
      });
    },
    
    testOkWithJWT: function() {
      ["/_api/version", "/_admin/version", "/_admin/status"].forEach((url) => {
        let result = request({ url: baseUrl() + url, method: "get", auth: { bearer: jwtRoot } });
        assertEqual(200, result.status);
      });
    },
    
    testDisabledEndpoint: function() {
      ["/_api/collection", "/_api/transaction", "/_admin/aardvark"].forEach((url) => {
        let result = request({ url: baseUrl() + url, method: "get", auth: { bearer: jwtRoot } });
        assertEqual(503, result.status);
      });
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
