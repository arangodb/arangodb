/* jshint globalstrict:false, strict:false, unused: false */
/* global arango, assertEqual, assertTrue, ARGUMENTS, fail */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the sync method of the replication
// /
// / Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / DISCLAIMER
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const { deriveTestSuite } = require('@arangodb/test-helper');
const fs = require('fs');
const db = require('@arangodb').db;

const {
  BaseTestConfig,
  connectToLeader,
  connectToFollower,
  clearFailurePoints } = require(fs.join('tests', 'js', 'client', 'replication', 'sync', 'replication-sync-malarkey.inc'));

const cn = 'UnitTestsReplication';

function ReplicationIncrementalMalarkeyNewFormat() {
  'use strict';

  let suite = {
    setUp: function () {
      connectToFollower();
      // clear all failure points
      clearFailurePoints();
      db._drop(cn);

      connectToLeader();
      // clear all failure points
      clearFailurePoints();
      db._drop(cn);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_NewFormat');
  return suite;
}

let res = arango.GET("/_admin/debug/failat");
if (res === true) {
  // tests only work when compiled with -DUSE_FAILURE_TESTS
  jsunity.run(ReplicationIncrementalMalarkeyNewFormat);
}

return jsunity.done();
