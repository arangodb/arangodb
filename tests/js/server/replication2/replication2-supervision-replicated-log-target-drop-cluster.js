/*jshint strict: true */
/*global assertTrue, assertEqual*/
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");

const database = "replication2_supervision_test_drop_db";

const {setUpAll, tearDownAll, setUp, tearDown} = lh.testHelperFunctions(database);

const replicatedLogSuite = function () {

  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    replicationFactor: 3,
    waitForSync: false,
  };

  return {
    setUpAll, tearDownAll,
    setUp, tearDown,

    // This test create a replicated log in target and then drops it again
    // we expect Plan and Current to be deleted
    testDropFromTarget: function () {
      const {logId} = lh.createReplicatedLog(database, targetConfig);
      lh.waitForReplicatedLogAvailable(logId);
      lh.replicatedLogDeleteTarget(database, logId);

      lh.waitFor(lpreds.replicatedLogIsGone(database, logId));
    },
  };
};

jsunity.run(replicatedLogSuite);
return jsunity.done();
