/*jshint strict: true */
/*global assertTrue, assertFalse, assertEqual, assertNotNull, print*/
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
const internal = require("internal");
const _ = require('lodash');
const db = arangodb.db;
const helper = require('@arangodb/test-helper');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const lp = require("@arangodb/testutils/replicated-logs-predicates");
const lhttp = require('@arangodb/testutils/replicated-logs-http-helper');
const dh = require("@arangodb/testutils/document-state-helper");
const request = require("@arangodb/request");

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
 * This test suite validates the correctness of most basic operations, checking replicated log entries.
 */
const replicatedStateDocumentStoreSuiteReplication2 = function () {
  let collection = null;
  let shards = null;
  let shardsToLogs = null;
  let logs = null;

  const {setUpAll, tearDownAll, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      collection = db._create(collectionName, {"numberOfShards": 2, "writeConcern": 2, "replicationFactor": 3});
      shards = collection.shards();
      shardsToLogs = lh.getShardsToLogsMapping(database, collection._id);
      logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
    }),
    tearDown: tearDownAnd(() => {
      if (collection !== null) {
        collection.drop();
      }
      collection = null;
    }),

    testCreateReplicatedStateForEachShard: function() {
      const colPlan = lh.readAgencyValueAt(`Plan/Collections/${database}/${collection._id}`);
      const colCurrent = lh.readAgencyValueAt(`Current/Collections/${database}/${collection._id}`);

      for (const shard of collection.shards()) {
        const logId = shardsToLogs[shard];
        assertFalse(logId === undefined, `No log found for shard ${shard}`);

        let {target, plan, current} = lh.readReplicatedLogAgency(database, logId);
        let shardPlan = colPlan.shards[shard];
        let shardCurrent = colCurrent[shard];

        // Check if the replicated state in Target and the shard in Plan match
        assertEqual(target.leader, shardPlan[0]);
        assertTrue(lh.sortedArrayEqualOrError(Object.keys(target.participants)), _.sortBy(shardPlan));

        // Check if the replicated state in Plan and the shard in Plan match
        assertTrue(lh.sortedArrayEqualOrError(Object.keys(plan.participantsConfig.participants)), _.sortBy(shardPlan));

        // Check if the replicated state in Current and the shard in Current match
        assertEqual(target.leader, shardCurrent.servers[0]);
        assertTrue(lh.sortedArrayEqualOrError(Object.keys(current.localStatus)), _.sortBy(shardCurrent.servers));
      }
    },

    testReplicateOperationsCommit: function() {
      const opType = "Commit";

      collection.insert({_key: "abcd"});
      const mergedLogs = dh.mergeLogs(logs);
      let commitEntries = dh.getDocumentEntries(mergedLogs, opType);
      let insertEntries = dh.getDocumentEntries(mergedLogs, "Insert");
      assertEqual(commitEntries.length, 1,
          `Found more commitEntries than expected: ${JSON.stringify(commitEntries)}. Insert entries: ${JSON.stringify(insertEntries)}`);
      assertEqual(insertEntries.length, commitEntries.length,
          `Insert entries: ${JSON.stringify(insertEntries)} do not match Commit entries ${JSON.stringify(commitEntries)}`);
      assertEqual(insertEntries[0].trx, commitEntries[0].trx,
          `Insert entries: ${JSON.stringify(insertEntries)} do not match Commit entries ${JSON.stringify(commitEntries)}`);
    },

    testReplicateOperationsInsert: function() {
      const opType = "Insert";

      // Insert single document
      let documents = [{_key: "foo"}, {_key: "bar"}];
      documents.forEach(doc => collection.insert(doc));
      dh.searchDocs(logs, documents, opType);

      // Insert multiple documents
      documents = [...Array(10).keys()].map(i => {return {name: "testInsert1", foobar: i};});
      collection.insert(documents);
      let result = dh.getArrayElements(logs, opType, "testInsert1");
      for (const doc of documents) {
        assertTrue(result.find(entry => entry.foobar === doc.foobar) !== undefined);
      }

      // AQL INSERT
      documents = [...Array(10).keys()].map(i => {return {name: "testInsert2", baz: i};});
      db._query(`FOR i in 0..9 INSERT {_key: CONCAT('test', i), name: "testInsert2", baz: i} INTO ${collectionName}`);
      result = dh.getArrayElements(logs, opType, "testInsert2");
      for (const doc of documents) {
        assertTrue(result.find(entry => entry.baz === doc.baz) !== undefined);
      }
    },

    testReplicateOperationsModify: function() {
      const opType = "Update";

      // Choose two distinct random numbers
      const numDocs = 2;
      const nums = new Set();
      while(nums.size !== numDocs) {
        nums.add(_.random(1000));
      }

      // Update single document
      const documents = [...nums].map(i => ({_key: `test${i}`}));
      let docHandles = [];
      documents.forEach(doc => {
        let docUpdate = {_key: doc._key, name: `updatedTest${doc.value}`};
        let d = collection.insert(doc);
        docHandles.push(collection.update(d, docUpdate));
        dh.searchDocs(logs, [docUpdate], opType);
      });

      // Replace multiple documents
      let replacements = [{value: 10, name: "testR"}, {value: 20, name: "testR"}];
      docHandles = collection.replace(docHandles, replacements);
      let result = dh.getArrayElements(logs, "Replace",  "testR");
      for (const doc of replacements) {
        assertTrue(result.find(entry => entry.value === doc.value) !== undefined);
      }

      // Update multiple documents
      let updates = documents.map(_ => {return {name: "testModify10"};});
      docHandles = collection.update(docHandles, updates);
      result = dh.getArrayElements(logs, opType, "testModify10");
      assertEqual(result.length, updates.length);

      // AQL UPDATE
      db._query(`FOR doc IN ${collectionName} UPDATE {_key: doc._key, name: "testModify100"} IN ${collectionName}`);
      result = dh.getArrayElements(logs, opType, "testModify100");
      assertEqual(result.length, updates.length);
    },

    testReplicateOperationsRemove: function() {
      const opType = "Remove";

      // Remove single document
      let doc = {_key: `test${_.random(1000)}`};
      let d = collection.insert(doc);
      collection.remove(d);
      dh.searchDocs(logs, [doc], opType);

      // AQL REMOVE
      let documents = [
        {_key: `test${_.random(1000)}`}, {_key: `test${_.random(1000)}`}
      ];
      collection.insert(documents);
      db._query(`FOR doc IN ${collectionName} REMOVE doc IN ${collectionName}`);
      let result = dh.getArrayElements(logs, opType);
      for (const doc of documents) {
        assertTrue(result.find(entry => entry._key === doc._key) !== undefined);
      }
    },

    testReplicateOperationsTruncate: function() {
      const opType = "Truncate";
      // we need to fill the collection with some docs since truncate will only
      // use range-deletes (which are replicated as truncate op) if the number
      // of docs is > 32*1024, otherwise it uses babies removes.
      // note that our collection has multiple shards, so we need to make sure that
      // each shard has >= 32*1024 docs!
      const numberOfShards = collection.properties().numberOfShards;
      let docs = [];
      for (let j = 0; j < 1024; ++j) {
        docs.push({});
      }
      for (let i = 0; i < 33 * numberOfShards; ++i) {
        collection.insert(docs);
      }
      collection.truncate();

      let found = [];
      let allEntries = logs.reduce((previous, current) => previous.concat(current.head(1010)), []);
      for (const entry of allEntries) {
        if (entry.hasOwnProperty("payload") && entry.payload.operation.type === opType) {
          let colName = entry.payload.operation.shard;
          assertTrue(shards.includes(colName) && !found.includes(colName));
          found.push(colName);
        }
      }
      assertEqual(found.length, 2, `Dumping combined log entries (excluding inserts): ` +
          JSON.stringify(allEntries.filter(entry => !entry.hasOwnProperty("payload") ||
              entry.hasOwnProperty("payload") && entry.payload.operation.type !== "Insert"
              && entry.payload.operation.type !== "Commit")));
    }
  };
};

/**
 * This test suite intentionally triggers intermediate commit operations and validates their execution.
 */
const replicatedStateIntermediateCommitsSuite = function() {
  const rc = lh.dbservers.length;
  const servers = Object.assign({}, ...lh.dbservers.map((serverId) => ({[serverId]: lh.getServerUrl(serverId)})));
  let collection = null;
  let shards = null;
  let shardId = null;
  let logs = null;
  let shardsToLogs = null;

  const {setUpAll, tearDownAll, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      const props = {numberOfShards: 1, writeConcern: rc, replicationFactor: rc};
      collection = db._create(collectionName, props);
      shards = collection.shards();
      shardId = shards[0];
      shardsToLogs = lh.getShardsToLogsMapping(database, collection._id);
      logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
    }),
    tearDown: tearDownAnd(() => {
      if (collection !== null) {
        collection.drop();
      }
      collection = null;
    }),

    testIntermediateCommitsNoLogEntries: function(testName) {
      db._query(`FOR i in 0..10 INSERT {_key: CONCAT('test', i), name: '${testName}', baz: i} INTO ${collectionName}`);
      let intermediateCommitEntries = dh.getDocumentEntries(dh.mergeLogs(logs), "IntermediateCommit");
      assertEqual(intermediateCommitEntries.length, 0);
    },

    testIntermediateCommitsLogEntries: function(testName) {
      db._query(`FOR i in 0..1000 INSERT {_key: CONCAT('test', i), name: '${testName}', baz: i} INTO ${collectionName}`,
        {}, {intermediateCommitCount: 1});
      let intermediateCommitEntries = dh.getDocumentEntries(dh.mergeLogs(logs), "IntermediateCommit");
      assertEqual(intermediateCommitEntries.length, 2);
    },

    testIntermediateCommitsFull: function(testName) {
      db._query(`
      FOR i in 0..2000 
      INSERT {_key: CONCAT('test', i), name: '${testName}', value: i} INTO ${collectionName}`,
        {}, {intermediateCommitCount: 1});
      let keys = [];
      for (let i = 0; i <= 2000; ++i) {
        keys.push(`test${i}`);
      }

      // Wait for the last key to be applied on all servers
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `test2000`, 2000, true);
      // Check that all keys are applied on all servers
      for (let server of Object.values(servers)) {
        let bulk = dh.getBulkDocuments(server, database, shardId, keys);
        let keysSet = new Set(keys);
        for (let doc of bulk) {
          assertTrue(keysSet.has(doc._key));
          keysSet.delete(doc._key);
        }
      }
    },

    testIntermediateCommitsPartial: function(testName) {
      // Intentionally fail the query after an intermediate commit
      try {
        db._query(`
        FOR i in 0..2000
        FILTER ASSERT(i < 1500, "was erlaube")
        INSERT {_key: CONCAT('test', i), name: '${testName}', value: i} INTO ${collectionName}`, {},
          {intermediateCommitCount: 1});
      } catch (err) {
        assertEqual(err.errorNum, internal.errors.ERROR_QUERY_USER_ASSERT.code);
      }

      // AQL operates in batches of 1000 documents. Each batch is processed in a single babies operation.
      // Intermediate commits are only performed after a babies operation.
      // The first 1000 documents are part of the first intermediate commit, after which the query fails.
      let keys = [];
      for (let i = 0; i <= 2000; ++i) {
        keys.push(`test${i}`);
      }

      // Wait for the last key to be applied on all servers
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `test999`, 999, true);

      // Check that first batch of keys is applied on all servers
      for (let server of Object.values(servers)) {
        let bulk = dh.getBulkDocuments(server, database, shardId, keys);
        for (let doc of bulk) {
          assertEqual(doc.value < 1000, doc._key !== undefined);
        }
      }
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
  let shards = null;
  let shardsToLogs = null;
  let logs = null;

  const {setUpAll, tearDownAll, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      collection = db._create(collectionName, {"numberOfShards": 1, "writeConcern": rc, "replicationFactor": rc});
      shards = collection.shards();
      shardId = shards[0];
      shardsToLogs = lh.getShardsToLogsMapping(database, collection._id);
      logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
    }),
    tearDown: tearDownAnd(() => {
      if (collection !== null) {
        collection.drop();
      }
      collection = null;
    }),

    testFollowersSingleDocument: function(testName) {
      let handle = collection.insert({_key: `${testName}-foo`, value: `${testName}-bar`});
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, `${testName}-bar`, isReplication2);

      handle = collection.update(handle, {value: `${testName}-baz`});
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, `${testName}-baz`, isReplication2);

      handle = collection.replace(handle, {_key: `${testName}-foo`, value: `${testName}-bar`});
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, `${testName}-bar`, isReplication2);

      collection.remove(handle);
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, null, isReplication2);
    },

    testFollowersMultiDocuments: function(testName) {
      let documents = [...Array(3).keys()].map(i => {return {_key: `${testName}-foo${i}`, value: i};});

      let handles = collection.insert(documents);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, doc.value, isReplication2);
      }

      let updates = documents.map(doc => {return {value: doc.value + 100};});
      handles = collection.update(handles, updates);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, doc.value + 100, isReplication2);
      }

      handles = collection.replace(handles, documents);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, doc.value, isReplication2);
      }

      collection.remove(handles);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, null, isReplication2);
      }
    },

    testFollowersAql: function(testName) {
      let documents = [...Array(3).keys()].map(i => {return {_key: `${testName}-foo${i}`, value: i};});

      db._query(`FOR i in 0..9 INSERT {_key: CONCAT('${testName}-foo', i), value: i} INTO ${collectionName}`);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, doc.value, isReplication2);
      }

      db._query(`FOR doc IN ${collectionName} UPDATE {_key: doc._key, value: doc.value + 100} IN ${collectionName}`);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, doc.value + 100, isReplication2);
      }

      db._query(`FOR doc IN ${collectionName} REPLACE {_key: doc._key, value: CONCAT(doc._key, "bar")} IN ${collectionName}`);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, doc._key + "bar", isReplication2);
      }

      db._query(`FOR doc IN ${collectionName} REMOVE doc IN ${collectionName}`);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, null, isReplication2);
      }
    },

    testFollowersTruncate: function(testName) {
      collection.insert({_key: `${testName}-foo`, value: `${testName}-bar`});
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, `${testName}-bar`, isReplication2);
      collection.truncate();
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, null, isReplication2);
    }
  };
};

/**
 * This test suite checks that everything is left clean after dropping a database.
 */
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
      let shardsToLogs = lh.getShardsToLogsMapping(database, collection._id);
      db._useDatabase("_system");
      for (const shard of shards) {
        let logId = shardsToLogs[shard];
        let {plan} = lh.readReplicatedLogAgency(database, logId);
        assertFalse(plan === undefined, `Expected plan entry for shard ${logId}`);
      }
      db._dropDatabase(database);
      for (const shard of shards) {
        let logId = shardsToLogs[shard];
        let {plan} = lh.readReplicatedLogAgency(database, logId);
        assertEqual(plan, undefined, `Expected nothing in plan for replicated log ${logId}, got ${JSON.stringify(plan)}`);
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
  let logs = null;
  let shardsToLogs = null;

  const {setUpAll, tearDownAll, stopServerWait, continueServerWait, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      collection = db._create(collectionName, {"numberOfShards": 1, "writeConcern": 2, "replicationFactor": 3});
      shards = collection.shards();
      shardsToLogs = lh.getShardsToLogsMapping(database, collection._id);
      logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
      shardId = shards[0];
      logId = shardsToLogs[shardId];
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
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, `${testName}-bar`, true);

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

      // Check if the universal abort command appears in the log during the current term.
      let logContents = lh.dumpLogHead(logId);
      let abortAllEntryFound = _.some(logContents, entry => {
        if (entry.logTerm !== newTerm || entry.payload === undefined) {
          return false;
        }
        return entry.payload.operation.type === "AbortAllOngoingTrx";
      });
      assertTrue(abortAllEntryFound, `Log contents for ${shardId}: ${JSON.stringify(logContents)}`);

      // Try a new transaction.
      servers = Object.assign({}, ...followers.map((serverId) => ({[serverId]: lh.getServerUrl(serverId)})));
      collection.update(handle, {value: `${testName}-baz`});
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, `${testName}-baz`, true);

      // Try an AQL query.
      let documents = [...Array(3).keys()].map(i => {return {_key: `${testName}-${i}`, value: i};});
      db._query(`FOR i in 0..3 INSERT {_key: CONCAT('${testName}-', i), value: i} INTO ${collectionName}`);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, doc.value, true);
      }

      // Resume the dead server.
      continueServerWait(leader);

      // Expect to find all values on the awakened server.
      servers = {[leader]: lh.getServerUrl(leader)};
      dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], `${testName}-foo`, `${testName}-baz`, true);
      for (let doc of documents) {
        dh.checkFollowersValue(servers, database, shardId, shardsToLogs[shardId], doc._key, doc.value, true);
      }
    },
  };
};

/**
 * This test suite checks that replication2 leaves no side effects when creating a replication1 DB.
 */
const replicatedStateDocumentStoreSuiteReplication1 = function () {
  const dbNameR1 = "replication1TestDatabase";
  const {setUpAll, tearDownAll, setUp, tearDown} =
    lh.testHelperFunctions(dbNameR1, {replicationVersion: "1"});

  return {
    setUpAll,
    tearDownAll,
    setUp,
    tearDown,

    testDoesNotCreateReplicatedStateForEachShard: function() {
      db._create(collectionName, {"numberOfShards": 2, "writeConcern": 2, "replicationFactor": 3});
      let plan = lh.readAgencyValueAt(`Plan/ReplicatedLogs/${dbNameR1}`);
      assertEqual(plan, undefined, `Expected no replicated logs in agency, got ${JSON.stringify(plan)}`);
    },
  };
};

/**
 * This test suite checks the correctness of a DocumentState snapshot transfer and the related REST APIs.
 */
const replicatedStateSnapshotTransferSuite = function () {
  const coordinator = lh.coordinators[0];
  let collection = null;
  let shards = null;
  let shardId = null;
  let logId = null;
  let log = null;
  let shardsToLogs = null;
  let logs = null;

  const {setUpAll, tearDownAll, setUpAnd, tearDownAnd} =
      lh.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      collection = db._create(collectionName, {"numberOfShards": 1, "writeConcern": 2, "replicationFactor": 3});
      shards = collection.shards();
      shardsToLogs = lh.getShardsToLogsMapping(database, collection._id);
      logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
      shardId = shards[0];
      logId = shardsToLogs[shardId];
      log = db._replicatedLog(logId);
    }),
    tearDown: tearDownAnd(() => {
      if (collection !== null) {
        collection.drop();
      }
      collection = null;
    }),

    // This is disabled because we currently implement no cleanup for snapshots
    DISABLED_testDropCollectionOngoingTransfer: function (testName) {
      collection.insert({_key: testName});
      let {leader} = lh.getReplicatedLogLeaderPlan(database, logId);
      let leaderUrl = lh.getServerUrl(leader);
      let url = `${leaderUrl}/_db/${database}/_api/document-state/${logId}/snapshot/first?waitForIndex=0`;
      let result = request.get({url: url});
      lh.checkRequestResult(result);
    },

    testFollowerSnapshotTransfer: function () {
      // Prepare the grounds for replacing a follower.
      const participants = lhttp.listLogs(coordinator, database).result[logId];
      const followers = participants.slice(1);
      const oldParticipant = _.sample(followers);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      assertTrue(nonParticipants.length > 0, "Not enough DBServers to run this test");
      const newParticipant = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldParticipant), [newParticipant]).sort();

      let documents1 = [];
      for (let counter = 0; counter < 100; ++counter) {
        documents1.push({_key: `foo${counter}`});
      }
      for (let idx = 0; idx < documents1.length; ++idx) {
        collection.insert(documents1[idx]);
      }

      // Trigger compaction intentionally.
      log.compact();

      let documents2 = [];
      for (let counter = 0; counter < 100; ++counter) {
        documents2.push({_key: `bar${counter}`});
      }
      for (let idx = 0; idx < documents2.length; ++idx) {
        collection.insert(documents2[idx]);
      }

      // Replace the follower.
      const result = lh.replaceParticipant(database, logId, oldParticipant, newParticipant);
      assertEqual({}, result);

      // Wait for replicated state to be available on the new follower.
      lh.waitFor(() => {
        const {current} = lh.readReplicatedLogAgency(database, logId);
        if (current.leader.hasOwnProperty("committedParticipantsConfig")) {
          return lh.sortedArrayEqualOrError(
            newParticipants,
            Object.keys(current.leader.committedParticipantsConfig.participants).sort());
        } else {
          return Error(`committedParticipantsConfig not found in ` +
            JSON.stringify(current.leader));
        }
      });

      let checkKeys = [...documents1.map(doc => doc._key)].concat([...documents2.map(doc => doc._key)]);
      let bulk = dh.getBulkDocuments(lh.getServerUrl(newParticipant), database, shardId, checkKeys);
      let keysSet = new Set(checkKeys);
      for (let doc of bulk) {
        assertFalse(doc.hasOwnProperty("error"), `Expected no error, got ${JSON.stringify(doc)}`);
        assertTrue(keysSet.has(doc._key));
        keysSet.delete(doc._key);
      }
    },

    testRecoveryAfterCompaction: function () {
      collection.insert([{_key: "test1"}, {_key: "test2"}]);
      let documents = [];
      for (let counter = 0; counter < 100; ++counter) {
        documents.push({_key: `foo${counter}`});
      }
      for (let idx = 0; idx < documents.length; ++idx) {
        collection.insert(documents[idx]);
      }

      // Trigger compaction intentionally.
      log.compact();

      // TODO this is not safe, we might loose already committed log entries.
      //      either force the leader in the first place, or make sure a leader
      //      election is done (by deleting the current leader when increasing the term)
      lh.bumpTermOfLogsAndWaitForConfirmation(database, collection);

      let checkKeys = documents.map(doc => doc._key);
      let {leader} = lh.getReplicatedLogLeaderPlan(database, logId);
      let bulk = dh.getBulkDocuments(lh.getServerUrl(leader), database, shardId, checkKeys);
      let keysSet = new Set(checkKeys);
      for (let doc of bulk) {
        assertFalse(doc.hasOwnProperty("error"), `Expected no error, got ${JSON.stringify(doc)}`);
        assertTrue(keysSet.has(doc._key));
        keysSet.delete(doc._key);
      }
    }
  };
};


function replicatedStateFollowerSuiteV1() { return makeTestSuites(replicatedStateFollowerSuite)[0]; }
function replicatedStateFollowerSuiteV2() { return makeTestSuites(replicatedStateFollowerSuite)[1]; }

jsunity.run(replicatedStateDocumentStoreSuiteReplication2);
jsunity.run(replicatedStateDocumentStoreSuiteDatabaseDeletionReplication2);
jsunity.run(replicatedStateDocumentStoreSuiteReplication1);
jsunity.run(replicatedStateIntermediateCommitsSuite);
jsunity.run(replicatedStateFollowerSuiteV1);
jsunity.run(replicatedStateFollowerSuiteV2);
jsunity.run(replicatedStateRecoverySuite);
jsunity.run(replicatedStateSnapshotTransferSuite);

return jsunity.done();
