/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
///
/// @file
///
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
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}
const jsunity = require('jsunity');
const request = require('@arangodb/request').request;
const crypto = require('@arangodb/crypto');

function testSuite() {
  return {
    testUnauthorized : function() {
      let result = request({ url: "/_api/version", method: "get" });
      assertEqual(401, result.status);
      assertTrue(result.headers.hasOwnProperty('www-authenticate'));
    },
    
    testUnauthorizedOmit : function() {
      let result = request({ url: "/_api/version", method: "get", headers: { 'x-omit-WWW-authenticate': 'abc' } });
      assertEqual(401, result.status);
      assertFalse(result.headers.hasOwnProperty('www-authenticate'));
    },
    
    testAuthorized : function() {
      const jwtSecret = 'haxxmann';
      const jwtRoot = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodb",
        "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');
      let result = request({ url: "/_api/version", method: "get", auth: { bearer: jwtRoot } });
      assertEqual(200, result.status);
      assertFalse(result.headers.hasOwnProperty('www-authenticate'));
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
