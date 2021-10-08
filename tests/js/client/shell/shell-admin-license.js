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
      }
      print(result);
    },

    testGet: function () {
      if (!skip) {
        var result;
        try {
          result = arango.GET('/_admin/license');
        } catch (e) {
          console.warn(e);
        }
        let now = new Date();
        assertTrue(result.hasOwnProperty("status"));
        assertTrue(result.status === "expiring");
        assertTrue(result.hasOwnProperty("features"));
        assertTrue(result.features.hasOwnProperty("expires"));
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
        }
        assertTrue(result.error);
        assertEqual(result.code, 400);
        assertEqual(result.errorNum, 37);
      }
    },

    testPutHelloWorldString: function () {
      if (!skip) {
        var result;
        try {
          result = arango.PUT('/_admin/license', '"Hello World"');
        } catch (e) {
          console.warn(e);
        }
        print(result);
        assertTrue(result.error);
        assertEqual(result.code, 400);
        assertEqual(result.errorNum, 9001);
      }
    },

  };
}

jsunity.run(adminLicenseSuite);

return jsunity.done();
