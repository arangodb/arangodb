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

function SynchronousReplicationFailFollowerSuite () {
  'use strict';
  const suite = internal.load(fs.join(internal.pathForTesting('client'), 'resilience', 'failover', 'resilience-synchronous-repl-cluster.inc'));

  return {
    setUp : suite.setUp,
    tearDown : suite.tearDown,

////////////////////////////////////////////////////////////////////////////////
/// @brief run a standard check with failures:
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFailureFollower : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.failFollower();
      suite.runBasicOperations({}, {});
      suite.healFollower();
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 1
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail1 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:1, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 2
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail2 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:2, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 3
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail3 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:3, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 4
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail4 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:4, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 5
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail5 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:5, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 6
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail6 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:6, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 7
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail7 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:7, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 8
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail8 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:8, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 9
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail9 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:9, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 10
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail10 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:10, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 11
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail11 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:11, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 12
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail12 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:12, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 13
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail13 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:13, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 14
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail14 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:14, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 15
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail15 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:15, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 16
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail16 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:16, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 17
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail17 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:17, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 18
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail18 : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({place:18, follower:true}, {place:18, follower:true});
      assertTrue(suite.waitForSynchronousReplication("_system"));
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SynchronousReplicationFailFollowerSuite);

return jsunity.done();

