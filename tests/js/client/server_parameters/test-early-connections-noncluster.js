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
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jwtSecret = 'haxxmann';
const jsunity = require('jsunity');
const request = require("@arangodb/request");
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const crypto = require('@arangodb/crypto');
const arango = require('@arangodb').arango;
let IM = global.instanceManager;

if (getOptions === true) {
  return {
    'server.jwt-secret': jwtSecret,
    'server.authentication': 'true',
    'server.early-connections': 'true',
    'server.failure-point': 'startListeningEarly',
  };
}

function testSuite() {
  const jwtRoot = crypto.jwtEncode(jwtSecret, {
    "server_id": "test",
    "iss": "arangodb",
    "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');

  return {
    tearDownAll: function() {
      let result = IM.debugClearFailAt("startListeningEarly");

      // wait until normal REST API responds normally
      let iterations = 0;
      while (iterations++ < 180) {
        let result = arango.GET_RAW("/_api/collection");
        if (result.code === 200) {
          break;
        }

        require("internal").sleep(0.5);
      }
    },

    testForbiddenWithoutJWT: function() {
      ["/_api/version", "/_admin/version", "/_admin/status", "/_api/collection", "/_admin/aardvark"].forEach((url) => {
        let result = request({ url: IM.url + url, method: "get" });
        assertEqual(401, result.status);
      });
    },
    
    testOkWithJWT: function() {
      ["/_api/version", "/_admin/version", "/_admin/status"].forEach((url) => {
        let result = arango.GET_RAW(url);
        assertEqual(200, result.code);
      });
    },
    
    testDisabledEndpoint: function() {
      ["/_api/collection", "/_api/transaction", "/_admin/aardvark"].forEach((url) => {
        let result = arango.GET_RAW(url);
        assertEqual(503, result.code);
      });
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
