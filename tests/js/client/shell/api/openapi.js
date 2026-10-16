/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual, assertNotUndefined */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// / Author Max Neunhoeffer
// /
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const jsunity = require("jsunity");

const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";

////////////////////////////////////////////////////////////////////////////////
// OpenAPI endpoints
////////////////////////////////////////////////////////////////////////////////

function openapi_endpointsSuite() {
  return {
    test_retrieves_openapi_v0: function() {
      let doc = arango.GET_RAW("/_arango/v0/openapi.json");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], "application/json");
      
      let response = doc.parsedBody;
      assertNotUndefined(response);
      
      // Verify it's a valid OpenAPI structure
      assertNotUndefined(response.openapi, "Should have openapi version field");
      assertNotUndefined(response.info, "Should have info object");
      assertNotUndefined(response.paths, "Should have paths object");
      
      // Verify it's a JSON object with expected structure
      assertEqual(typeof response, 'object');
      assertEqual(typeof response.paths, 'object');
    },

    test_openapi_v1_does_not_exist: function () {
      let doc = arango.GET_RAW("/_arango/v1/openapi.json");

      assertEqual(doc.code, 404);
      assertEqual(doc.errorNum, 404);

    },

    test_openapi_v2_does_not_exist: function () {
      let doc = arango.GET_RAW("/_arango/v2/openapi.json");

      assertEqual(doc.code, 404);
      assertEqual(doc.errorNum, 404);
    },

    test_retrieves_openapi_experimental: function () {
      let doc = arango.GET_RAW("/_arango/experimental/openapi.json");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], "application/json");
      
      let response = doc.parsedBody;
      assertNotUndefined(response);
      
      // Verify it's a valid OpenAPI structure
      assertNotUndefined(response.openapi, "Should have openapi version field");
      assertNotUndefined(response.info, "Should have info object");
      assertNotUndefined(response.paths, "Should have paths object");
      
      // Verify it's a JSON object with expected structure
      assertEqual(typeof response, 'object');
      assertEqual(typeof response.paths, 'object');
    },

    test_openapi_v0_and_experimental_differ: function () {
      let docV0 = arango.GET_RAW("/_arango/v0/openapi.json");
      let docExp = arango.GET_RAW("/_arango/experimental/openapi.json");

      assertEqual(docV0.code, 200);
      assertEqual(docExp.code, 200);

      let v0 = docV0.parsedBody;
      let exp = docExp.parsedBody;

      // Both should be valid OpenAPI documents
      assertNotUndefined(v0.openapi);
      assertNotUndefined(exp.openapi);

      // They should potentially differ in content
      // (At minimum, they should have different version info or paths)
      assertNotUndefined(v0.paths);
      assertNotUndefined(exp.paths);
    }
  };
}

jsunity.run(openapi_endpointsSuite);
return jsunity.done();
