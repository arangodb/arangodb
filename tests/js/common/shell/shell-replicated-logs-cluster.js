/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const ERRORS = arangodb.errors;

const waitForLeader = function (id) {
  while (true) {
    try {
      db._replicatedLog(id);
      break;
    } catch(err) {
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
      try {
        db._replicatedLog(logId);
        fail();
      } catch(err) {
        assertEqual(err.errorNum, ERRORS.ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND.code);
      }
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

    testAt : function() {
      const log = db._replicatedLog(logId);
      const index = log.insert({foo: "bar"}).index;
      let s = log.at(index);
      assertEqual(s.logIndex, index);
      assertEqual(s.payload.foo, "bar");
    },

    testRelease : function() {
      const log = db._replicatedLog(logId);
      for (let i = 0; i < 2000; i++) {
        log.insert({foo: i});
      }
      const s1 = log.status();
      assertEqual(s1.local.firstIndex, 1);
      log.release(1500);
      let s2 = log.status();
      assertEqual(s2.local.firstIndex, 1501);
    },

    testPoll : function() {
      let log = db._replicatedLog(logId);
      let index = 0;
      for (let i = 0; i < 100; i++) {
        let next = log.insert({foo: i}).index;
        assertTrue(next > index);
        index = next;
      }
      let s = log.poll(50, 10);
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
