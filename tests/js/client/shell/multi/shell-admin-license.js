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

  };
}
jsunity.run(adminLicenseSuite);

return jsunity.done();
