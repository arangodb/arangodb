/*jshint strict: true */
/*global assertTrue, assertEqual*/
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
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");

const database = "replication2_document_store_test_db";
const collectionName = "testCollection";

const shardIdToLogId = function (shardId) {
  return shardId.slice(1);
};

const getDocumentEntry = function (document, entries) {
  for (const entry of entries) {
    if (entry.hasOwnProperty("payload") && entry.payload[1].data.name === document.name) {
      return entry;
    }
  }
  return null;
};

const replicatedStateDocumentStoreSuiteReplication2 = function () {
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
      if (!databaseExisted) {
        db._dropDatabase(database);
      }
    },
    setUp: function (testName) {
      lh.registerAgencyTestBegin(testName);
      db._create(collectionName, {"numberOfShards": 2, "writeConcern": 2, "replicationFactor": 3});
    },
    tearDown: function (testName) {
      if (db._collection(collectionName) !== null) {
        db._drop(collectionName);
      }
      lh.registerAgencyTestEnd(testName);
    },

    testCreateReplicatedStateForEachShard: function() {
      let collection = db._collection(collectionName);
      let colPlan = lh.readAgencyValueAt(`Plan/Collections/${database}/${collection._id}`);
      let colCurrent = lh.readAgencyValueAt(`Current/Collections/${database}/${collection._id}`);

      for (const shard of collection.shards()) {
        let {target, plan, current} = sh.readReplicatedStateAgency(database, shardIdToLogId(shard));
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
      let collection = db._collection(collectionName);
      let shards = collection.shards();
      db._drop(collectionName);
      for (const shard of shards) {
        let {plan} = sh.readReplicatedStateAgency(database, shardIdToLogId(shard));
        assertEqual(plan, undefined);
      }
    },

    testReplicateOperationsInsert: function() {
      let collection = db._collection(collectionName);
      let shards = collection.shards();
      let logs = shards.map(shardId => db._replicatedLog(shardId.slice(1)));
      let documents = [{name: "foo"}, {name: "bar"}];

      documents.forEach(doc => collection.insert(doc));
      let allEntries = logs.reduce((previous, current) => previous.concat(current.head(1000)), []);
      for (const doc of documents) {
        let entry = getDocumentEntry(doc, allEntries);
        assertTrue(entry !== null);
        assertEqual(entry.payload[1].operation, "insert");
      }
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
        let {plan} = sh.readReplicatedStateAgency(database, shardIdToLogId(shard));
        assertEqual(plan, undefined);
      }
    },
  };
};

const replicatedStateDocumentStoreSuiteReplication1 = function () {
  let previousDatabase, databaseExisted = true;
  return {
    setUpAll: function () {
      previousDatabase = db._name();
      if (!_.includes(db._databases(), database)) {
        db._createDatabase(database, {"replicationVersion": "1"});
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
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

    testDoesNotCreateReplicatedStateForEachShard: function() {
      let collection = db._create(collectionName, {"numberOfShards": 2, "writeConcern": 2, "replicationFactor": 3});
      for (const shard of collection.shards()) {
        let {target} = sh.readReplicatedStateAgency(database, shardIdToLogId(shard));
        assertEqual(target, undefined);
      }
    },
  };
};

jsunity.run(replicatedStateDocumentStoreSuiteReplication2);
jsunity.run(replicatedStateDocumentStoreSuiteDatabaseDeletionReplication2);
jsunity.run(replicatedStateDocumentStoreSuiteReplication1);
return jsunity.done();
