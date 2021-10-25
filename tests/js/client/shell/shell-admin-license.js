/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2021 ArangoDB Inc, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB Inc, Cologne, Germany
// /
// / @author Kaveh Vahedipour
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const crypto = require('@arangodb/crypto');;

function adminLicenseSuite () {
  'use strict';
  var skip = false;

  return {

    testSetup: function () {
      var result;
      try {
        result = arango.GET('/_admin/license');
        if (result.hasOwnProperty("error") && result.error && result.code === 404) {
          skip = true;
        }
      } catch (e) {
        console.warn(e);
        assertTrue(false);
      }
    },

    testGet: function () {
      if (!skip) {
        var result;
        try {
          result = arango.GET('/_admin/license');
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }

        let now = new Date();
        assertTrue(result.hasOwnProperty("status"));
        assertTrue(result.status === "expiring");
        assertTrue(result.hasOwnProperty("features"));
        assertTrue(result.features.hasOwnProperty("expires"));
        assertTrue(result.hasOwnProperty("status"));
        assertEqual(result.status, "expiring");
        assertTrue(!result.hasOwnProperty("hash"));
        let expires = new Date(result.features.expires * 1000);
        assertTrue(expires > now);

      }
    },

    testPutHelloWorld: function () {
      if (!skip) {
        var result;
        try {
          result = arango.PUT('/_admin/license', "Hello World");
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }
        assertTrue(result.error);
        assertEqual(result.code, 400);
        assertEqual(result.errorNum, 37);
        assertTrue(!result.hasOwnProperty("hash"));
        try {
          result = arango.GET('/_admin/license');
          assertEqual(result.version, 1);
          assertTrue(!result.hasOwnProperty("hash"));
          assertTrue(result.hasOwnProperty("status"));
          assertTrue(result.status === "expiring");
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }
      }
    },

    testPutHelloWorldString: function () {
      if (!skip) {
        var result;
        try {
          result = arango.PUT('/_admin/license', '"Hello World"');
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }
        assertTrue(result.error);
        assertEqual(result.code, 400);
        assertEqual(result.errorNum, 9001);
        assertTrue(!result.hasOwnProperty("hash"));
        try {
          result = arango.GET('/_admin/license');
          assertEqual(result.version, 1);
          assertTrue(!result.hasOwnProperty("hash"));
          assertTrue(result.hasOwnProperty("status"));
          assertTrue(result.status === "expiring");
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }
      }
    },

    testExpired: function () {
      var result;
      var license = "eyJncmFudCI6ImV5Sm1aV0YwZFhKbGN5STZleUpsZUhCcGNtVnpJam94TlRRd05EWXlNVFUwZlN3aWRtVnljMmx2YmlJNk1YMD0iLCJzaWduYXR1cmUiOiJEcVpKYlhEbWFDYUxlMmJDVGxyTWM4L3ZMb21vdU8raEJBbkFwMUkwZnlyOFUxQTJ6NDdGQytYSXlRK0d2WDdkWXpaYnBrUzBDTVU2VVBrdDVUd2lwUDM3NEVEazZ2S3l6VnIyZnVndm1DWXQ2VENwZjEyNEhNcEkwTE44dFlSZ01xbTVDMkt2RkhyaVhpTk9udFc5NTVWSzN4OXBDdjNwU2ZFbktDeW5nb2xyRVRJeDRXVzQ1YWthUjVOWEp0bzZiR3lybFp5RklVK1lhWXl2bGhocFRVeFdWUmQwKzV6UjQxdk4yWWRlWm50dVY5WDU0SVJRWkVVNEdpQmRaMHo1aGRZUERsMys5cTc0LzJ6WTI2VzlEdlNwcmZLYkZwOC95ejViV21IY1lEYWtLUHJkcXd6UnZaMDU1NXNWbDUwUjRvWUNPNGRuakxTeXhIbGdnczliSWc9PSJ9";
      try {
        result = arango.PUT('/_admin/license', JSON.stringify(license));
      } catch (e) {
        console.warn(e);
        assertTrue(false);
      }
      assertTrue(result.error);
      assertEqual(result.code, 400);
      assertEqual(result.errorNum, 9007);
      assertTrue(!result.hasOwnProperty("hash"));

      try {
        result = arango.GET('/_admin/license');
        assertEqual(result.version, 1);
        assertTrue(!result.hasOwnProperty("hash"));
        assertTrue(result.hasOwnProperty("status"));
        assertTrue(result.status === "expiring");
      } catch (e) {
        console.warn(e);
        assertTrue(false);
      }
    },

    testForceExpired: function () {
      var license = "eyJncmFudCI6ImV5Sm1aV0YwZFhKbGN5STZleUpsZUhCcGNtVnpJam94TlRRd05EWXlNVFUwZlN3aWRtVnljMmx2YmlJNk1YMD0iLCJzaWduYXR1cmUiOiJEcVpKYlhEbWFDYUxlMmJDVGxyTWM4L3ZMb21vdU8raEJBbkFwMUkwZnlyOFUxQTJ6NDdGQytYSXlRK0d2WDdkWXpaYnBrUzBDTVU2VVBrdDVUd2lwUDM3NEVEazZ2S3l6VnIyZnVndm1DWXQ2VENwZjEyNEhNcEkwTE44dFlSZ01xbTVDMkt2RkhyaVhpTk9udFc5NTVWSzN4OXBDdjNwU2ZFbktDeW5nb2xyRVRJeDRXVzQ1YWthUjVOWEp0bzZiR3lybFp5RklVK1lhWXl2bGhocFRVeFdWUmQwKzV6UjQxdk4yWWRlWm50dVY5WDU0SVJRWkVVNEdpQmRaMHo1aGRZUERsMys5cTc0LzJ6WTI2VzlEdlNwcmZLYkZwOC95ejViV21IY1lEYWtLUHJkcXd6UnZaMDU1NXNWbDUwUjRvWUNPNGRuakxTeXhIbGdnczliSWc9PSJ9";
      try {
        var result = arango.PUT('/_admin/license?force=true', JSON.stringify(license));
        assertTrue(!result.error);

        try {
          result = arango.GET('/_admin/license');
          assertEqual(result.version, 1);
          assertTrue(result.hasOwnProperty("hash"));
          assertEqual(result.hash, crypto.sha256(license));
          assertTrue(result.hasOwnProperty("status"));
          assertTrue(result.status === "read-only");
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }
        try {
          result = arango.GET('/_admin/server/mode');
          assertEqual(result.mode, "readonly");
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }
      } catch (e) {
        console.warn(e);
        assertTrue(false);
      }

    },

    testUnExpire: function () {
      var license = "eyJncmFudCI6ImV5Sm1aV0YwZFhKbGN5STZleUpsZUhCcGNtVnpJam94TnpJNU9EUTVNRFEzZlN3aWRtVnljMmx2YmlJNk1YMD0iLCJzaWduYXR1cmUiOiJ3SzBqSnpUNFVSdExUTGhGVzBkMy9RYm9OQzFrZnorM1ZqNGhwREFkd3VWL3c3VWF5QjJYMmtpNHVYd1MrM1ZJN1FmM25pNUh1VUlTMDIyVEs2UzZUTzdibDM0Q3c3WWVuSW43RDNEdTFHeVlkNk5Jay9ndEYrZFNVc1hRL1RxbWo2UWtsRW5XcDE5dzZLd1BVQ2NCWDFJRyt6NTMrd01kVzk5R1ZKWnV1ZkZmMzRzSnpxbEQ5WmM1UW12S0o5NFVVU2lnQlNlRW0wUWZvTWdKKzcrUmNRUVFMTVozTWhjOTMxL1U0dUJ2V0oyTEtQdUl1T1ZOMGhnK053V1ljQnk3RXNKc2o5cGRhNWdkM3dldm9TMmxIekZZQ3FzemQ0cmRwMlRIazJCbnY4aDRZTGxHeElOVjJnZExhNzVhdnBIS05ORkxSTGhNd3RpOWNGVWxwcUVTUnc9PSJ9";
      try {
        var result = arango.PUT('/_admin/license', JSON.stringify(license));
        assertTrue(!result.error);
        assertEqual(result.result.code, 201);

        try {
          result = arango.GET('/_admin/license');
          assertEqual(result.version, 1);
          assertTrue(result.hasOwnProperty("hash"));
          assertEqual(result.hash, crypto.sha256(license));
          assertTrue(result.hasOwnProperty("status"));
          assertTrue(result.status === "good");
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }

        try {
          result = arango.GET('/_admin/server/mode');
          assertEqual(result.mode, "default");
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }

      } catch (e) {
        console.warn(e);
      }
    },

  };
}

jsunity.run(adminLicenseSuite);

return jsunity.done();
