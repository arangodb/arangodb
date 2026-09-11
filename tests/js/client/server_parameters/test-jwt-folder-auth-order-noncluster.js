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
let IM = global.instanceManager;


// Dummy ES256 key pair 1 (primary)
const publicPrivateKeypair1 = `-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgE9UrCRndJypo4FJG
CZRZoPjLL1cD3WtipcIV4klbI6yhRANCAAQ4VbtPOezJa9iday7L1aXICQ+AY5Ua
0g6LZsHQRZdTVtIhaEyKhDASvzwdagTU9UY4dTcmTMA4XS7bIJt0n3ZO
-----END PRIVATE KEY-----
`;
const publicPrivateKeypair1Sha256 = crypto.sha256(publicPrivateKeypair1.trim());

const publicKey1 = `-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEOFW7TznsyWvYnWsuy9WlyAkPgGOV
GtIOi2bB0EWXU1bSIWhMioQwEr88HWoE1PVGOHU3JkzAOF0u2yCbdJ92Tg==
-----END PUBLIC KEY-----
`;
const publicKey1Sha256 = crypto.sha256(publicKey1.trim());

// Dummy ES256 key pair 2 (secondary)
const publicPrivateKeypair2 = `-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgUQAbplUZLUp+JVQJ
RrMwcTKW7qQAfdjQsBdi9vTOq+ChRANCAAQFywYqn11zi3YO1B5QJHi9shcfFb2o
qWUVFw/7F/PnJB6IvNy+Ap+9PjzjjQwKV7EtyGWrD6UihBTEhHB85c+K
-----END PRIVATE KEY-----
`;
const publicPrivateKeypair2Sha256 = crypto.sha256(publicPrivateKeypair2.trim());

const publicKey2 = `-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEBcsGKp9dc4t2DtQeUCR4vbIXHxW9
qKllFRcP+xfz5yQeiLzcvgKfvT48440MClexLchlqw+lIoQUxIRwfOXPig==
-----END PUBLIC KEY-----
`;
const publicKey2Sha256 = crypto.sha256(publicKey2.trim());

const hmac1 = `0000000000000000000000000000000000000000000000000000000000000000`;
const hmac1Sha256 = crypto.sha256(hmac1.trim());

const hmac2 = `0000000000000000000000000000000000000000000000000000000000000001`;
const hmac2Sha256 = crypto.sha256(hmac2.trim());

const hmac3 = `0000000000000000000000000000000000000000000000000000000000000002`;
const hmac3Sha256 = crypto.sha256(hmac3.trim());


let tmpDir;

if (getOptions === true) {
  // Create temporary directory for JWT secrets
  tmpDir = fs.getTempPath();
	const secretDir = fs.join(tmpDir, 'secrets');
  fs.makeDirectory(secretDir);

  // Write the key files to the temporary directory
  fs.write(fs.join(secretDir, 'jwt-secret-1.pem'), publicPrivateKeypair1);
  fs.write(fs.join(secretDir, 'jwt-secret-2.pem'), publicKey2);
  fs.write(fs.join(secretDir, '.jwt-secret-1.pem'), publicPrivateKeypair1);
  fs.write(fs.join(secretDir, 'jwt-secret-3.pem.tmp'), hmac1);
  fs.write(fs.join(secretDir, 'jwt-secret-4.pem'), hmac2);
  fs.write(fs.join(secretDir, 'jwt-secret-5.pem.tmp'), hmac3);
  fs.write(fs.join(secretDir, 'jwt-secret-6.pem'), hmac3);

  return {
    'server.authentication': 'true',
    'server.jwt-secret-folder': secretDir
  };
}

function testSuite() {
	
  // Helper function to create a JWT token
  let createToken = function(privateKey, payload) {
    const defaultPayload = {
      "server_id": "testserver",
      "iss": "arangodb",
      "exp": Math.floor(Date.now() / 1000) + 3600
    };
    const finalPayload = Object.assign({}, defaultPayload, payload || {});
    return crypto.jwtEncode(privateKey, finalPayload, 'ES256');
  };

  // Helper function to make authenticated request
  let makeRequest = function(token) {
    let options = {
      method: "GET",
      url: IM.url + "/_admin/server/jwt"
    };
    if (token !== undefined) {
      if (token === null) {
        // No authentication
      } else {
        options.auth = { bearer: token };
      }
    }

    return request(options);
  };

  return {
    setUp: function() {
    },

    tearDown: function() {
    },

    testKeysLoadedInCorrectOrder : function () {
      const token = createToken(publicPrivateKeypair1);
      const res = makeRequest(token);

      assertEqual(200, res.status, "Request with valid token from primary key should succeed");
      assertEqual(res.json.result.active.sha256, crypto.sha256(publicPrivateKeypair1.trim()),
                  `Expecting ${publicPrivateKeypair1.trim} with sha256 ${publicPrivateKeypair1Sha256} to be the primary secret`);

      assertEqual(res.json.result.passive.length, 3,
                  `Expecting 3 passive secrets`); 
      assertEqual(res.json.result.passive[0].sha256, publicKey2Sha256,
                  `Expecting passive secret 0 to be ${publicKey2Sha256}`); 
      assertEqual(res.json.result.passive[1].sha256, hmac2Sha256,
                  `Expecting passive secret 1 to be ${hmac2Sha256}`); 
      assertEqual(res.json.result.passive[2].sha256, hmac3Sha256,
                  `Expecting passive secret 2 to be ${hmac3Sha256}`); 
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
