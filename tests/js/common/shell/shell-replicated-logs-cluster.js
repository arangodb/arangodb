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
      let status = db._replicatedLog(id).status();
      if ("logStatus" in status && status.logStatus.role === "leader") {
        break;
      }
    } catch (err) {
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
  const targetConfig = {
    replicationFactor: 3,
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: true
  };

  return {
    setUp : function () {},
    tearDown : function () {},

    testCreateAndDropReplicatedLog : function() {
      const log = db._createReplicatedLog({id: logId, targetConfig: targetConfig});
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
  const targetConfig = {
    replicationFactor: 3,
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: true
  };

  return {
    setUp : function () {
      db._createReplicatedLog({id: logId, targetConfig: targetConfig});
      waitForLeader(logId);
    },
    tearDown : function () {
      db._replicatedLog(logId).drop();
    },

    testInsert : function() {
      let log = db._replicatedLog(logId);
      let index = 0;
      for (let i = 0; i < 100; i++) {
        let result = log.insert({foo: i});
        assertTrue('quorum' in result.result &&
            'commitIndex' in result.result);
        let next = result.index;
        assertTrue(next > index);
        index = next;
      }
      let status = log.status();
      assertTrue(status.logStatus.local.commitIndex >= index);
    },

    testMultiInsert : function() {
      let log = db._replicatedLog(logId);
      let index = 0;
      for (let i = 0; i < 100; i += 3) {
        let result = log.multiInsert([{foo0: i}, {foo1: i + 1}, {foo2: i + 2}]);
        assertTrue('quorum' in result.result &&
            'commitIndex' in result.result);
        let indexes = result.indexes;
        assertTrue(Array.isArray(indexes));
        assertTrue(indexes.length === 3);
        assertTrue(index < indexes[0] &&
            indexes[0] < indexes[1] &&
            indexes[1] < indexes[2]);
        index = indexes[indexes.length - 1];
      }
      let status = log.status();
      assertTrue(status.logStatus.local.commitIndex >= index);
    },

    testHeadTail : function() {
      let log = db._replicatedLog(logId);
      let index = 0;
      for (let i = 0; i < 100; i++) {
        let next = log.insert({foo: i}).index;
        assertTrue(next > index);
        index = next;
      }
      let head = log.head();
      assertEqual(head.length, 10);
      assertEqual(head[9].logIndex, 10);
      head = log.head(11);  // skip first entry
      assertEqual(head.length, 11);
      for (let i = 0; i < 10; i++) {
        assertEqual(head[i+1].payload.foo, i);
      }
      let tail = log.tail();
      assertEqual(tail.length, 10);
      assertEqual(tail[9].logIndex, 101);
      tail = log.tail(10);
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
      let s = log.slice();
      assertEqual(s.length, 10);
      assertEqual(s[0].logIndex, 1);
      assertEqual(s[9].logIndex, 10);
      s = log.slice(50, 60);
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
      assertEqual(s1.logStatus.local.firstIndex, 1);
      log.release(1500);
      let s2 = log.status();
      assertEqual(s2.logStatus.local.firstIndex, 1501);
    },

    testPoll : function() {
      let log = db._replicatedLog(logId);
      let index = 0;
      for (let i = 0; i < 100; i++) {
        let next = log.insert({foo: i}).index;
        assertTrue(next > index);
        index = next;
      }
      let s = log.poll();
      assertEqual(s.length, 9);
      assertEqual(s[0].logIndex, 1);
      assertEqual(s[8].logIndex, 9);
      s = log.poll(50, 10);
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
