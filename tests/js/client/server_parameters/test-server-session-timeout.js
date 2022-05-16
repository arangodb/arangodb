/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server parameters
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

      require("internal").sleep(7);

      result = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });

      // JWT expired
      assertEqual(401, result.statusCode);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
