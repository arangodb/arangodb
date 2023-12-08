/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertMatch, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
    'http.handle-content-encoding-for-unauthenticated-requests': 'false',
  };
}
const jsunity = require('jsunity');
const crypto = require('@arangodb/crypto');
const request = require('@arangodb/request');
const internal = require('internal');
const fs = require("fs");

const jwtRoot = () => {
  const jwtSecret = 'haxxmann';
  return crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodb",
        "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');
};

function HandleContentEncodingSuite() {
  return {
    testPublicAPIWhenUnauthorized: function() {
      // send an unauthenticated request to a public API,
      // using the Content-Encoding/Transfer-Encoding: gzip/deflate headers
      // this is supposed to fail
      const headers = [
        {"Content-Encoding": "gzip"},
        {"Content-Encoding": "deflate"},
        {"Content-Encoding": "piffpaff"},
      ];
      headers.forEach((h) => {
        let res = request.post({
          url: arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + "/_open/auth", 
          body: "piff", 
          headers: h
        });
        assertEqual(400, res.status);
        assertMatch(/handling Content-Encoding.*turned off.*unauthenticated requests/, res.json.errorMessage);
      });
    },
    
    testPublicAPIWhenAuthorized: function() {
      // send an authenticated request to a public API,
      // using the Content-Encoding/Transfer-Encoding: gzip/deflate headers
      const headers = [
        {"Content-Encoding": "gzip"},
        {"Content-Encoding": "deflate"},
      ];
      headers.forEach((h) => {
        let res = request.post({
          url: arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + "/_open/auth", 
          body: "piff", 
          headers: h,
          auth: { bearer: jwtRoot() },
        });
        // this is still supposed to fail, because we are not sending any
        // valid content
        assertEqual(400, res.status);
        assertMatch(/decoding error occurred while handling Content-Encoding/, res.json.errorMessage);
      });
    },
    
    testPrivateAPIWhenAuthorized: function() {
      // send an authenticated request to a private API,
      // using the Content-Encoding/Transfer-Encoding: gzip/deflate headers
      const headers = [
        {"Content-Encoding": "gzip"},
        {"Content-Encoding": "deflate"},
      ];
      headers.forEach((h) => {
        let res = request.post({
          url: arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + "/_api/version", 
          body: "piff", 
          headers: h,
          auth: { bearer: jwtRoot() },
        });
        // this is still supposed to fail, because we are not sending any
        // valid content
        assertEqual(400, res.status);
        assertMatch(/decoding error occurred while handling Content-Encoding/, res.json.errorMessage);
      });
    },

    testPrivateAPISendGzipData: function() {
      let compressedFile = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'import', 'import-1.json.gz'));
      let compressedBuffer = fs.readFileSync(compressedFile);
      let uncompressedBuffer = fs.readGzip(compressedFile);

      let res = request.post({
        url: arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + "/_admin/echo", 
        body: compressedBuffer, 
        headers: {"Content-Encoding": "gzip"},
        auth: { bearer: jwtRoot() },
      });
      assertEqual(200, res.status);
      assertEqual(res.json.requestBody, uncompressedBuffer.toString());
    },

  };
}

jsunity.run(HandleContentEncodingSuite);
return jsunity.done();
