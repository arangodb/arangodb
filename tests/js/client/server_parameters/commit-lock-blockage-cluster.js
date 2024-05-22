/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse, assertTrue, arango, print */

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
/// @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const isEnterprise = require("internal").isEnterprise();

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;

function AuthBlockageSuite() {
  'use strict';

  return {
    testBlockage: function () {
      if (!isEnterprise) {
        return;
      }
      let r = arango.POST_RAW("/_admin/backup/lock", {unlockTimeout:6000});
      assertEqual(200, r.code);
      let r2 = arango.POST_RAW("/_api/user", {"user":"hanswurst", "passwd":""});
      // Without the fix this will block for 6000 seconds
      assertEqual(201, r2.code);

      // And cleanup again:
      let r3 = arango.DELETE_RAW("/_api/user/hanswurst");
      assertEqual(202, r3.code);
    },
  };
}

//////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
//////////////////////////////////////////////////////////////////////////////

jsunity.run(AuthBlockageSuite);

return jsunity.done();
