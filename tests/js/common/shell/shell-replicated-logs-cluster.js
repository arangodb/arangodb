/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test ttl configuration
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const print = internal.print;
const db = arangodb.db;
const ERRORS = arangodb.errors;

const waitForLeader = function (id) {
  while (true) {
    try {
      db._replicatedLog(id);
      break;
    } catch(err) {
      print("waitForLeader error:", err);
      if (err.errorNum !== ERRORS.ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED.code) {
        throw err;
      }
    }

    internal.sleep(1);
  }
};

function ReplicatedLogsSuite () {
  'use strict';

  const logId = 12;

  return {
    setUp : function () {},
    tearDown : function () {},

    testCreateAndDropReplicatedLog : function() {
      const log = db._createReplicatedLog({id: logId, targetConfig: {replicationFactor: 3, writeConcern: 2, waitForSync: true}});
      assertEqual(log.id(), logId);
      waitForLeader(logId);
      assertEqual(db._replicatedLog(logId).id(), logId);
      log.drop();
    }
  };
}

function ReplicatedLogsWriteSuite () {
  'use strict';

  const logId = 12;

  return {
    setUp : function () {
      db._createReplicatedLog({id: logId, targetConfig: {replicationFactor: 3, writeConcern: 2, waitForSync: true}});
      waitForLeader(logId);
    },
    tearDown : function () {
      db._replicatedLog(logId).drop();
    },

    testInsert : function() {
      let log = db._replicatedLog(logId);
      let index = 0;
      for (let i = 0; i < 100; i++) {
        let next = log.insert({foo: i}).index;
        assertTrue(next > index);
        index = next;
      }
      let status = log.status();
      assertTrue(status.local.commitIndex >= index);
    },

    testHeadTail : function() {
      let log = db._replicatedLog(logId);
      let index = 0;
      for (let i = 0; i < 100; i++) {
        let next = log.insert({foo: i}).index;
        assertTrue(next > index);
        index = next;
      }
      let head = log.head(11);  // skip first entry
      assertEqual(head.length, 11);
      for (let i = 0; i < 10; i++) {
        assertEqual(head[i+1].payload.foo, i);
      }
      let tail = log.tail(10);
      assertEqual(tail.length, 10);
      for (let i = 0; i < 10; i++) {
        assertEqual(tail[i].payload.foo, 100 + i - 10);
      }
    },

    testSlice : function() {
      let log = db._replicatedLog(logId);
      let index = 0;
      for (let i = 0; i < 100; i++) {
        let next = log.insert({foo: i}).index;
        assertTrue(next > index);
        index = next;
      }
      let s = log.slice(50, 60);
      assertEqual(s.length, 10);
      for (let i = 0; i < 10; i++) {
        assertEqual(s[i].logIndex, 50 + i);
      }
    },
  };
}

jsunity.run(ReplicatedLogsSuite);
jsunity.run(ReplicatedLogsWriteSuite);

return jsunity.done();
