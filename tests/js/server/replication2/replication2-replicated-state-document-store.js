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
const request = require("@arangodb/request");
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");

const database = "replication2_document_store_test_db";
const collectionName = "testCollection";

const shardIdToLogId = function (shardId) {
  return shardId.slice(1);
};

const getShardDistribution = function (serverUrl, database, collection) {
  let result = request.get({url: `${serverUrl}/_db/${database}/_admin/cluster/shardDistribution`});
  lh.checkRequestResult(result);
  return result.json.results[collection];
};

const getDbServersById = function() {
  return global.ArangoClusterInfo.getDBServers().reduce((result, x) => {
    result[x.serverId] = x.serverName;
    return result;
  }, {});
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
    setUp: function () {
      lh.registerAgencyTestBegin();
      db._create(collectionName, {"numberOfShards": 2, "writeConcern": 2, "replicationFactor": 3});
    },
    tearDown: function () {
      if (db._collection(collectionName) !== null) {
        db._drop(collectionName);
      }
      lh.registerAgencyTestEnd();
    },

    testCreateReplicatedStateForEachShard: function() {
      let collection = db._collection(collectionName);

      let dbServersIdToName = getDbServersById();
      let coord = lh.coordinators[0];
      let coordUrl = lh.getServerUrl(coord);
      let shardDistribution = getShardDistribution(coordUrl, database, collectionName);
      assertTrue(shardDistribution !== undefined);

      const checkTarget = (state, shard) => {
        assertEqual(dbServersIdToName[state.leader], shardDistribution.Current[shard].leader);
        for (const p of Object.keys(state.participants)) {
          let name = dbServersIdToName[p];
          assertTrue(name === shardDistribution.Current[shard].leader
              || shardDistribution.Current[shard].followers.includes(name));
        }
      };

      const checkCurrent = (state, shard) => {
        assertEqual(state.supervision.version, 1);
        for (const [p, status] of Object.entries(state.participants)) {
          let name = dbServersIdToName[p];
          assertTrue(name === shardDistribution.Current[shard].leader
              || shardDistribution.Current[shard].followers.includes(name));
          assertEqual(status.snapshot.status, "Completed");
        }
      };

      for (const shard of collection.shards()) {
        let {target, current} = sh.readReplicatedStateAgency(database, shardIdToLogId(shard));
        checkTarget(target, shard);
        checkCurrent(current, shard);
      }
    },

    testDeleteReplicatedStateForEachShard: function() {
      let collection = db._collection(collectionName);
      let shards = collection.shards();
      db._drop(collectionName);
      for (const shard of shards) {
        let {plan} = sh.readReplicatedStateAgency(database, shardIdToLogId(shard));
        assertEqual(plan, undefined);
      }
    },
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

    testDatabaseDeletion: function() {
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

    testDoNotCreateReplicatedStateForEachShard: function() {
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
