/* jshint strict: false, sub: true */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

// Shared steps of the tests in tests/js/client/agency-restart. arangosh is
// connected to a single agent; agency operations go through the agencyMgr.

const internal = require('internal');
const db = internal.db;
const { assertEqual, assertFalse } = require('jsunity').jsUnity.assertions;

const clientId = 'testi';
// one agency transaction becomes one log entry; batching only saves round trips
const writeBatchSize = 1000;

function testRequest (i) {
  return { ['arango/test' + i]: { 'op': 'set', 'new': 'testmann' + i } };
}

function writeTestEntries (agencyMgr, count) {
  for (let start = 0; start < count; start += writeBatchSize) {
    let transactions = [];
    for (let i = start; i < Math.min(count, start + writeBatchSize); ++i) {
      transactions.push([testRequest(i), {}, clientId]);
    }
    agencyMgr.write(transactions);
  }
}

// a write with waitForSync flushes the RocksDB WAL, and with it all agency
// log entries written before, so they survive the following crash
function syncToDisk () {
  db._drop('UnitTestsRecovery');
  db._create('UnitTestsRecovery').insert({ _key: 'sync' }, { waitForSync: true });
}

function waitForCompactionsToSettle () {
  const compactionCount = () => db.compact.count();
  while (true) {
    let numCompactions = compactionCount();
    let start = internal.time();
    do {
      internal.wait(0.25);
      if (compactionCount() !== numCompactions) {
        break;
      }
    } while (internal.time() - start < 5);

    if (compactionCount() === numCompactions) {
      return;
    }
  }
}

// 0 if no compaction snapshot exists, like the agent's in-memory `current`
function latestCompactionIndex () {
  if (db._collection('compact') === null) {
    return 0;
  }
  const keys = db._query('FOR c IN compact SORT c._key DESC LIMIT 1 RETURN c._key').toArray();
  return keys.length === 0 ? 0 : Number(keys[0]);
}

// the persisted log up to the commit index, as served by /_api/agency/state
function persistedLog (agencyMgr) {
  const reply = agencyMgr.getAnyAgent(agencyMgr.agencyInstances[0], '/_api/agency/state', 'GET_RAW');
  assertEqual(200, reply.code, JSON.stringify(reply));
  return reply.parsedBody.log.map(entry => ({
    index: Number(entry._key),
    term: entry.term,
    query: entry.request,
    clientId: entry.clientId,
    timestamp: entry.epoch_millis
  }));
}

function assertContiguousLog (log) {
  let index = log[0].index;
  log.forEach(entry => {
    assertEqual('number', typeof entry.timestamp);
    assertEqual('number', typeof entry.term);
    assertEqual(index, entry.index);
    ++index;
  });
}

function testEntries (log) {
  return log.filter(entry => entry.clientId === clientId);
}

// test entries are written in order, so the key encodes the request number
function assertWellFormedTestEntry (entry) {
  const keys = Object.keys(entry.query);
  assertEqual(1, keys.length);
  const i = keys[0].substr('arango/test'.length);
  assertEqual(testRequest(i), entry.query);
}

function testValues (agencyMgr) {
  return agencyMgr.postAgency('read', [['/arango']])[0].arango || {};
}

function assertTestValuesPresent (agencyMgr, count) {
  const values = testValues(agencyMgr);
  for (let i = 0; i < count; ++i) {
    assertEqual('testmann' + i, values['test' + i]);
  }
}

// an agent missing its log or compact collection at startup wipes all data
function assertAgencyWiped (agencyMgr, count) {
  assertEqual(0, latestCompactionIndex());
  const log = persistedLog(agencyMgr);
  assertEqual(0, log[0].index);
  assertContiguousLog(log);
  assertEqual(0, testEntries(log).length);

  const values = testValues(agencyMgr);
  for (let i = 0; i < count; ++i) {
    assertFalse(values.hasOwnProperty('test' + i));
  }
}

exports.writeTestEntries = writeTestEntries;
exports.syncToDisk = syncToDisk;
exports.waitForCompactionsToSettle = waitForCompactionsToSettle;
exports.latestCompactionIndex = latestCompactionIndex;
exports.persistedLog = persistedLog;
exports.assertContiguousLog = assertContiguousLog;
exports.testEntries = testEntries;
exports.testRequest = testRequest;
exports.assertWellFormedTestEntry = assertWellFormedTestEntry;
exports.assertTestValuesPresent = assertTestValuesPresent;
exports.assertAgencyWiped = assertAgencyWiped;
