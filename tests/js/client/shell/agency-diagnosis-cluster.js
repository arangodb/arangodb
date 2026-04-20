/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const internal = require('internal');
const request = require("@arangodb/request");
let IM = global.instanceManager;

////////////////////////////////////////////////////////////////////////////////
// Test Agency Diagnosis API
////////////////////////////////////////////////////////////////////////////////

function agencyDiagnosisAPISuite() {
  const agencyDiagnosisApi = IM.url + "/_admin/cluster/agencyDiagnosis";

  return {
    test_agency_diagnosis_get_endpoint_exists_verify_result: function () {
      let doc = request.get(agencyDiagnosisApi);
      
      // The endpoint should exist and return 200 for GET requests
      assertEqual(doc.statusCode, 200,
                 `Expected agency diagnosis endpoint to exist, got HTTP ${doc.statusCode}`);
      // If we get a successful response, validate the structure
      assertFalse(doc.json['error'], "Response should not contain an error");
      assertEqual(typeof doc.json['result'], "object");
      let result = doc.json.result;
      assertFalse(result.error);
      assertEqual(typeof result['diagnosis'], "object");
      assertTrue(Array.isArray(result.diagnosis['goodTests']));
      assertTrue(Array.isArray(result.diagnosis['badTests']));
    },

    test_agency_diagnosis_get_with_strict_parameter: function () {
      // Test the 'strict' parameter that the API supports
      let docStrict = request.get(agencyDiagnosisApi + "?strict=true");
      let docNonStrict = request.get(agencyDiagnosisApi + "?strict=false");
      
      assertEqual(docStrict.statusCode, 200);
      assertEqual(docNonStrict.statusCode, 200);
    },

    test_agency_diagnosis_method_restrictions: function () {
      // The API should support GET and POST, but let's test other methods are rejected
      let putResponse = request.put(agencyDiagnosisApi, "{}");
      assertEqual(putResponse.statusCode, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code,
                  "PUT should not be allowed");
      assertTrue(putResponse.json['error'], "PUT response should contain error");
      
      let deleteResponse = request.delete(agencyDiagnosisApi);
      assertEqual(deleteResponse.statusCode, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code,
                  "DELETE should not be allowed");
      assertTrue(deleteResponse.json['error'], "DELETE response should contain error");
      
      let patchResponse = request.patch(agencyDiagnosisApi, "{}");
      assertEqual(patchResponse.statusCode, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code,
                  "PATCH should not be allowed");
      assertTrue(patchResponse.json['error'], "PATCH response should contain error");
    }

  };
}

jsunity.run(agencyDiagnosisAPISuite);
return jsunity.done(); 
