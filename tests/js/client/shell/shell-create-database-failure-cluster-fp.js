/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let errors = arangodb.errors;
let { getEndpointById,
      getEndpointsByType,
    } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function createDatabaseFailureSuite() {
  'use strict';
  const cn = 'UnitTestsCreateDatabase';

  return {

    setUp: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      try {
        db._dropDatabase(cn);
      } catch (err) {
      }
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
    },
    
    // make follower execute intermediate commits (before the leader), but let the
    // transaction succeed
    testCreateDatabaseWithFailure: function () {
      IM.debugSetFailAt("CreateDatabase::first", instanceRole.dbServer);

      try {
        db._createDatabase(cn);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE.code);
      }
    },
    
  };
}

jsunity.run(createDatabaseFailureSuite);
return jsunity.done();
