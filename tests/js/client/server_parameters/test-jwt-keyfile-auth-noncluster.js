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
const fs = require('fs');
const request = require("@arangodb/request");
const crypto = require('@arangodb/crypto');
const arango = require("@arangodb").arango;
const { makeAuthorizationHeaders } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;
// must not contain trailing spaces:
const JWT_key = "The quick brown foxx jumps  over";
let tmpDir;

if (getOptions === true) {
  // Create temporary directory for JWT secrets
  tmpDir = fs.getTempPath();
  let keyfile = fs.join(tmpDir, 'jwt-secret-keyfile');
  fs.write(keyfile, JWT_key);

  return {
    'server.authentication': 'true',
    'server.jwt-secret-keyfile': keyfile
  };
}

function testSuite() {

  // Helper function to make authenticated request
  let makeRequest = function() {
    let options = {
      method: "GET",
      url: IM.url + "/_api/version",
      ...makeAuthorizationHeaders(IM.options, JWT_key)
    };

    return request(options);
  };

  return {
    setUp: function() {
    },

    tearDown: function() {
    },
    testVersionReply: function() {
      const res = makeRequest();
      assertEqual(200, res.status, `Request with valid token from primary key should succeed ${JSON.stringify(res)}`);
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
