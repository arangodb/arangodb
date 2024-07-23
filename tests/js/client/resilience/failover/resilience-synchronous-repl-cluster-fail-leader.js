/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, fail, instanceInfo */

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
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const fs = require('fs');
const internal = require('internal');

function SynchronousReplicationFailLeaderSuite () {
  'use strict';
  const suite = internal.load(fs.join(internal.pathForTesting('client'), 'resilience', 'failover', 'resilience-synchronous-repl-cluster.inc'));

  return {
    setUp : suite.setUp,
    tearDown : suite.tearDown,

////////////////////////////////////////////////////////////////////////////////
/// @brief run a standard check with failures:
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFailureLeader : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.failLeader();
      suite.runBasicOperations({}, {});
      suite.healLeader();
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 1
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail1 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:1, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 2
////////////////////////////////////////////////////////////////////////////////
    testBasicOperationsLeaderFail2 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:2, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 3
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail3 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:3, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 4
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail4 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:4, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 5
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail5 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:5, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 6
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail6 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:6, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 7
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail7 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:7, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 8
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail8 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:8, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 9
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail9 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:9, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 10
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail10 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:10, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 11
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail11 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:11, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 12
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail12 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:12, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 13
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail13 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:13, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 14
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail14 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:14, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 15
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail15 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:15, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 16
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail16 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:16, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 17
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail17 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:17, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 18
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail18 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:18, follower: false},
                         {place:18, follower: false});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SynchronousReplicationFailLeaderSuite);

return jsunity.done();

