/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, assertFalse, arango */

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
/// @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const crypto = require('@arangodb/crypto');

// Dummy ES256 key pair 1 (primary)
const privateKey1 = `-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgE9UrCRndJypo4FJG
CZRZoPjLL1cD3WtipcIV4klbI6yhRANCAAQ4VbtPOezJa9iday7L1aXICQ+AY5Ua
0g6LZsHQRZdTVtIhaEyKhDASvzwdagTU9UY4dTcmTMA4XS7bIJt0n3ZO
-----END PRIVATE KEY-----
`;

const publicKey1 = `-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEOFW7TznsyWvYnWsuy9WlyAkPgGOV
GtIOi2bB0EWXU1bSIWhMioQwEr88HWoE1PVGOHU3JkzAOF0u2yCbdJ92Tg==
-----END PUBLIC KEY-----
`;

// Dummy ES256 key pair 2 (secondary)
const privateKey2 = `-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgUQAbplUZLUp+JVQJ
RrMwcTKW7qQAfdjQsBdi9vTOq+ChRANCAAQFywYqn11zi3YO1B5QJHi9shcfFb2o
qWUVFw/7F/PnJB6IvNy+Ap+9PjzjjQwKV7EtyGWrD6UihBTEhHB85c+K
-----END PRIVATE KEY-----
`;

const publicKey2 = `-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEBcsGKp9dc4t2DtQeUCR4vbIXHxW9
qKllFRcP+xfz5yQeiLzcvgKfvT48440MClexLchlqw+lIoQUxIRwfOXPig==
-----END PUBLIC KEY-----
`;

let tmpDir;

if (getOptions === true) {
  // Create temporary directory for JWT secrets
  const internal = require('internal');
  tmpDir = fs.getTempPath();
  fs.makeDirectory(fs.join(tmpDir, 'secrets'));

  // Write the key files to the temporary directory
  fs.write(fs.join(tmpDir, 'secrets', 'jwt-secret-1.pem'), privateKey1);
  fs.write(fs.join(tmpDir, 'secrets', 'jwt-secret-2.pem'), publicKey2);

  return {
    'server.authentication': 'true',
    'server.jwt-secret-folder': fs.join(tmpDir, 'secrets')
  };
}

const jsunity = require('jsunity');
const request = require("@arangodb/request");

function testSuite() {
  const baseUrl = arango.getEndpoint().replace(/^tcp:/, 'http:');

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
      url: baseUrl + "/_api/version"
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

    testRequestWithValidTokenFromPrimaryKey : function () {
      const token = createToken(privateKey1);
      const res = makeRequest(token);
      assertEqual(200, res.status, "Request with valid token from primary key should succeed");
    },

    testRequestWithValidTokenFromSecondaryKey : function () {
      const token = createToken(privateKey2);
      const res = makeRequest(token);
      assertEqual(200, res.status, "Request with valid token from secondary key should succeed");
    },

    testRequestWithoutToken : function () {
      const res = makeRequest(null);
      assertEqual(401, res.status, "Request without token should fail with 401");
    },

    testRequestWithInvalidToken : function () {
      const res = makeRequest("invalid.token.here");
      assertEqual(401, res.status, "Request with invalid token should fail with 401");
    },

    testRequestWithExpiredToken : function () {
      const token = createToken(privateKey1, {
        "exp": Math.floor(Date.now() / 1000) - 3600  // Expired 1 hour ago
      });
      const res = makeRequest(token);
      assertEqual(401, res.status, "Request with expired token should fail with 401");
    },

    testRequestWithWrongSignature : function () {
      // Create a token with primary key but modify it slightly
      const token = createToken(privateKey1);
      const parts = token.split('.');
      // Corrupt the signature by changing the last character
      if (parts.length === 3) {
        const firstChar = parts[2].charAt(0);
        const newFirstChar = firstChar === 'a' ? 'b' : 'a';
        parts[2] = newFirstChar + parts[2].substring(1, parts[2].length - 1);
      }
      const corruptedToken = parts.join('.');

      const res = makeRequest(corruptedToken);
      assertEqual(401, res.status, "Request with corrupted signature should fail with 401");
    },

    testRequestWithMissingIssuer : function () {
      const token = createToken(privateKey1, {
        "iss": undefined  // Missing issuer
      });
      const res = makeRequest(token);
      // This might succeed or fail depending on server configuration
      // Adjust assertion based on expected behavior
      assertEqual(401, res.status, "Request should have been declined (missing issuer)");
    },

    testRequestWithWrongIssuer : function () {
      const token = createToken(privateKey1, {
        "iss": "wrongissuer"
      });
      const res = makeRequest(token);
      // This might succeed or fail depending on server configuration
      // Adjust assertion based on expected behavior
      assertEqual(401, res.status, "Request should have been declined (wrong issuer)");
    },

    testMultipleRequestsWithDifferentKeys : function () {
      // Test that both keys work consistently
      const token1 = createToken(privateKey1);
      const token2 = createToken(privateKey2);

      for (let i = 0; i < 5; i++) {
        const res1 = makeRequest(token1);
        assertEqual(200, res1.status, "Request " + i + " with primary key should succeed");

        const res2 = makeRequest(token2);
        assertEqual(200, res2.status, "Request " + i + " with secondary key should succeed");
      }
    },

    testRequestToProtectedEndpoint : function () {
      // Test access to a more sensitive endpoint
      const token = createToken(privateKey1);
      const res = request({
        method: "GET",
        url: baseUrl + "/_api/database/current",
        auth: { bearer: token }
      });
      assertEqual(200, res.status, "Request to protected endpoint should succeed with valid token");
    },

    testRequestToProtectedEndpointWithoutAuth : function () {
      const res = request({
        method: "GET",
        url: baseUrl + "/_api/database/current"
      });
      assertEqual(401, res.status, "Request to protected endpoint without token should fail");
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
