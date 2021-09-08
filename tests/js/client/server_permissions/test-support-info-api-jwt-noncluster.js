/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, arango */

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

const crypto = require('@arangodb/crypto');
const request = require('@arangodb/request');

const jwtSecret = 'abc123';

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.support-info-api': "jwt",
    'server.jwt-secret': jwtSecret,
  };
}

const jsunity = require('jsunity');

function testSuite() {
  let endpoint = arango.getEndpoint();
  let db = require("@arangodb").db;

  let baseUrl = function () {
    return endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  const jwt = crypto.jwtEncode(jwtSecret, {
    "server_id": "ABCD",
    "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');

  return {
    testApiGetNoJwt : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/support-info",
      });
      assertEqual(401, res.status);
    },

    testApiGetJwt : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/support-info",
        auth: {
          bearer: jwt,
        }
      });
      assertEqual(200, res.status);
      assertTrue(res.json.hasOwnProperty("deployment"));
    },
    
    testApiGetJwtOtherDatabase : function() {
      const cn = "SupportInfoApiTest";
      db._createDatabase(cn);
      try {
        let res = request.get({
          url: baseUrl() + "/_db/" + cn + "/_admin/support-info",
          auth: { bearer: jwt },
        });
        assertEqual(403, res.status);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
