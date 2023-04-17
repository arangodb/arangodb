/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, assertNotNull, fail, instanceInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief test synchronous replication in the cluster
///
/// DISCLAIMER
///
/// Copyright 2016-2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const fs = require('fs');
const internal = require('internal');

/*
 *  This test has been disabled because of https://arangodb.atlassian.net/browse/BTS-1312.
 */

function SynchronousReplicationFailLeaderSuite() {
  'use strict';
  const suite = internal.load(fs.join(internal.pathForTesting('server'), 'resilience', 'failover', 'resilience-synchronous-repl-cluster.inc'));
  let c;
  return {
    setUp: function () {
      suite.setUp();
      c = suite.getCollection();
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },
    tearDown: function () {
      suite.healLeader();
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.tearDown();
    },

    testBasicOperationFailureLeader: function () {
      suite.failLeader();
      try {
        const doc = c.insert({Hallo: 13});
        assertEqual(1, c.count());
        assertNotNull(doc._id);
      } catch (e) {
        // either all documents have been inserted or none
        const count = c.count();
        assertTrue(count === 1 || count === 0, "actual count is " + count);
      }
    },

    testBasicOperationsFailureLeader: function () {
      suite.failLeader();
      try {
        const ids = c.insert([{Hallo: 13}, {Hallo: 14}]);
        assertEqual(2, c.count());
        assertEqual(2, ids.length);
      } catch (e) {
        // either all documents have been inserted or none
        const count = c.count();
        assertEqual(c.properties().numberOfShards, 1); // otherwise this is not guaranteed
        assertTrue(count === 2 || count === 0, "actual count is " + count);
      }
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SynchronousReplicationFailLeaderSuite);

return jsunity.done();
