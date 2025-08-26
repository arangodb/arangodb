/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual, assertFalse, assertTrue, GLOBAL */

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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let internal = require('internal');
let db = arangodb.db;
let { agency } = require('@arangodb/test-helper');
const IM = GLOBAL.instanceManager;
  
const cn = 'UnitTestsCollection';
  
function DropDatabaseWithFailedSuite() {
  'use strict';

  return {
    setUp: function () {
      let d = db._createDatabase("d");
    },

    tearDown: function () {
      // Normally, database is already dropped, so this might fail:
      try {
        db._dropDatabase("d");
      } catch (err) {
      }
    },
    
    testDropDatabaseWithFailedServer: function () {
      const currentKey = `/arango/Current/Databases/d/nonExistingServer`;
      let currentValue = { name:"nonExistingServer", id:"4711",
                        error: false, errorMessage: "", errorNum: 0 };
      let body = {"/arango/Current/Version":{"op":"increment"}};
      body[currentKey] = {"op":"set", "new": currentValue};
      IM.agencyMgr.call("write", [[body]]);

      internal.wait(5);   // Ensure that all coordinators have seen the value!
  
      let start = new Date();
      // Drop database: Should work:
      db._dropDatabase("d");
      let duration = new Date() - start;
      assertTrue(duration < 15000, "drop database needed " << duration << " ms to complete, too long!");
    },
      
  };
}

jsunity.run(DropDatabaseWithFailedSuite);
return jsunity.done();
