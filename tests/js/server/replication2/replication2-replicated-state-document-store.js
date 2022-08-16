/*jshint strict: true */
/*global assertTrue, assertEqual, assertNotNull, print*/
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const _ = require('lodash');
const db = arangodb.db;
const helper = require('@arangodb/test-helper');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const lp = require("@arangodb/testutils/replicated-logs-predicates");
const lhttp = require('@arangodb/testutils/replicated-logs-http-helper');
const sh = require("@arangodb/testutils/replicated-state-helper");
const sp = require("@arangodb/testutils/replicated-state-predicates");

const database = "replication2_document_store_test_db";
const collectionName = "testCollection";

function makeTestSuites(testSuite) {
  let suiteV1 = {};
  let suiteV2 = {};
  helper.deriveTestSuite(testSuite({}), suiteV1, "_V1");
  helper.deriveTestSuite(testSuite({replicationVersion: "2"}), suiteV2, "_V2");
  return [suiteV1, suiteV2];
}

/**
 * TODO this function is here temporarily and is will be removed once we have a better solution.
 * Its purpose is to synchronize the participants of replicated logs with the participants of their respective shards.
 * This is needed because we're using the list of participants from two places.
 */
const syncShardsWithLogs = function(dbn) {
  const coordinator = lh.coordinators[0];
  let logs = lhttp.listLogs(coordinator, dbn).result;
  let collections = lh.readAgencyValueAt(`Plan/Collections/${dbn}`);
  for (const [colId, colInfo] of Object.entries(collections)) {
    for (const shardId of Object.keys(colInfo.shards)) {
      const logId = shardId.slice(1);
      if (logId in logs) {
        helper.agency.set(`Plan/Collections/${dbn}/${colId}/shards/${shardId}`, logs[logId]);
      }
    }
  }

  const waitForCurrent  = lh.readAgencyValueAt("Current/Version");
  helper.agency.increaseVersion(`Plan/Version`);

  lh.waitFor(() => {
    const currentVersion  = lh.readAgencyValueAt("Current/Version");
    if (currentVersion > waitForCurrent) {
      return true;
    }
    return Error(`Current/Version expected to be greater than ${waitForCurrent}, but got ${currentVersion}`);
  }, 30, (e) => {
    // We ignore this and continue. Most probably current was increased before we could observe it.
    print(e.message);
  });
};

/**
 * Checks if a given key exists (or not) on all followers.
 */
const checkFollowersValue = function (servers, shardId, key, value, isReplication2) {
  let localValues = {};
  for (const [serverId, endpoint] of Object.entries(servers)) {
    if (value === null) {
      // Check for absence of key
      lh.waitFor(sp.localKeyStatus(endpoint, database, shardId, key, false));
    } else {
      // Check for key and value
      lh.waitFor(sp.localKeyStatus(endpoint, database, shardId, key, true, value));
    }
    localValues[serverId] = sh.getLocalValue(endpoint, database, shardId, key);
  }

  let replication2Log = '';
  if (isReplication2) {
    replication2Log = `Log entries: ${JSON.stringify(lh.dumpShardLog(shardId))}`;
  }
  let extraErrorMessage = `All responses: ${JSON.stringify(localValues)}` + `\n${replication2Log}`;

  for (const [serverId, res] of Object.entries(localValues)) {
    if (value === null) {
      assertTrue(res.code === 404,
          `Expected 404 while reading key from ${serverId}/${database}/${shardId}, ` +
          `but the response was ${JSON.stringify(res)}.\n` + extraErrorMessage);
    } else {
      assertTrue(res.code === undefined,
          `Error while reading key from ${serverId}/${database}/${shardId}/${key}, ` +
          `got: ${JSON.stringify(res)}.\n` + extraErrorMessage);
      assertEqual(res.value, value,
          `Wrong value returned by ${serverId}/${database}/${shardId}, expected ${value} but ` +
          `got: ${JSON.stringify(res)}. ` + extraErrorMessage);
    }
  }

  if (value !== null) {
    // All ids and revisions should be equal
    const revs = Object.values(localValues).map(value => value._rev);
    assertTrue(revs.every((val, i, arr) => val === arr[0]), `_rev mismatch ${JSON.stringify(localValues)}` +
      `\n${replication2Log}`);

    const ids = Object.values(localValues).map(value => value._id);
    assertTrue(ids.every((val, i, arr) => val === arr[0]), `_id mismatch ${JSON.stringify(localValues)}` +
      `\n${replication2Log}`);
  }
};

/**
 * Returns first entry with the same key and type as the document provided.
 * If no document is provided, all entries of the specified type are returned.
 */
const getDocumentEntries = function (entries, type, document) {
  if (document === undefined) {
    let matchingType = [];
    for (const entry of entries) {
      if (entry.hasOwnProperty("payload") && entry.payload[1].operation === type) {
        matchingType.push(entry);
      }
    }
    return matchingType;
  }
  for (const entry of entries) {
    if (entry.hasOwnProperty("payload") && entry.payload[1].operation === type) {
      // replication entries can contain an array of documents (batch op)
      if (Array.isArray(entry.payload[1].data)) {
        // in this case try to find the document in the batch
        let res = entry.payload[1].data.filter((doc) => doc._key === document._key);
        if (res.length === 1) {
          return entry;
        }
      } else if (entry.payload[1].data._key === document._key) {
        // single document operation was replicated
        return entry;
      }
    }
  }
  return null;
};

const mergeLogs = function(logs) {
  return logs.reduce((previous, current) => previous.concat(current.head(1000)), []);
};

/**
 * Check if all the documents are in the logs and have the provided type.
 */
const searchDocs = function(logs, docs, opType) {
  let allEntries = mergeLogs(logs);
  for (const doc of docs) {
    let entry = getDocumentEntries(allEntries, opType, doc);
    assertNotNull(entry);
    assertEqual(entry.payload[1].operation, opType, `Dumping combined log entries: ${JSON.stringify(allEntries)}`);
  }
};

/**
 * Unroll all array entries from all logs and optionally filter by name.
 */
const getArrayElements = function(logs, opType, name) {
  let entries = logs.reduce((previous, current) => previous.concat(current.head(1000)), [])
      .filter(entry => entry.hasOwnProperty("payload") && entry.payload[1].operation === opType
          && Array.isArray(entry.payload[1].data))
      .reduce((previous, current) => previous.concat(current.payload[1].data), []);
  if (name === undefined) {
    return entries;
  }
  return entries.filter(entry => entry.name === name);
};

const replicatedStateDocumentStoreSuiteReplication2 = function () {
  let collection = null;
  let shards = null;
  let logs = null;

  const {setUpAll, tearDownAll, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      collection = db._create(collectionName, {"numberOfShards": 2, "writeConcern": 2, "replicationFactor": 3});
      shards = collection.shards();
      logs = shards.map(shardId => db._replicatedLog(shardId.slice(1)));
    }),
    tearDown: tearDownAnd(() => {
      if (collection !== null) {
        collection.drop();
      }
      collection = null;
    }),

    testCreateReplicatedStateForEachShard: function() {
      let colPlan = lh.readAgencyValueAt(`Plan/Collections/${database}/${collection._id}`);
      let colCurrent = lh.readAgencyValueAt(`Current/Collections/${database}/${collection._id}`);

      for (const shard of collection.shards()) {
        let {target, plan, current} = sh.readReplicatedStateAgency(database, lh.shardIdToLogId(shard));
        let shardPlan = colPlan.shards[shard];
        let shardCurrent = colCurrent[shard];

        // Check if the replicated state in Target and the shard in Plan match
        assertEqual(target.leader, shardPlan[0]);
        assertTrue(lh.sortedArrayEqualOrError(Object.keys(target.participants)), _.sortBy(shardPlan));

        // Check if the replicated state in Plan and the shard in Plan match
        assertTrue(lh.sortedArrayEqualOrError(Object.keys(plan.participants)), _.sortBy(shardPlan));

        // Check if the replicated state in Current and the shard in Current match
        assertEqual(target.leader, shardCurrent.servers[0]);
        assertTrue(lh.sortedArrayEqualOrError(Object.keys(current.participants)), _.sortBy(shardCurrent.servers));
      }
    },

    testDropCollection: function() {
      db._drop(collectionName);
      collection = null;
      for (const shard of shards) {
        let {plan} = sh.readReplicatedStateAgency(database, lh.shardIdToLogId(shard));
        assertEqual(plan, undefined);
      }
    },

    testReplicateOperationsCommit: function() {
      const opType = "Commit";

      collection.insert({_key: "abcd"});
      let commitEntries = getDocumentEntries(mergeLogs(logs), opType);
      let insertEntries = getDocumentEntries(mergeLogs(logs), "Insert");
      assertEqual(commitEntries.length, 1,
          `Found more commitEntries than expected: ${commitEntries}. Insert entries: ${insertEntries}`);
      assertEqual(insertEntries.length, commitEntries.length,
          `Insert entries: ${insertEntries} do not match Commit entries ${commitEntries}`);
      assertEqual(insertEntries[0].trx, commitEntries[0].trx,
          `Insert entries: ${insertEntries} do not match Commit entries ${commitEntries}`);
    },

    testReplicateOperationsInsert: function() {
      const opType = "Insert";

      // Insert single document
      let documents = [{_key: "foo"}, {_key: "bar"}];
      documents.forEach(doc => collection.insert(doc));
      searchDocs(logs, documents, opType);

      // Insert multiple documents
      documents = [...Array(10).keys()].map(i => {return {name: "testInsert1", foobar: i};});
      collection.insert(documents);
      let result = getArrayElements(logs, opType, "testInsert1");
      for (const doc of documents) {
        assertTrue(result.find(entry => entry.foobar === doc.foobar) !== undefined);
      }

      // AQL INSERT
      documents = [...Array(10).keys()].map(i => {return {name: "testInsert2", baz: i};});
      db._query(`FOR i in 0..9 INSERT {_key: CONCAT('test', i), name: "testInsert2", baz: i} INTO ${collectionName}`);
      result = getArrayElements(logs, opType, "testInsert2");
      for (const doc of documents) {
        assertTrue(result.find(entry => entry.baz === doc.baz) !== undefined);
      }
    },

    testReplicateOperationsModify: function() {
      const opType = "Update";

      // Update single document
      let documents = [{_key: `test${_.random(1000)}`}, {_key: `test${_.random(1000)}`}];
      let docHandles = [];
      documents.forEach(doc => {
        let docUpdate = {_key: doc._key, name: `updatedTest${doc.value}`};
        let d = collection.insert(doc);
        docHandles.push(collection.update(d, docUpdate));
        searchDocs(logs, [docUpdate], opType);
      });

      // Replace multiple documents
      let replacements = [{value: 10, name: "testR"}, {value: 20, name: "testR"}];
      docHandles = collection.replace(docHandles, replacements);
      let result = getArrayElements(logs, "Replace",  "testR");
      for (const doc of replacements) {
        assertTrue(result.find(entry => entry.value === doc.value) !== undefined);
      }

      // Update multiple documents
      let updates = documents.map(_ => {return {name: "testModify10"};});
      docHandles = collection.update(docHandles, updates);
      result = getArrayElements(logs, opType, "testModify10");
      assertEqual(result.length, updates.length);

      // AQL UPDATE
      db._query(`FOR doc IN ${collectionName} UPDATE {_key: doc._key, name: "testModify100"} IN ${collectionName}`);
      result = getArrayElements(logs, opType, "testModify100");
      assertEqual(result.length, updates.length);
    },

    testReplicateOperationsRemove: function() {
      const opType = "Remove";

      // Remove single document
      let doc = {_key: `test${_.random(1000)}`};
      let d = collection.insert(doc);
      collection.remove(d);
      searchDocs(logs, [doc], opType);

      // AQL REMOVE
      let documents = [
        {_key: `test${_.random(1000)}`}, {_key: `test${_.random(1000)}`}
      ];
      collection.insert(documents);
      db._query(`FOR doc IN ${collectionName} REMOVE doc IN ${collectionName}`);
      let result = getArrayElements(logs, opType);
      for (const doc of documents) {
        assertTrue(result.find(entry => entry._key === doc._key) !== undefined);
      }
    },

    testReplicateOperationsTruncate: function() {
      const opType = "Truncate";
      collection.truncate();

      let found = [];
      let allEntries = logs.reduce((previous, current) => previous.concat(current.head(1000)), []);
      for (const entry of allEntries) {
        if (entry.hasOwnProperty("payload") && entry.payload[1].operation === opType) {
          let colName = entry.payload[1].data.collection;
          assertTrue(shards.includes(colName) && !found.includes(colName));
          found.push(colName);
        }
      }
      assertEqual(found.length, 2, `Dumping combined log entries: ${JSON.stringify(allEntries)}`);
    },
  };
};

/**
 * This test suite checks the correctness of replicated operations with respect to the follower contents.
 * All tests must use a single shard, so we don't have to guess where the keys are saved.
 * writeConcern must be equal to replicationFactor, so we replicate all documents on all followers.
 */
const replicatedStateFollowerSuite = function (dbParams) {
  const rc = lh.dbservers.length;
  const isReplication2 = dbParams.replicationVersion === "2";
  const servers = Object.assign({}, ...lh.dbservers.map((serverId) => ({[serverId]: lh.getServerUrl(serverId)})));
  let collection = null;
  let shardId = null;

  const {setUpAll, tearDownAll, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      collection = db._create(collectionName, {"numberOfShards": 1, "writeConcern": rc, "replicationFactor": rc});
      shardId = collection.shards()[0];
    }),
    tearDown: tearDownAnd(() => {
      if (collection !== null) {
        collection.drop();
      }
      collection = null;
    }),

    testFollowersSingleDocument: function(testName) {
      let handle = collection.insert({_key: `${testName}-foo`, value: `${testName}-bar`});
      checkFollowersValue(servers, shardId, `${testName}-foo`, `${testName}-bar`, isReplication2);

      handle = collection.update(handle, {value: `${testName}-baz`});
      checkFollowersValue(servers, shardId, `${testName}-foo`, `${testName}-baz`, isReplication2);

      handle = collection.replace(handle, {_key: `${testName}-foo`, value: `${testName}-bar`});
      checkFollowersValue(servers, shardId, `${testName}-foo`, `${testName}-bar`, isReplication2);

      collection.remove(handle);
      checkFollowersValue(servers, shardId, `${testName}-foo`, null, isReplication2);
    },

    testFollowersMultiDocuments: function(testName) {
      let documents = [...Array(3).keys()].map(i => {return {_key: `${testName}-foo${i}`, value: i};});

      let handles = collection.insert(documents);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, doc.value, isReplication2);
      }

      let updates = documents.map(doc => {return {value: doc.value + 100};});
      handles = collection.update(handles, updates);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, doc.value + 100, isReplication2);
      }

      handles = collection.replace(handles, documents);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, doc.value, isReplication2);
      }

      collection.remove(handles);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, null, isReplication2);
      }
    },

    testFollowersAql: function(testName) {
      let documents = [...Array(3).keys()].map(i => {return {_key: `${testName}-foo${i}`, value: i};});

      db._query(`FOR i in 0..9 INSERT {_key: CONCAT('${testName}-foo', i), value: i} INTO ${collectionName}`);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, doc.value, isReplication2);
      }

      db._query(`FOR doc IN ${collectionName} UPDATE {_key: doc._key, value: doc.value + 100} IN ${collectionName}`);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, doc.value + 100, isReplication2);
      }

      db._query(`FOR doc IN ${collectionName} REPLACE {_key: doc._key, value: CONCAT(doc._key, "bar")} IN ${collectionName}`);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, doc._key + "bar", isReplication2);
      }

      db._query(`FOR doc IN ${collectionName} REMOVE doc IN ${collectionName}`);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, null, isReplication2);
      }
    },

    testFollowersTruncate: function(testName) {
      collection.insert({_key: `${testName}-foo`, value: `${testName}-bar`});
      checkFollowersValue(servers, shardId, `${testName}-foo`, `${testName}-bar`, isReplication2);
      collection.truncate();
      checkFollowersValue(servers, shardId, `${testName}-foo`, null, isReplication2);
    }
  };
};

const replicatedStateDocumentStoreSuiteDatabaseDeletionReplication2 = function () {
  let previousDatabase, databaseExisted = true;
  return {
    setUpAll: function () {
      previousDatabase = db._name();
      if (!_.includes(db._databases(), database)) {
        db._createDatabase(database, {"replicationVersion": "2"});
        databaseExisted = false;
      }
      db._useDatabase(database);
    },

    tearDownAll: function () {
      db._useDatabase(previousDatabase);
      if (_.includes(db._databases(), database) && !databaseExisted) {
        db._dropDatabase(database);
      }
    },
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

    testDropDatabase: function() {
      let collection = db._create(collectionName, {"numberOfShards": 2, "writeConcern": 2, "replicationFactor": 3});
      let shards = collection.shards();
      db._useDatabase("_system");
      db._dropDatabase(database);
      for (const shard of shards) {
        let {plan} = sh.readReplicatedStateAgency(database, lh.shardIdToLogId(shard));
        assertEqual(plan, undefined, `Expected nothing in plan for shard ${shard}, got ${JSON.stringify(plan)}`);
      }
    },
  };
};

/**
 * This test suite checks that the cluster is still usable after we alter the state of some participants.
 * For example, after a failover.
 */
const replicatedStateRecoverySuite = function () {
  const coordinator = lh.coordinators[0];
  let collection = null;
  let shards = null;
  let shardId = null;
  let logId = null;

  const {setUpAll, tearDownAll, stopServerWait, continueServerWait, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      collection = db._create(collectionName, {"numberOfShards": 1, "writeConcern": 2, "replicationFactor": 3});
      shards = collection.shards();
      shardId = shards[0];
      logId = shardId.slice(1);
    }),
    tearDown: tearDownAnd(() => {
      if (collection !== null) {
        collection.drop();
      }
      collection = null;
    }),

    testRecoveryFromFailover: function (testName) {
      const participants = lhttp.listLogs(coordinator, database).result[logId];
      assertTrue(participants !== undefined);
      let servers = Object.assign({}, ...participants.map((serverId) => ({[serverId]: lh.getServerUrl(serverId)})));

      let handle = collection.insert({_key: `${testName}-foo`, value: `${testName}-bar`});
      checkFollowersValue(servers, shardId, `${testName}-foo`, `${testName}-bar`, true);


      // We unset the leader here so that once the old leader node is resumed we
      // do not move leadership back to that node.
      lh.unsetLeader(database, logId);

      // Stop the leader. This triggers a failover.
      let leader = participants[0];
      let followers = participants.slice(1);
      let term = lh.readReplicatedLogAgency(database, logId).plan.currentTerm.term;
      let newTerm = term + 2;
      stopServerWait(leader);
      lh.waitFor(lp.replicatedLogLeaderEstablished(database, logId, newTerm, followers));

      syncShardsWithLogs(database);

      // Check if the universal abort command appears in the log during the current term.
      let logContents = lh.dumpShardLog(shardId);
      let abortAllEntryFound = _.some(logContents, entry => {
        if (entry.logTerm !== newTerm || entry.payload === undefined) {
          return false;
        }
        return entry.payload[1].operation === "AbortAllOngoingTrx";
      });
      assertTrue(abortAllEntryFound, `Log contents for ${shardId}: ${JSON.stringify(logContents)}`);

      // Try a new transaction.
      servers = Object.assign({}, ...followers.map((serverId) => ({[serverId]: lh.getServerUrl(serverId)})));
      collection.update(handle, {value: `${testName}-baz`});
      checkFollowersValue(servers, shardId, `${testName}-foo`, `${testName}-baz`, true);

      // Try an AQL query.
      let documents = [...Array(3).keys()].map(i => {return {_key: `${testName}-${i}`, value: i};});
      db._query(`FOR i in 0..10 INSERT {_key: CONCAT('${testName}-', i), value: i} INTO ${collectionName}`);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, doc.value, true);
      }

      // Resume the dead server.
      continueServerWait(leader);
      syncShardsWithLogs(database);

      // Expect to find all values on the awakened server.
      servers = {[leader]: lh.getServerUrl(leader)};
      checkFollowersValue(servers, shardId, `${testName}-foo`, `${testName}-baz`, true);
      for (let doc of documents) {
        checkFollowersValue(servers, shardId, doc._key, doc.value, true);
      }
    },
  };
};

const replicatedStateDocumentStoreSuiteReplication1 = function () {
  const {setUpAll, tearDownAll, setUp, tearDown} =
    lh.testHelperFunctions(database, {replicationVersion: "1"});

  return {
    setUpAll,
    tearDownAll,
    setUp,
    tearDown,

    testDoesNotCreateReplicatedStateForEachShard: function() {
      let collection = db._create(collectionName, {"numberOfShards": 2, "writeConcern": 2, "replicationFactor": 3});
      for (const shard of collection.shards()) {
        let {target} = sh.readReplicatedStateAgency(database, lh.shardIdToLogId(shard));
        assertEqual(target, undefined, `Expected nothing in target for shard ${shard}, got ${JSON.stringify(target)}`);
      }
    },
  };
};

function replicatedStateFollowerSuiteV1() { return makeTestSuites(replicatedStateFollowerSuite)[0]; }
function replicatedStateFollowerSuiteV2() { return makeTestSuites(replicatedStateFollowerSuite)[1]; }

jsunity.run(replicatedStateDocumentStoreSuiteReplication2);
jsunity.run(replicatedStateDocumentStoreSuiteDatabaseDeletionReplication2);
jsunity.run(replicatedStateDocumentStoreSuiteReplication1);
jsunity.run(replicatedStateFollowerSuiteV1);
jsunity.run(replicatedStateFollowerSuiteV2);
jsunity.run(replicatedStateRecoverySuite);

return jsunity.done();
