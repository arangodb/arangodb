/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, assertNotEqual, arango */

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
// / @author Kaveh Vahedipour
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const crypto = require('@arangodb/crypto');
const makeLicense = require('@arangodb/license-helper').makeLicense;
const pem2048 = require('@arangodb/license-helper').pem2048;
const pem4096 = require('@arangodb/license-helper').pem4096;

function adminLicenseSuite () {
  'use strict';

  return {

    testGet: function () {
      if (!require("internal").isEnterprise()) {
        let result = arango.GET('/_admin/license');
        assertEqual(result, {license:"none"});
      }
    },

    testPut: function () {
      if (!require("internal").isEnterprise()) {
        let result = arango.PUT('/_admin/license', "Hello World");
        assertTrue(result.error);
        assertEqual(result.code, 501);
        assertEqual(result.errorNum, 31);
      }
    },

    testUnsupportedMethods: function () {
      ["POST", "PATCH", "DELETE"].forEach((method) => {
        let result = arango[method]('/_admin/license', {});
        assertTrue(result.error);
        assertEqual(result.code, 405);
        assertEqual(result.errorNum, 405);
      });
    },

    testLicenseWithWrongDeploymentId: function () {
      if (!internal.isEnterprise()) {
        return; // Skip this test in community edition
      }
      
      // Get the actual deployment ID
      let deploymentResult = arango.GET_RAW("/_admin/deployment/id");
      if (deploymentResult.code !== 200) {
        return; // Skip if deployment ID is not available
      }
      let actualDeploymentId = deploymentResult.parsedBody.id;
      
      // Create a license with a wrong deployment ID (just modify one character)
      let wrongDeploymentId = actualDeploymentId.substring(0, actualDeploymentId.length - 1) + 
                              (actualDeploymentId[actualDeploymentId.length - 1] === 'a' ? 'b' : 'a');
      
      // Create a valid license but with wrong deployment ID
      let license = makeLicense(3600 * 24 * 365, pem2048, wrongDeploymentId);
      
      // Try to set the license - should fail
      let result = arango.PUT_RAW('/_admin/license', JSON.stringify(license));
      assertTrue(result.code >= 400, "License with wrong deployment ID should be rejected");
      assertTrue(result.parsedBody.error, "Response should indicate an error");
    },

    testLicenseWithCorrectDeploymentId: function () {
      if (!internal.isEnterprise()) {
        return; // Skip this test in community edition
      }
      
      // Get the actual deployment ID
      let deploymentResult = arango.GET_RAW("/_admin/deployment/id");
      if (deploymentResult.code !== 200) {
        return; // Skip if deployment ID is not available
      }
      let actualDeploymentId = deploymentResult.parsedBody.id;
      
      // Create a valid license with correct deployment ID
      let license = makeLicense(3600 * 24 * 365, pem2048, actualDeploymentId);
      
      // Try to set the license - should succeed
      let result = arango.PUT_RAW('/_admin/license', JSON.stringify(license));
      assertEqual(result.code, 201, "License with correct deployment ID should be accepted");
      assertFalse(result.parsedBody.error || false, "Response should not indicate an error");
    },

    testLicenseWithoutDeploymentId: function () {
      if (!internal.isEnterprise()) {
        return; // Skip this test in community edition
      }
      
      // Create a valid license without deployment ID (empty string = no deployment ID),
      // note that it must live longer than the previous one, thus the 366 days!
      let license = makeLicense(3600 * 24 * 366, pem2048, "");
      
      // Try to set the license - should succeed (deployment ID is optional)
      let result = arango.PUT_RAW('/_admin/license', JSON.stringify(license));
      assertEqual(result.code, 201, "License without deployment ID should be accepted");
      assertFalse(result.parsedBody.error || false, "Response should not indicate an error");
    },

    testGetLicenseReturnsGrantJson: function () {
      if (!internal.isEnterprise()) {
        return; // Skip this test in community edition
      }
      
      // First, set a valid license, note that it must live longer than the previous one,
      // thus the 367 days!
      let license = makeLicense(3600 * 24 * 367, pem2048, "");
      let putResult = arango.PUT_RAW('/_admin/license', JSON.stringify(license));
      if (putResult.code !== 201) {
        return; // Skip if we couldn't set a license
      }
      
      // Now GET the license and check that grant is included in clear JSON
      let result = arango.GET_RAW('/_admin/license');
      assertEqual(result.code, 200, "GET /_admin/license should return 200");
      
      let body = result.parsedBody;
      assertTrue(body.hasOwnProperty('grant'), "Response should contain 'grant' field");
      assertTrue(typeof body.grant === 'object', "Grant should be a JSON object");
      
      // Check that grant contains expected fields (but not issuer/subject which are filtered)
      assertTrue(body.grant.hasOwnProperty('version'), "Grant should contain version");
      assertTrue(body.grant.hasOwnProperty('features'), "Grant should contain features");
      assertTrue(body.grant.hasOwnProperty('notBefore'), "Grant should contain notBefore");
      assertTrue(body.grant.hasOwnProperty('notAfter'), "Grant should contain notAfter");
      
      // Verify issuer and subject are NOT included (they should be filtered out)
      assertEqual(body.grant.hasOwnProperty('issuer'), false, "Grant should NOT contain issuer (filtered)");
      assertEqual(body.grant.hasOwnProperty('subject'), false, "Grant should NOT contain subject (filtered)");
    },

  };
}

jsunity.run(adminLicenseSuite);

return jsunity.done();
