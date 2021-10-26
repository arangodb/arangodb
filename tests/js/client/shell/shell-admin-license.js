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
const crypto = require('@arangodb/crypto');

function isEnterprise() {
  var result;
  try {
    result = arango.GET('/_admin/version');
  } catch (e) {
    console.warn(e);
    assertTrue(false);
  }
  return result.license === "enterprise";
}
function isCommunity() {
  return !isEnterprise();
}

function adminLicenseSuite () {
  'use strict';

  return {

    testGet: function () {
      if (isCommunity()) {
        var result;
        try {
          result = arango.GET('/_admin/license');
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }
        assertEqual(result, {license:"none"});
      }
    },

    testPut: function () {
      if (isCommunity()) {
        var result;
        try {
          result = arango.PUT('/_admin/license', "Hello World");
        } catch (e) {
          console.warn(e);
          assertTrue(false);
        }
        assertTrue(result.error);
        assertEqual(result.code, 501);
        assertEqual(result.errorNum, 31);
      }
    },

    testPost: function () {
      var result;
      try {
        result = arango.POST('/_admin/license', {});
      } catch (e) {
        console.warn(e);
        assertTrue(false);
      }
      assertTrue(result.error);
      assertEqual(result.code, 405);
      assertEqual(result.errorNum, 405);
    },

    testDelete: function () {
      var result;
      try {
        result = arango.DELETE('/_admin/license', {});
      } catch (e) {
        console.warn(e);
        assertTrue(false);
      }
      assertTrue(result.error);
      assertEqual(result.code, 405);
      assertEqual(result.errorNum, 405);
    },

    testPatch: function () {
      var result;
      try {
        result = arango.PATCH('/_admin/license', {});
      } catch (e) {
        console.warn(e);
        assertTrue(false);
      }
      assertTrue(result.error);
      assertEqual(result.code, 405);
      assertEqual(result.errorNum, 405);
    },

  };
}
jsunity.run(adminLicenseSuite);

return jsunity.done();
