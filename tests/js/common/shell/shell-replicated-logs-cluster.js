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
const _ = require('lodash');
const db = arangodb.db;
const ERRORS = arangodb.errors;
const database = "replication2_test_db";

const getLeaderStatus = function (log) {
  let status = log.status();
  const leaderId = status.leaderId;
  if (leaderId === undefined) {
    console.error(`leader not available for replicated log ${log.id()}`);
    console.error(JSON.stringify(status));
    return null;
  }
  if (status.participants === undefined || status.participants[leaderId] === undefined) {
    console.error(`participants status not available for replicated log ${log.id()}`);
    console.error(JSON.stringify(status));
    return null;
  }
  const leaderResponse = status.participants[leaderId].response;
  if (leaderResponse === undefined || leaderResponse.role !== "leader") {
    console.error(`leader not available for replicated log ${log.id()}`);
    console.error(JSON.stringify(status));
    return null;
  }
  if (!leaderResponse.leadershipEstablished) {
    console.error(`leadership not yet established`);
    console.error(JSON.stringify(status));
    return null;
  }
  return status.participants[leaderId].response;
};

const {setUpAll, tearDownAll} = (function () {
  let previousDatabase, databaseExisted = true;
  return {
    setUpAll: function () {
      previousDatabase = db._name();
      if (!_.includes(db._databases(), database)) {
        db._createDatabase(database);
        databaseExisted = false;
      }
      db._useDatabase(database);
    },

    tearDownAll: function () {
      db._useDatabase(previousDatabase);
      if (!databaseExisted) {
        db._dropDatabase(database);
      }
    },
  };
}());

function ReplicatedLogsCreateSuite() {
  'use strict';
  const config = {
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: true
  };

  return {
    setUpAll, tearDownAll,

    testCreateAndDropReplicatedLog: function () {
      const logId = 12;
      const log = db._createReplicatedLog({id: logId, config});

      assertEqual(log.id(), logId);
      assertEqual(db._replicatedLog(logId).id(), logId);
      log.drop();
      try {
        db._replicatedLog(logId);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND.code);
      }
    },

    testCreateNoId: function () {
      const log = db._createReplicatedLog({config});
      assertEqual(log.id(), db._replicatedLog(log.id()).id());
    },
  };
}

function ReplicatedLogsWriteSuite() {
  'use strict';

  const config = {
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: true
  };

  let log = null;

  return {
    setUpAll, tearDownAll,
    setUp: function () {
      log = db._createReplicatedLog({config});
    },
    tearDown: function () {
      db._replicatedLog(log.id()).drop();
    },

    testStatus: function () {
      let leaderStatus = getLeaderStatus(log);
      assertEqual(leaderStatus.local.commitIndex, 1, `log ${log.id()}, leader status: ${JSON.stringify(leaderStatus)}`);
      let globalStatus = log.globalStatus();
      assertEqual(globalStatus.specification.source, "RemoteAgency",
          `log ${log.id()}, global status: ${JSON.stringify(globalStatus)}`);
      let status = log.status();

      // Make sure that coordinator/status and coordinator/global-status return the same thing.
      // We should avoid comparing timestamps, so comparing only the keys of these objects would suffice.
      assertEqual(Object.keys(status).sort(), Object.keys(globalStatus).sort(),
          `coordinator/status: ${JSON.stringify(status)}\ncoordinator/global-status: ${JSON.stringify(globalStatus)}`);

      let localGlobalStatus = log.globalStatus({useLocalCache: true});
      assertEqual(localGlobalStatus.specification.source, "LocalCache",
          `log ${log.id()}, global-status: ${JSON.stringify(localGlobalStatus)}`);
    },

    testInsert: function () {
      let index = 0;
      for (let i = 0; i < 100; i++) {
        let result = log.insert({foo: i});
        assertTrue('quorum' in result.result &&
            'commitIndex' in result.result);
        let next = result.index;
        assertTrue(next > index);
        index = next;
      }
      let leaderStatus = getLeaderStatus(log);
      assertTrue(leaderStatus.local.commitIndex >= index);
    },

    testPing: function () {
      let result = log.ping("foo bar");
      assertTrue('quorum' in result.result &&
          'commitIndex' in result.result);
      const tail = log.tail();
      assertEqual(tail[1].meta.type, "Ping");
      assertEqual(tail[1].meta.message, "foo bar");
      assertTrue(tail[1].meta.time !== undefined);
    },

    testMultiInsert: function () {
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
      let leaderStatus = getLeaderStatus(log);
      assertTrue(leaderStatus.local.commitIndex >= index);
    },

    testHeadTail: function () {
      try {
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
          assertEqual(head[i + 1].payload.foo, i);
        }
        let tail = log.tail();
        assertEqual(tail.length, 10);
        assertEqual(tail[9].logIndex, 101); // !here (see below)
        tail = log.tail(10);
        assertEqual(tail.length, 10);
        for (let i = 0; i < 10; i++) {
          assertEqual(tail[i].payload.foo, 100 + i - 10);
        }
      } catch (e) {
        // This catch is temporary, to debug the following failure we've seen in Jenkins:
        //   "testHeadTail" failed: Error: at assertion #115: assertEqual: (101) is not equal to (102)
        // which happens at the line marked !here above.
        var console = require('console');
        console.warn({
          status: log.status(),
          head: log.head(200),
        });
        throw e;
      }
    },

    testSlice: function () {
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

    testAt: function () {
      const index = log.insert({foo: "bar"}).index;
      let s = log.at(index);
      assertEqual(s.logIndex, index);
      assertEqual(s.payload.foo, "bar");
    },

    testRelease: function () {
      const payloads = [...Array(2000).keys()].map(i => ({foo: i}));
      for (const batch of _.chunk(payloads, 1000)) {
        log.multiInsert(batch);
      }
      const s1 = getLeaderStatus(log);
      assertEqual(s1.local.firstIndex, 1);
      assertEqual(s1.local.spearhead.index, 2001);
      assertEqual(s1.local.commitIndex, 2001);

      log.release(1500);

      const s2 = getLeaderStatus(log);
      // Compaction runs asynchronously, so we can not expect the firstIndex to be 1500 as well (yet)
      assertEqual(s2.local.releaseIndex, 1500);
    },

   testPoll: function () {
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

function ReplicatedLogsCompactSuite() {
  'use strict';

  const config = {
    writeConcern: 3,
    softWriteConcern: 3,
    waitForSync: true
  };

  let log = null;

  return {
    setUpAll, tearDownAll,
    setUp: function () {
      log = db._createReplicatedLog({config});
    },
    tearDown: function () {
      db._replicatedLog(log.id()).drop();
    },

    testCompact: function () {
      // It is important the writeConcern is set to maximum, otherwise this test might fail.
      for (let i = 0; i < 100; i++) {
        log.insert({foo: i});
      }
      const s1 = getLeaderStatus(log);
      assertEqual(s1.local.firstIndex, 1, JSON.stringify(s1));
      log.release(100);
      const s2 = getLeaderStatus(log);
      assertEqual(s2.local.firstIndex, 1, JSON.stringify(s2));
      log.compact();
      const s3 = getLeaderStatus(log);
      assertEqual(s3.local.firstIndex, 100, JSON.stringify(s3));
    }
  };
}

jsunity.run(ReplicatedLogsCreateSuite);
jsunity.run(ReplicatedLogsWriteSuite);
jsunity.run(ReplicatedLogsCompactSuite);

return jsunity.done();
