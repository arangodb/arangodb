/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertMatch, assertTrue, assertFalse, assertNotEqual, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var internal = require("internal");
var arango = internal.arango;

function DeploymentIdSuite () {

  return {

    testGetDeploymentId : function () {
      var result = arango.GET_RAW("/_admin/deployment/id");
      assertEqual(200, result.code);
      assertTrue(result.parsedBody.hasOwnProperty("id"));
      
      var deploymentId = result.parsedBody.id;
      assertTrue(typeof deploymentId === "string");
      assertNotEqual("", deploymentId);
      
      // Deployment ID should be a valid UUID format
      assertMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i, deploymentId);
    },
    
    testGetDeploymentIdConsistency : function () {
      // Multiple calls should return the same deployment ID
      var result1 = arango.GET_RAW("/_admin/deployment/id");
      assertEqual(200, result1.code);
      
      var result2 = arango.GET_RAW("/_admin/deployment/id");
      assertEqual(200, result2.code);
      
      assertEqual(result1.parsedBody.id, result2.parsedBody.id);
    },

    testPostDeploymentIdNotAllowed : function () {
      try {
        var result = arango.POST("/_admin/deployment/id", {});
        assertEqual(405, result.code);
      } catch (e) {
        // Expected - method not allowed
        assertTrue(e.errorNum === internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code ||
                  e.errorMessage.indexOf("405") !== -1);
      }
    },

    testPutDeploymentIdNotAllowed : function () {
      try {
        var result = arango.PUT("/_admin/deployment/id", {});
        assertEqual(405, result.code);
      } catch (e) {
        // Expected - method not allowed
        assertTrue(e.errorNum === internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code ||
                  e.errorMessage.indexOf("405") !== -1);
      }
    },

    testDeleteDeploymentIdNotAllowed : function () {
      try {
        var result = arango.DELETE("/_admin/deployment/id");
        assertEqual(405, result.code);
      } catch (e) {
        // Expected - method not allowed
        assertTrue(e.errorNum === internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code ||
                  e.errorMessage.indexOf("405") !== -1);
      }
    },

    testInvalidDeploymentPath : function () {
      try {
        var result = arango.GET("/_admin/deployment/invalid");
        assertEqual(404, result.code);
      } catch (e) {
        // Expected - not found
        assertTrue(e.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code ||
                  e.errorMessage.indexOf("404") !== -1);
      }
    },

    testDeploymentRootPathNotFound : function () {
      try {
        var result = arango.GET("/_admin/deployment");
        assertEqual(404, result.code);
      } catch (e) {
        // Expected - not found
        assertTrue(e.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code ||
                  e.errorMessage.indexOf("404") !== -1);
      }
    }

  };
}

jsunity.run(DeploymentIdSuite);

return jsunity.done();
