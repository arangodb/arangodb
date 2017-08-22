/* global ArangoServerState, ArangoClusterInfo */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// / @brief JavaScript cluster functionality
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// /////////////////////////////////////////////////////////////////////////////

var console = require('console');
var arangodb = require('@arangodb');
var ArangoCollection = arangodb.ArangoCollection;
var ArangoError = arangodb.ArangoError;
var errors = require("internal").errors;
var request = require('@arangodb/request').clusterRequest;
var wait = require('internal').wait;
var isEnterprise = require('internal').isEnterprise();
var _ = require('lodash');

const curDatabases = '/arango/Current/Databases/';
const curCollections = '/arango/Current/Collections/';
const curVersion = '/arango/Current/Version';
const agencyOperations = {
  'delete' : {'op' : 'delete'},
  'increment' : {'op' : 'increment'}
};

// good enough isEqual which does not depend on equally sorted objects as _.isEqual
var isEqual = function(object, other) {
  if (typeof object !== typeof other) {
    return false;
  }

  if (typeof object !== 'object') {
    // explicitly use non strict equal
    // eslint-disable-next-line eqeqeq
    return object == other;
  }

  if (Array.isArray(object)) {
    if (object.length !== other.length) {
      return false;
    }
    return object.every((value, index) => {
      return isEqual(value, other[index]);
    });
  } else if (object === null) {
    return other === null;
  }

  let myKeys = Object.keys(object);
  let otherKeys = Object.keys(other);

  if (myKeys.length !== otherKeys.length) {
    return false;
  }

  return myKeys.every(key => {
    if (!isEqual(object[key], other[key])) {
      return false;
    }
    return true;
  });
};

var endpointToURL = function (endpoint) {
  if (endpoint.substr(0, 6) === 'ssl://') {
    return 'https://' + endpoint.substr(6);
  }
  var pos = endpoint.indexOf('://');
  if (pos === -1) {
    return 'http://' + endpoint;
  }
  return 'http' + endpoint.substr(pos);
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief fetch a READ lock on a collection and will time out after a
// / number of seconds
// /////////////////////////////////////////////////////////////////////////////

function startReadLockOnLeader (endpoint, database, collName, timeout) {
  var url = endpointToURL(endpoint) + '/_db/' + database;
  var r = request({ url: url + '/_api/replication/holdReadLockCollection',
                    method: 'GET' });
  if (r.status !== 200) {
    console.topic("heartbeat=error", 'startReadLockOnLeader: Could not get ID for shard',
      collName, r);
    return false;
  }
  try {
    r = JSON.parse(r.body);
  } catch (err) {
    console.topic("heartbeat=error", 'startReadLockOnLeader: Bad response body from',
      '/_api/replication/holdReadLockCollection', r,
      JSON.stringify(err));
    return false;
  }
  const id = r.id;

  var body = { 'id': id, 'collection': collName, 'ttl': timeout };
  request({ url: url + '/_api/replication/holdReadLockCollection',
            body: JSON.stringify(body),
            method: 'POST', headers: {'x-arango-async': true} });
  // Intentionally do not look at the outcome, even in case of an error
  // we must make sure that the read lock on the leader is not active!
  // This is done automatically below.

  var count = 0;
  while (++count < 20) { // wait for some time until read lock established:
    // Now check that we hold the read lock:
    r = request({ url: url + '/_api/replication/holdReadLockCollection',
                  body: JSON.stringify(body), method: 'PUT' });
    if (r.status === 200) {
      let ansBody = {};
      try {
        ansBody = JSON.parse(r.body);
      } catch (err) {
      }
      if (ansBody.lockHeld) {
        return id;
      } else {
        console.topic('heartbeat=debug', 'startReadLockOnLeader: Lock not yet acquired...');
      }
    } else {
      console.topic('heartbeat=debug', 'startReadLockOnLeader: Do not see read lock yet...');
    }
    wait(0.5);
  }
  console.topic('heartbeat=error', 'startReadLockOnLeader: giving up');
  try {
    r = request({ url: url + '/_api/replication/holdReadLockCollection',
                  body: JSON.stringify({'id': id}), method: 'DELETE' });
  } catch (err2) {
    console.topic('heartbeat=error', 'startReadLockOnLeader: expection in cancel:',
                  JSON.stringify(err2));
  }
  if (r.status !== 200) {
    console.topic('heartbeat=error', 'startReadLockOnLeader: cancelation error for shard',
                  collName, r);
  }
  return false;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief cancel read lock
// /////////////////////////////////////////////////////////////////////////////

function cancelReadLockOnLeader (endpoint, database, lockJobId) {
  // Note that we always use the _system database here because the actual
  // database might be gone already on the leader and we need to cancel
  // the read lock under all circumstances.
  var url = endpointToURL(endpoint) + '/_db/_system' +
    '/_api/replication/holdReadLockCollection';
  var r;
  var body = {'id': lockJobId};
  try {
    r = request({url, body: JSON.stringify(body), method: 'DELETE' });
  } catch (e) {
    console.topic('heartbeat=error', 'cancelReadLockOnLeader: exception caught:', e);
    return false;
  }
  if (r.status !== 200) {
    console.topic('heartbeat=error', 'cancelReadLockOnLeader: error', lockJobId, r.status,
                  r.message, r.body, r.json);
    return false;
  }
  console.topic('heartbeat=debug', 'cancelReadLockOnLeader: success');
  return true;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief cancel barrier from sync
// /////////////////////////////////////////////////////////////////////////////

function cancelBarrier (endpoint, database, barrierId) {
  if (barrierId <= 0) {
    return true;
  }
  var url = endpointToURL(endpoint) + '/_db/' + database +
    '/_api/replication/barrier/' + barrierId;
  var r = request({url, method: 'DELETE' });
  if (r.status !== 200 && r.status !== 204) {
    console.topic('heartbeat=error', 'CancelBarrier: error', r);
    return false;
  }
  console.topic('heartbeat=debug', 'cancelBarrier: success');
  return true;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief tell leader that we are in sync
// /////////////////////////////////////////////////////////////////////////////

function addShardFollower (endpoint, database, shard, lockJobId) {
  console.topic('heartbeat=debug', 'addShardFollower: tell the leader to put us into the follower list...');
  var url = endpointToURL(endpoint) + '/_db/' + database +
    '/_api/replication/addFollower';
  let db = require('internal').db;
  var body = {followerId: ArangoServerState.id(), shard, checksum: db._collection(shard).count() + '', readLockId: lockJobId};
  var r = request({url, body: JSON.stringify(body), method: 'PUT'});
  if (r.status !== 200) {
    console.topic('heartbeat=error', "addShardFollower: could not add us to the leader's follower list.", r);
    return false;
  }
  return true;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief tell leader that we are stop following
// /////////////////////////////////////////////////////////////////////////////

function removeShardFollower (endpoint, database, shard) {
  console.topic('heartbeat=debug', 'removeShardFollower: tell the leader to take us off the follower list...');
  var url = endpointToURL(endpoint) + '/_db/' + database +
    '/_api/replication/removeFollower';
  var body = {followerId: ArangoServerState.id(), shard};
  var r = request({url, body: JSON.stringify(body), method: 'PUT'});
  if (r.status !== 200) {
    console.topic('heartbeat=error', "removeShardFollower: could not remove us from the leader's follower list: ", r.status, r.body);
    return false;
  }
  return true;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief 
// /////////////////////////////////////////////////////////////////////////////

function fetchKey(structure, ...path) {
  let current = structure;
  do {
    let key = path.shift();
    if (typeof current !== 'object' || !current.hasOwnProperty(key)) {
      return undefined;
    }
    current = current[key];
  } while (path.length > 0);
  return current;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief return a shardId => server map
// /////////////////////////////////////////////////////////////////////////////

function getShardMap (plannedCollections) {
  var shardMap = { };

  var database;

  for (database in plannedCollections) {
    if (plannedCollections.hasOwnProperty(database)) {
      var collections = plannedCollections[database];
      var collection;

      for (collection in collections) {
        if (collections.hasOwnProperty(collection)) {
          var shards = collections[collection].shards;
          var shard;

          for (shard in shards) {
            if (shards.hasOwnProperty(shard)) {
              shardMap[shard] = shards[shard];
            }
          }
        }
      }
    }
  }

  return shardMap;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief return a hash with the local databases
// /////////////////////////////////////////////////////////////////////////////

function getLocalDatabases () {
  let result = { };
  let db = require('internal').db;
  let curDb = db._name();
  try {
    db._databases().forEach(function (database) {
      db._useDatabase(database);
      result[database] = { name: database, id: db._id() };
    });
  } finally {
    db._useDatabase(curDb);
  }
  return result;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief return a hash with the local collections
// /////////////////////////////////////////////////////////////////////////////

function getLocalCollections () {
  var result = { };
  var db = require('internal').db;

  db._collections().forEach(function (collection) {
    var name = collection.name();

    if (name.substr(0, 1) !== '_') {
      var data = {
        id: collection._id,
        name: name,
        type: collection.type(),
        status: collection.status(),
        planId: collection.planId(),
        theLeader: collection.getLeader()
      };

      // merge properties
      var properties = collection.properties();
      var p;
      for (p in properties) {
        if (properties.hasOwnProperty(p)) {
          data[p] = properties[p];
        }
      }

      result[name] = data;
    }
  });

  return result;
}

function organiseLeaderResign (database, collId, shardName) {
  console.topic('heartbeat=info', "trying to withdraw as leader of shard '%s/%s' of '%s/%s'",
    database, shardName, database, collId);
  // This starts a write transaction, just to wait for any ongoing
  // write transaction on this shard to terminate. We will then later
  // report to Current about this resignation. If a new write operation
  // starts in the meantime (which is unlikely, since no coordinator that
  // has seen the _ will start a new one), it is doomed, and we ignore the
  // problem, since similar problems can arise in failover scenarios anyway.
  try {
    // we know the shard exists locally!
    var db = require('internal').db;
    db._collection(shardName).setTheLeader("LEADER_NOT_YET_KNOWN");  // resign
    // Note that it is likely that we will be a follower for this shard
    // with another leader in due course. However, we do not know the
    // name of the new leader yet. This setting will make us a follower
    // for now but we will not accept any replication operation from any
    // leader, until we have negotiated a deal with it. Then the actual
    // name of the leader will be set.
    db._executeTransaction(
      { 'collections': { 'write': [shardName] },
        'action': function () { }
      });
  } catch (x) {
    console.topic('heartbeat=error', 'exception thrown when resigning:', x);
  }
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief lock key space
// /////////////////////////////////////////////////////////////////////////////

function lockSyncKeyspace () {
  while (!global.KEY_SET_CAS('shardSynchronization', 'lock', 1, null)) {
    wait(0.001);
  }
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief unlock key space
// /////////////////////////////////////////////////////////////////////////////

function unlockSyncKeyspace () {
  global.KEY_SET('shardSynchronization', 'lock', null);
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief launch a scheduled job if needed
// /////////////////////////////////////////////////////////////////////////////

function tryLaunchJob () {
  const registerTask = require('internal').registerTask;
  var isStopping = require('internal').isStopping;
  if (isStopping()) {
    return;
  }
  var doCleanup = false;
  lockSyncKeyspace();
  try {
    var jobs = global.KEYSPACE_GET('shardSynchronization');
    if (jobs.running === null) {
      var shards = Object.keys(jobs.scheduled).sort();
      if (shards.length > 0) {
        var done = false;
        while (!done) {
          var jobInfo = jobs.scheduled[shards[0]];
          try {
            registerTask({
              database: jobInfo.database,
              params: {database: jobInfo.database, shard: jobInfo.shard,
              planId: jobInfo.planId, leader: jobInfo.leader},
              command: function (params) {
                require('@arangodb/cluster').synchronizeOneShard(
                  params.database, params.shard, params.planId, params.leader);
            }});
            done = true;
            global.KEY_SET('shardSynchronization', 'running', jobInfo);
            console.topic('heartbeat=debug', 'tryLaunchJob: have launched job', jobInfo);
            delete jobs.scheduled[shards[0]];
            global.KEY_SET('shardSynchronization', 'scheduled', jobs.scheduled);
          } catch (err) {
            if (err.errorNum === errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code) {
              doCleanup = true;
              done = true;
            }
            if (!require('internal').isStopping()) {
              console.topic('heartbeat=debug', 'Could not registerTask for shard synchronization.',
                            err);
              wait(1.0);
            } else {
              doCleanup = true;
              done = true;
            }
          }
        }
      }
    }
  }
  finally {
    unlockSyncKeyspace();
  }
  if (doCleanup) {   // database was deleted
    global.KEYSPACE_REMOVE("shardSynchronization");
  }
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief synchronize one shard, this is run as a V8 task
// /////////////////////////////////////////////////////////////////////////////

function synchronizeOneShard (database, shard, planId, leader) {
  // synchronize this shard from the leader
  // this function will throw if anything goes wrong

  var startTime = new Date();
  var isStopping = require('internal').isStopping;
  var ourselves = global.ArangoServerState.id();

  function terminateAndStartOther () {
    lockSyncKeyspace();
    try {
      global.KEY_SET('shardSynchronization', 'running', null);
    }
    finally {
      unlockSyncKeyspace();
    }
    tryLaunchJob(); // start a new one if needed
  }

  // First wait until the leader has created the shard (visible in
  // Current in the Agency) or we or the shard have vanished from
  // the plan:
  while (true) {
    if (isStopping()) {
      terminateAndStartOther();
      return;
    }
    var planned = [];
    try {
      planned = global.ArangoClusterInfo.getCollectionInfo(database, planId)
        .shards[shard];
    } catch (e) {}
    if (!Array.isArray(planned) ||
      planned.indexOf(ourselves) <= 0 ||
      planned[0] !== leader) {
      // Things have changed again, simply terminate:
      terminateAndStartOther();
      let endTime = new Date();
      console.topic('heartbeat=debug', 'synchronizeOneShard: cancelled, %s/%s, %s/%s, started %s, ended %s',
        database, shard, database, planId, startTime.toString(), endTime.toString());
      return;
    }
    var current = [];
    try {
      current = global.ArangoClusterInfo.getCollectionInfoCurrent(
        database, planId, shard).servers;
    } catch (e2) {}
    if (current[0] === leader) {
      if (current.indexOf(ourselves) === -1) {
        break; // start synchronization work
      }
      // We are already there, this is rather strange, but never mind:
      terminateAndStartOther();
      let endTime = new Date();
      console.topic('heartbeat=debug', 'synchronizeOneShard: already done, %s/%s, %s/%s, started %s, ended %s',
        database, shard, database, planId, startTime.toString(), endTime.toString());
      return;
    }
    console.topic('heartbeat=debug', 'synchronizeOneShard: waiting for leader, %s/%s, %s/%s',
      database, shard, database, planId);
    wait(1.0);
  }

  // Once we get here, we know that the leader is ready for sync, so
  // we give it a try:
  var ok = false;
  const rep = require('@arangodb/replication');

  console.topic('heartbeat=debug', "synchronizeOneShard: trying to synchronize local shard '%s/%s' for central '%s/%s'", database, shard, database, planId);
  try {
    var ep = ArangoClusterInfo.getServerEndpoint(leader);
    // First once without a read transaction:
    var sy;
    if (isStopping()) {
      throw 'server is shutting down';
    }

    // Mark us as follower for this leader such that we begin
    // accepting replication operations, note that this is also
    // used for the initial synchronization:
    var db = require("internal").db;
    var collection = db._collection(shard);
    collection.setTheLeader(leader);

    let startTime = new Date();
    sy = rep.syncCollection(shard,
      { endpoint: ep, incremental: true, keepBarrier: true,
        useCollectionId: false, leaderId: leader, skipCreateDrop: true });
    let endTime = new Date();
    let longSync = false;
    if (endTime - startTime > 5000) {
      console.topic('heartbeat=error', 'synchronizeOneShard: long call to syncCollection for shard', shard, JSON.stringify(sy), "start time: ", startTime.toString(), "end time: ", endTime.toString());
      longSync = true;
    }
    if (sy.error) {
      console.topic('heartbeat=error', 'synchronizeOneShard: could not initially synchronize',
        'shard ', shard, sy);
      throw 'Initial sync for shard ' + shard + ' failed';
    } else {
      if (sy.collections.length === 0 ||
        sy.collections[0].name !== shard) {
        if (longSync) {
          console.topic('heartbeat=error', 'synchronizeOneShard: long sync, before cancelBarrier',
                        new Date().toString());
        }
        cancelBarrier(ep, database, sy.barrierId);
        if (longSync) {
          console.topic('heartbeat=error', 'synchronizeOneShard: long sync, after cancelBarrier',
                        new Date().toString());
        }
        throw 'Shard ' + shard + ' seems to be gone from leader!';
      } else {
        // Now start a read transaction to stop writes:
        var lockJobId = false;
        try {
          lockJobId = startReadLockOnLeader(ep, database,
            shard, 300);
          console.topic('heartbeat=debug', 'lockJobId:', lockJobId);
        } catch (err1) {
          console.topic('heartbeat=error', 'synchronizeOneShard: exception in startReadLockOnLeader:', err1, err1.stack);
        }
        finally {
          cancelBarrier(ep, database, sy.barrierId);
        }
        if (lockJobId !== false) {
          try {
            var sy2 = rep.syncCollectionFinalize(
              database, shard, sy.lastLogTick, { endpoint: ep }, leader);
            if (sy2.error) {
              console.topic('heartbeat=error', 'synchronizeOneShard: Could not finalize shard synchronization',
                shard, sy2);
              ok = false;
            } else {
              try {
                ok = addShardFollower(ep, database, shard, lockJobId);
              } catch (err4) {
                db._drop(shard);
                throw err4;
              }
            }
          } catch (err3) {
            console.topic('heartbeat=error', 'synchronizeOneshard: exception in',
              'syncCollectionFinalize:', err3);
          }
          finally {
            if (!cancelReadLockOnLeader(ep, database, lockJobId)) {
              console.topic('heartbeat=error', 'synchronizeOneShard: read lock has timed out',
                'for shard', shard);
              ok = false;
            }
          }
        } else {
          console.topic('heartbeat=error', 'synchronizeOneShard: lockJobId was false for shard',
            shard);
        }
        if (ok) {
          console.topic('heartbeat=debug', 'synchronizeOneShard: synchronization worked for shard',
            shard);
        } else {
          throw 'Did not work for shard ' + shard + '.';
        // just to log below in catch
        }
      }
    }
  } catch (err2) {
    if (!isStopping()) {
      let logLevel = 'error';
      // ignore failures of jobs where the database to sync has been removed on the leader
      // {"errorNum":1400,"errorMessage":"cannot sync from remote endpoint: job not found on master at tcp://127.0.0.1:15179. last progress message was 'send batch finish command to url /_api/replication/batch/2103395': no response"}
      if (err2 && (err2.errorNum === 1203 || err2.errorNum === 1400)) {
        logLevel = 'debug';
      } else if (err2 && err2.errorNum === 1402 && err2.errorMessage.match(/HTTP 404/)) {
        logLevel = 'debug';
      }
      let endTime = new Date();
      console.topic('heartbeat='+logLevel, "synchronization of local shard '%s/%s' for central '%s/%s' failed: %s, started: %s, ended: %s",
        database, shard, database, planId, JSON.stringify(err2),
        startTime.toString(), endTime.toString());
    }
  }
  // Tell others that we are done:
  terminateAndStartOther();
  let endTime = new Date();
  console.topic('heartbeat=debug', 'synchronizeOneShard: done, %s/%s, %s/%s, started: %s, ended: %s',
    database, shard, database, planId, startTime.toString(), endTime.toString());
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief schedule a shard synchronization
// /////////////////////////////////////////////////////////////////////////////

function scheduleOneShardSynchronization (database, shard, planId, leader) {
  console.topic('heartbeat=debug', 'scheduleOneShardSynchronization:', database, shard, planId,
    leader);

  lockSyncKeyspace();
  try {
    var jobs = global.KEYSPACE_GET('shardSynchronization');
    if ((jobs.running !== null && jobs.running.shard === shard) ||
      jobs.scheduled.hasOwnProperty(shard)) {
      console.topic('heartbeat=debug', 'task is already running or scheduled,',
        'ignoring scheduling request');
      return false;
    }

    // If we reach this, we actually have to schedule a new task:
    var jobInfo = { database, shard, planId, leader};
    jobs.scheduled[shard] = jobInfo;
    global.KEY_SET('shardSynchronization', 'scheduled', jobs.scheduled);
    console.topic('heartbeat=debug', 'scheduleOneShardSynchronization: have scheduled job', jobInfo);
  }
  finally {
    unlockSyncKeyspace();
  }
  return true;
}

function createIndexes(collection, plannedIndexes) {
  let existingIndexes = collection.getIndexes();

  let localId = plannedIndex => {
    return collection.name() + '/' + plannedIndex.id;
  };

  let clusterId = existingIndex => {
    return existingIndex.split('/')[1];
  };

  let findExisting = index => {
    return existingIndexes.filter(existingIndex => {
      return existingIndex.id === localId(index);
    })[0];
  };
  let errors = plannedIndexes.reduce((errors, plannedIndex) => {
    if (plannedIndex.type !== 'primary' && plannedIndex.type !== 'edge') {
      if (findExisting(plannedIndex)) {
        return errors;
      }
      try {
        console.topic('heartbeat=debug', "creating index '%s/%s': %s",
          collection._dbName,
          collection.name(),
          JSON.stringify(plannedIndex));
        collection.ensureIndex(plannedIndex);
      } catch (e) {
        errors[plannedIndex.id] = {
          errorNum: e.errorNum,
          errorMessage: e.errorMessage,
        };
      };
    }
    return errors;
  }, {});

  let indexesToDelete = existingIndexes.filter(index => {
    if (index.type === 'primary' || index.type === 'edge') {
      return false;
    }
    return plannedIndexes.filter(plannedIndex => {
      return localId(plannedIndex) === index.id;
    }).length === 0;
  });

  return indexesToDelete.reduce((errors, index) => {
    console.topic('heartbeat=debug', "dropping index '%s/%s': %s",
      collection._dbName,
      collection.name(),
      JSON.stringify(index));
    if (!collection.dropIndex(index)) {
      errors[clusterId(index)] = {
        errorNum: 4,
        errorMessage: 'could not delete index locally',
      };
    }
    return errors;
  }, errors);
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief executePlanForCollections
// /////////////////////////////////////////////////////////////////////////////

function executePlanForCollections(plannedCollections) {
  let ourselves = global.ArangoServerState.id();
  let localErrors = {};

  let db = require('internal').db;
  db._useDatabase('_system');

  let createShardError = function(errors, database, planId, shardName) {
    if (Object.keys(errors).length > 0) {
      let fullError = {};
      fullError[shardName] = {
        info: {database, planId, shardName},
      };
      fullError[shardName] = Object.assign(fullError[shardName], errors);
      return fullError;
    } else {
      return errors;
    }
  };

  let localDatabases = getLocalDatabases();
  // Create shards in Plan that are not there locally:
  Object.keys(plannedCollections).forEach(database => {
    if (localDatabases.hasOwnProperty(database)) {
      // switch into other database
      db._useDatabase(database);

      try {
        // iterate over collections of database
        let localCollections = getLocalCollections();
        let collections = plannedCollections[database];

        // diff the collections
        Object.keys(collections).forEach(function (collectionName) {
          let collectionInfo = collections[collectionName];
          let shards = collectionInfo.shards;

          collectionInfo.planId = collectionInfo.id;
          localErrors = Object.keys(shards).reduce((errors, shardName) => {
            let shardErrors = {};
            let plannedServers = shards[shardName];
            if (plannedServers.indexOf(ourselves) >= 0) {
              let shouldBeLeader = plannedServers[0] === ourselves;

              let collectionStatus;
              let collection;
              if (!localCollections.hasOwnProperty(shardName)) {
                // must create this shard
                console.topic('heartbeat=debug', "creating local shard '%s/%s' for central '%s/%s'",
                  database,
                  shardName,
                  database,
                  collectionInfo.planId);

                let save = {id: collectionInfo.id, name: collectionInfo.name};
                delete collectionInfo.id;     // must not
                delete collectionInfo.name;
                try {
                  if (collectionInfo.type === ArangoCollection.TYPE_EDGE) {
                    db._createEdgeCollection(shardName, collectionInfo);
                  } else {
                    db._create(shardName, collectionInfo);
                  }
                } catch (err2) {
                  shardErrors.collection = {
                    errorNum: err2.errorNum,
                    errorMessage: err2.errorMessage,
                  };
                  console.topic('heartbeat=error', "creating local shard '%s/%s' for central '%s/%s' failed: %s",
                    database,
                    shardName,
                    database,
                    collectionInfo.planId,
                    JSON.stringify(err2));
                  return Object.assign(errors, createShardError(shardErrors, database, collectionInfo.planId, shardName));
                }
                collectionInfo.id = save.id;
                collectionInfo.name = save.name;
                collection = db._collection(shardName);
                if (shouldBeLeader) {
                  collection.setTheLeader("");   // take power
                } else {
                  collection.setTheLeader(plannedServers[0]);
                }
                collectionStatus = ArangoCollection.STATUS_LOADED;
              } else {
                collection = db._collection(shardName);
                // We adjust local leadership, note that the planned
                // resignation case is not handled here, since then
                // ourselves does not appear in shards[shard] but only
                // "_" + ourselves. We adjust local leadership, note
                // that the planned resignation case is not handled
                // here, since then ourselves does not appear in
                // shards[shard] but only "_" + ourselves. See below
                // under "Drop local shards" to see the proper handling
                // of this case. Place is marked with *** in comments.

                if (shouldBeLeader) {
                  if (localCollections[shardName].theLeader !== "") {
                    collection.setTheLeader("");  // assume leadership
                  } else {
                    // If someone (the Supervision most likely) has thrown
                    // out a follower from the plan, then the leader
                    // will not notice until it fails to replicate an operation
                    // to the old follower. This here is to drop such a follower
                    // from the local list of followers. Will be reported
                    // to Current in due course. This is not needed for 
                    // correctness but is a performance optimization.
                    let currentFollowers = collection.getFollowers();
                    let plannedFollowers = plannedServers.slice(1);
                    let removedFollowers = currentFollowers.filter(follower => {
                      return plannedServers.indexOf(follower) === -1;
                    });
                    removedFollowers.forEach(removedFollower => {
                      collection.removeFollower(removedFollower);
                    });
                  }
                } else {   // !shouldBeLeader
                  if (localCollections[shardName].theLeader === "") {
                    // Note that the following does not delete the follower list
                    // and that this is crucial, because in the planned leader 
                    // resign case, updateCurrentForCollections will report the
                    // resignation together with the old in-sync list to the
                    // agency. If this list would be empty, then the supervision
                    // would be very angry with us!
                    collection.setTheLeader(plannedServers[0]);
                  }
                  // Note that if we have been a follower to some leader
                  // we do not immediately adjust the leader here, even if
                  // the planned leader differs from what we have set locally.
                  // The setting must only be adjusted once we have
                  // synchronized with the new leader and negotiated
                  // a leader/follower relationship!
                }

                collectionStatus = localCollections[shardName].status;

                // collection exists, now compare collection properties
                let cmp = [ 'journalSize', 'waitForSync', 'doCompact',
                  'indexBuckets' ];

                let properties = cmp.reduce((obj, key) => {
                  if (localCollections[shardName].hasOwnProperty(key) &&
                      localCollections[shardName][key] !== collectionInfo[key]) {
                    // property change
                    obj[key] = collectionInfo[key];
                  }
                  return obj;
                }, {});

                if (Object.keys(properties).length > 0) {
                  console.topic('heartbeat=debug', "updating properties for local shard '%s/%s'",
                    database,
                    shardName);

                  try {
                    collection.properties(properties);
                  } catch (err3) {
                    shardErrors.collection = {
                      errorNum: err3.errorNum,
                      errorMessage: err3.errorMessage,
                    };
                    return Object.assign(errors, createShardError(shardErrors, database, collectionInfo.planId, shardName));
                  }
                }
              }

              // Now check whether the status is OK:
              if (collectionStatus !== collectionInfo.status) {
                console.topic('heartbeat=debug', "detected status change for local shard '%s/%s'",
                  database,
                  shardName);

                if (collectionInfo.status === ArangoCollection.STATUS_UNLOADED) {
                  console.topic('heartbeat=debug', "unloading local shard '%s/%s'",
                    database,
                    shardName);

                  collection.unload();
                } else if (collectionInfo.status === ArangoCollection.STATUS_LOADED) {
                  console.topic('heartbeat=debug', "loading local shard '%s/%s'",
                    database,
                    shardName);
                  collection.load();
                }
              }

              let indexErrors = createIndexes(collection, collectionInfo.indexes || {});
              if (Object.keys(indexErrors).length > 0) {
                shardErrors.indexErrors = indexErrors;
              }
            }
            return Object.assign(errors, createShardError(shardErrors, database, collectionInfo.planId, shardName));
          }, localErrors);
        });
      } catch(e) {
        console.topic('heartbeat=error', "Got error executing plan", e, e.stack);
      } finally {
        // always return to previous database
        db._useDatabase('_system');
      }
    }
  });
  // Drop local shards that do no longer exist in Plan:
  let shardMap = getShardMap(plannedCollections);

  // iterate over all databases
  Object.keys(localDatabases).forEach(database => {
    let removeAll = !plannedCollections.hasOwnProperty(database);

    // switch into other database
    db._useDatabase(database);
    try {
      // iterate over collections of database
      let collections = getLocalCollections();

      Object.keys(collections).forEach(collection => {
        // found a local collection
        // check if it is in the plan and we are responsible for it
        if (removeAll ||
            !shardMap.hasOwnProperty(collection) ||
            shardMap[collection].indexOf(ourselves) === -1) {

          // May be we have been the leader and are asked to withdraw: ***
          if (shardMap.hasOwnProperty(collection) &&
              shardMap[collection][0] === '_' + ourselves) {
            if (collections[collection].theLeader === "") {
              organiseLeaderResign(database, collections[collection].planId,
                collection);
            }
          } else {
            if (collections[collection].theLeader !== "") {
              // Remove us from the follower list, this is a best
              // effort: If an error occurs, this is no problem, since
              // the leader will soon notice that the shard here is
              // gone and will drop us automatically:
              console.topic('heartbeat=debug', "removing local shard '%s/%s' of '%s/%s' from follower list",
                            database, collection, database,
                            collections[collection].planId);
              let servers = shardMap[collection];
              if (servers !== undefined) {
                let endpoint = ArangoClusterInfo.getServerEndpoint(servers[0]);
                try {
                  removeShardFollower(endpoint, database, collection);
                } catch (err) {
                  console.topic('heartbeat=debug', "caught exception during removal of local shard '%s/%s' of '%s/%s' from follower list",
                                database, collection, database,
                                collections[collection].planId, err);
                }
              }
            }
            console.topic('heartbeat=debug', "dropping local shard '%s/%s' of '%s/%s",
              database,
              collection,
              database,
              collections[collection].planId);

            try {
              db._drop(collection, {timeout:1.0});
              console.topic('heartbeat=debug', "dropping local shard '%s/%s' of '%s/%s => SUCCESS",
                    database,
                    collection,
                    database,
                    collections[collection].planId);
            }
            catch (err) {
              console.topic('heartbeat=debug', "could not drop local shard '%s/%s' of '%s/%s within 1 second, trying again later",
                database,
                collection,
                database,
                collections[collection].planId);
            }
          }
        }
      });
    } finally {
      db._useDatabase('_system');
    }
  });

  return localErrors;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief updateCurrentForCollections
// /////////////////////////////////////////////////////////////////////////////

function updateCurrentForCollections(localErrors, plannedCollections,
                                                  currentCollections) {
  let ourselves = global.ArangoServerState.id();

  let db = require('internal').db;
  db._useDatabase('_system');

  let localDatabases = getLocalDatabases();
  let database;

  let shardMap = getShardMap(plannedCollections);

  function assembleLocalCollectionInfo(info, error) {
    if (error.collection) {
      return {
        error: true,
        errorMessage: error.collection.errorMessage,
        errorNum: error.collection.errorNum,
        indexes: [],
        servers: [ourselves],
      };
    }
    let coll = db._collection(info.name);

    let indexes = coll.getIndexes().map(index => {
      let agencyIndex = {};
      Object.assign(agencyIndex, index);
      // Fix up the IDs of the indexes:
      let pos = index.id.indexOf("/");
      if (agencyIndex.hasOwnProperty("selectivityEstimate")) {
        delete agencyIndex.selectivityEstimate;
      }
      if (pos >= 0) {
        agencyIndex.id = index.id.slice(pos+1);
      } else {
        agencyIndex.id = index.id;
      }
      return agencyIndex;
    });

    if (error.indexErrors) {
      indexes = indexes.concat(Object.keys(error.indexErrors).map(id => {
        let indexError = error.indexErrors[id];
        return {
          id,
          error: true,
          errorMessage: indexError.errorMessage,
          errorNum: indexError.errorNum,
        };
      }));
    }

    return {
      error: false,
      errorMessage: '',
      errorNum: 0,
      indexes,
      servers: [ourselves].concat(coll.getFollowers()),
    };
  }

  function makeDropCurrentEntryCollection(dbname, col, shard) {
    let trx = {};
    trx[curCollections + dbname + '/' + col + '/' + shard] =
      {op: 'delete'};
    return trx;
  }

  let trx = {};

  // Go through local databases and collections and add stuff to Current
  // as needed:
  Object.keys(localDatabases).forEach(database => {
    // All local databases should be in Current by now, if not, we ignore
    // it, this will happen later.
    try {
      db._useDatabase(database);

      // iterate over collections (i.e. shards) of database
      let localCollections = getLocalCollections();
      let shard;
      for (shard in localCollections) {
        if (localCollections.hasOwnProperty(shard)) {
          let shardInfo = localCollections[shard];
          if (shardInfo.theLeader === "") {
            let localCollectionInfo = assembleLocalCollectionInfo(shardInfo, localErrors[shard] || {});
            let currentCollectionInfo = fetchKey(currentCollections, database, shardInfo.planId, shard);

            if (!isEqual(localCollectionInfo, currentCollectionInfo)) {
              trx[curCollections + database + '/' + shardInfo.planId + '/' + shardInfo.name] = {
                op: 'set',
                new: localCollectionInfo,
              };
            }
          } else {
            let currentServers = fetchKey(currentCollections, database, shardInfo.planId, shard, 'servers');
            // we were previously leader and we are done resigning. update current and let supervision handle the rest
            if (Array.isArray(currentServers) && currentServers[0] === ourselves) {
              trx[curCollections + database + '/' + shardInfo.planId + '/' + shardInfo.name + '/servers'] = {
                op: 'set',
                new: ['_' + ourselves].concat(db._collection(shardInfo.name).getFollowers()),
              };
            }
          }
          // mark error as handled in any case
          delete localErrors[shard];
        }
      }
    } catch (e) {
      console.topic('heartbeat=error', 'Got error while trying to sync current collections:', e, e.stack);
    } finally {
      // always return to previous database
      db._useDatabase('_system');
    }
  });

  // Go through all current databases and collections and remove stuff
  // if no longer present locally, provided :
  for (database in currentCollections) {
    if (currentCollections.hasOwnProperty(database)) {
      if (localDatabases.hasOwnProperty(database)) {
        // If a database has vanished locally, it is not our job to
        // remove it in Current, that is what `updateCurrentForDatabases`
        // does.
        db._useDatabase(database);

        try {
          // iterate over collections (i.e. shards) of database in Current
          let localCollections = getLocalCollections();
          let collection;
          for (collection in currentCollections[database]) {
            if (currentCollections[database].hasOwnProperty(collection)) {
              let shard;
              for (shard in currentCollections[database][collection]) {
                if (currentCollections[database][collection].hasOwnProperty(shard)) {
                  let cur = currentCollections[database][collection][shard];
                  if (!localCollections.hasOwnProperty(shard) &&
                      cur.servers[0] === ourselves &&
                      !shardMap.hasOwnProperty(shard)) {
                    Object.assign(trx, makeDropCurrentEntryCollection(database, collection, shard));
                  }
                }
              }
            }
          }
        } finally {
          // always return to previous database
          db._useDatabase('_system');
        }
      }
    }
  }
  trx = Object.keys(localErrors).reduce((trx, shardName) => {
    let error = localErrors[shardName];
    if (error.collection) {
      trx[curCollections + error.info.database + '/' + error.info.planId + '/' + error.info.shardName] = {
        op: 'set',
        new: error.collection,
      };
    }
    return trx;
  }, trx);
  return trx;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief syncReplicatedShardsWithLeaders
// /////////////////////////////////////////////////////////////////////////////

function syncReplicatedShardsWithLeaders(plan, current, localErrors) {
  let plannedDatabases = plan.Collections;
  let currentDatabases = current.Collections;
  let ourselves = global.ArangoServerState.id();

  let db = require('internal').db;
  db._useDatabase('_system');

  let localDatabases = getLocalDatabases();

  // Schedule sync tasks for shards which exist and we should be a follower:
  Object.keys(plannedDatabases).forEach(databaseName => {
    if (localDatabases.hasOwnProperty(databaseName)
      && currentDatabases.hasOwnProperty(databaseName)) {
        // switch into other database
        db._useDatabase(databaseName);
        // XXX shouldn't this be done during db init?
        // create keyspace
        try {
          global.KEY_GET('shardSynchronization', 'lock');
        } catch (e) {
          global.KEYSPACE_CREATE('shardSynchronization');
          global.KEY_SET('shardSynchronization', 'scheduled', {});
          global.KEY_SET('shardSynchronization', 'running', null);
          global.KEY_SET('shardSynchronization', 'lock', null);
        }

        try {
          // iterate over collections of database
          let localCollections = getLocalCollections();

          let plannedCollections = plannedDatabases[databaseName];
          let currentCollections = currentDatabases[databaseName];

          // find planned collections that need sync (not registered in current by the leader):
          Object.keys(plannedCollections).forEach(collectionName => {
            let plannedCollection = plannedCollections[collectionName];
            let currentShards = currentCollections[collectionName];
            // what should it bring
            // collection.planId = collection.id;
            if (currentShards !== undefined) {
              let plannedShards = plannedCollection.shards;
              Object.keys(plannedShards).forEach(shardName => {
                // shard does not exist locally so nothing we can do at this point
                if (!localCollections.hasOwnProperty(shardName)) {
                  return;
                }
                // current stuff is created by the leader
                // this one here will just bring followers in sync
                // so just continue here
                if (!currentShards.hasOwnProperty(shardName)) {
                  return;
                }
                let currentServers = currentShards[shardName].servers;
                let plannedServers = plannedShards[shardName];
                if (!plannedServers) {
                  console.topic('heartbeat=error', 'Shard ' + shardName + ' does not have servers substructure in plan');
                  return;
                }
                if (!currentServers) {
                  console.topic('heartbeat=error', 'Shard ' + shardName + ' does not have servers substructure in current');
                  return;
                }
                
                // we are not planned to be a follower
                if (plannedServers.indexOf(ourselves) <= 0) {
                  return;
                }
                // if we are considered to be in sync there is nothing to do
                if (currentServers.indexOf(ourselves) > 0) {
                  return;
                }

                let leader = plannedServers[0];
                scheduleOneShardSynchronization(databaseName, shardName, plannedCollection.id, leader);
              });
            }
          });
        } catch (e) {
          console.topic('heartbeat=debug', 'Got an error synchronizing with leader', e, e.stack);
        } finally {
          // process any jobs
          tryLaunchJob();
          // always return to previous database
          db._useDatabase('_system');
        }
      }
  });
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief take care of collections on primary DBservers according to Plan
// /////////////////////////////////////////////////////////////////////////////

function migratePrimary(plan, current) {
  // will analyze local state and then issue db._create(),
  // db._drop() etc. to sync plan and local state for shards
  let localErrors = executePlanForCollections(plan.Collections);

  // diff current and local and prepare agency transactions or whatever
  // to update current. Will report the errors created locally to the agency
  let trx = updateCurrentForCollections(localErrors, plan.Collections,
                                                     current.Collections);
  if (Object.keys(trx).length > 0) {
    trx[curVersion] = {op: 'increment'};
    trx = [trx];
    // TODO: reduce timeout when we can:
    try {
      let res = global.ArangoAgency.write([trx]);
      if (typeof res !== 'object' || !res.hasOwnProperty("results") ||
          typeof res.results !== 'object' || res.results.length !== 1 || 
          res.results[0] === 0) {
        console.topic('heartbeat=error', 'migratePrimary: could not send transaction for Current to agency, result:', res);
      }
    } catch (err) {
      console.topic('heartbeat=error', 'migratePrimary: caught exception when sending transaction for Current to agency:', err);
    }
  }

  // will do a diff between plan and current to find out
  // the shards for which this db server is a planned
  // follower. Background jobs for this activity are scheduled.
  // This will then supervise any actions necessary
  // to bring the shard into sync and ultimately register
  // at the leader using addFollower
  // this step should assume that the local state matches the
  // plan...can NOT be sure that the plan was completely executed
  // may react on the errors that have been created
  syncReplicatedShardsWithLeaders(plan, current, localErrors);
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief executePlanForDatabases
// /////////////////////////////////////////////////////////////////////////////

function executePlanForDatabases(plannedDatabases) {
  let localErrors = {};

  let db = require('internal').db;
  db._useDatabase('_system');

  let localDatabases = getLocalDatabases();
  let name;

  // check which databases need to be created locally:
  Object.keys(plannedDatabases).forEach(name => {
    if (!localDatabases.hasOwnProperty(name)) {
      // must create database

      // TODO: handle options and user information

      console.topic('heartbeat=debug', "creating local database '%s'", name);

      try {
        db._createDatabase(name);
      } catch (err) {
        localErrors[name] = { error: true, errorNum: err.errorNum,
                              errorMessage: err.errorMessage, name: name };
      }
    }
  });

  // check which databases need to be deleted locally
  localDatabases = getLocalDatabases();

  Object.keys(localDatabases).forEach(name => {
    if (!plannedDatabases.hasOwnProperty(name) && name.substr(0, 1) !== '_') {
      // must drop database

      console.topic('heartbeat=debug', "dropping local database '%s'", name);

      // Do we have to stop a replication applier first?
      if (ArangoServerState.role() === 'SECONDARY') {
        try {
          db._useDatabase(name);
          var rep = require('@arangodb/replication');
          var state = rep.applier.state();
          if (state.state.running === true) {
            console.topic('heartbeat=debug', 'stopping replication applier first');
            rep.applier.stop();
          }
        }
        finally {
          db._useDatabase('_system');
        }
      }
      db._dropDatabase(name);
    }
  });

  return localErrors;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief updateCurrentForDatabases
// /////////////////////////////////////////////////////////////////////////////

function updateCurrentForDatabases(localErrors, currentDatabases) {
  let ourselves = global.ArangoServerState.id();

  function makeAddDatabaseAgencyOperation(payload) {
    let create = {};
    create[curDatabases + payload.name + '/' + ourselves] =
      {op: 'set', new: payload};
    return create;
  };

  function makeDropDatabaseAgencyOperation(name) {
    let drop = {};
    drop[curDatabases + name + '/' + ourselves] = {'op':'delete'};
    return drop;
  };

  let db = require('internal').db;
  db._useDatabase('_system');

  let localDatabases = getLocalDatabases();
  let name;
  let trx = {};   // Here we collect all write operations

  // Add entries that we have but that are not in Current:
  for (name in localDatabases) {
    if (localDatabases.hasOwnProperty(name)) {
      if (!currentDatabases.hasOwnProperty(name) ||
          !currentDatabases[name].hasOwnProperty(ourselves) ||
          currentDatabases[name][ourselves].error) {
        console.topic('heartbeat=debug', "adding entry in Current for database '%s'", name);
        trx = Object.assign(trx, makeAddDatabaseAgencyOperation({error: false, errorNum: 0, name: name,
                                        id: localDatabases[name].id,
                                        errorMessage: ""}));
      }
    }
  }

  // Remove entries from current that no longer exist locally: 
  for (name in currentDatabases) {
    if (currentDatabases.hasOwnProperty(name)
      && name.substr(0, 1) !== '_'
      && localErrors[name] === undefined
    ) {
      if (!localDatabases.hasOwnProperty(name)) {
        // we found a database we don't have locally

        if (currentDatabases[name].hasOwnProperty(ourselves)) {
          // we are entered for a database that we don't have locally
          console.topic('heartbeat=debug', "cleaning up entry for unknown database '%s'", name);
          trx = Object.assign(trx, makeDropDatabaseAgencyOperation(name));
        }
      }
    }
  }

  // Finally, report any errors that might have been produced earlier when
  // we were trying to execute the Plan:
  Object.keys(localErrors).forEach(name => {
    console.topic('heartbeat=debug', "reporting error to Current about database '%s'", name);
    trx = Object.assign(trx, makeAddDatabaseAgencyOperation(localErrors[name]));
  });

  return trx;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief take care of databases on any type of server according to Plan
// /////////////////////////////////////////////////////////////////////////////

function migrateAnyServer(plan, current) {
  // will analyze local state and then issue db._createDatabase(),
  // db._dropDatabase() etc. to sync plan and local state for databases
  let localErrors = executePlanForDatabases(plan.Databases);
  // diff current and local and prepare agency transactions or whatever
  // to update current. will report the errors created locally to the agency
  let trx = updateCurrentForDatabases(localErrors, current.Databases);
  if (Object.keys(trx).length > 0) {
    trx[curVersion] = {op: 'increment'};
    trx = [trx];
    // TODO: reduce timeout when we can:
    try {
      let res = global.ArangoAgency.write([trx]);
      if (typeof res !== 'object' || !res.hasOwnProperty("results") ||
          typeof res.results !== 'object' || res.results.length !== 1 || 
          res.results[0] === 0) {
        console.topic('heartbeat=error', 'migrateAnyServer: could not send transaction for Current to agency, result:', res);
      }
    } catch (err) {
      console.topic('heartbeat=error', 'migrateAnyServer: caught exception when sending transaction for Current to agency:', err);
    }
  }
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief make sure that replication is set up for all databases
// /////////////////////////////////////////////////////////////////////////////

function setupReplication () {
  console.topic('heartbeat=debug', 'Setting up replication...');

  var db = require('internal').db;
  var rep = require('@arangodb/replication');
  var dbs = db._databases();
  var i;
  var ok = true;
  for (i = 0; i < dbs.length; i++) {
    var database = dbs[i];
    try {
      console.topic('heartbeat=debug', 'Checking replication of database ' + database);
      db._useDatabase(database);

      var state = rep.applier.state();
      if (state.state.running === false) {
        var endpoint = ArangoClusterInfo.getServerEndpoint(
          ArangoServerState.idOfPrimary());
        var config = { 'endpoint': endpoint, 'includeSystem': false,
          'incremental': false, 'autoStart': true,
        'requireFromPresent': true};
        console.topic('heartbeat=debug', 'Starting synchronization...');
        var res = rep.sync(config);
        console.topic('heartbeat=debug', 'Last log tick: ' + res.lastLogTick +
          ', starting replication...');
        rep.applier.properties(config);
        var res2 = rep.applier.start(res.lastLogTick);
        console.topic('heartbeat=debug', 'Result of replication start: ' + res2);
      }
    } catch (err) {
      console.topic('heartbeat=error', 'Could not set up replication for database ', database, JSON.stringify(err));
      ok = false;
    }
  }
  db._useDatabase('_system');
  return ok;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief role change from secondary to primary
// /////////////////////////////////////////////////////////////////////////////

function secondaryToPrimary () {
  console.topic('heartbeat=info', 'Switching role from secondary to primary...');
  var db = require('internal').db;
  var rep = require('@arangodb/replication');
  var dbs = db._databases();
  var i;
  try {
    for (i = 0; i < dbs.length; i++) {
      var database = dbs[i];
      console.topic('heartbeat=info', 'Stopping asynchronous replication for db ' +
        database + '...');
      db._useDatabase(database);
      var state = rep.applier.state();
      if (state.state.running === true) {
        try {
          rep.applier.stop();
        } catch (err) {
          console.topic('heartbeat=info', 'Exception caught whilst stopping replication!');
        }
      }
      rep.applier.forget();
    }
  }
  finally {
    db._useDatabase('_system');
  }
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief role change from primary to secondary
// /////////////////////////////////////////////////////////////////////////////

function primaryToSecondary () {
  console.topic('heartbeat=info', 'Switching role from primary to secondary...');
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief change handling trampoline function
// /////////////////////////////////////////////////////////////////////////////

function handleChanges (plan, current) {
  // Note: This is never called with role === 'COORDINATOR' or on a single
  // server.
  var changed = false;
  var role = ArangoServerState.role();
  // Need to check role change for automatic failover:
  var myId = ArangoServerState.id();
  if (role === 'PRIMARY') {
    if (!plan.DBServers[myId]) {
      // Ooops! We do not seem to be a primary any more!
      changed = ArangoServerState.redetermineRole();
    }
  } else { // role === "SECONDARY"
    if (plan.DBServers[myId]) {
      changed = ArangoServerState.redetermineRole();
      if (!changed) {
        // mop: oops...changing role has failed. retry next time.
        return false;
      }
    } else {
      var found = null;
      var p;
      for (p in plan) {
        if (plan.hasOwnProperty(p) && plan[p] === myId) {
          found = p;
          break;
        }
      }
      if (found !== ArangoServerState.idOfPrimary()) {
        // Note this includes the case that we are not found at all!
        changed = ArangoServerState.redetermineRole();
      }
    }
  }
  var oldRole = role;
  if (changed) {
    role = ArangoServerState.role();
    console.topic('heartbeat=info', 'Our role has changed to ' + role);
    if (oldRole === 'SECONDARY' && role === 'PRIMARY') {
      secondaryToPrimary();
    } else if (oldRole === 'PRIMARY' && role === 'SECONDARY') {
      primaryToSecondary();
    }
  }

  migrateAnyServer(plan, current);
  if (role === 'PRIMARY') {
    migratePrimary(plan, current);
  } else {   // if (role == 'SECONDARY') {
    setupReplication();
  }

  return true;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief throw an ArangoError
// /////////////////////////////////////////////////////////////////////////////

var raiseError = function (code, msg) {
  var err = new ArangoError();
  err.errorNum = code;
  err.errorMessage = msg;

  throw err;
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief retrieve a list of shards for a collection
// /////////////////////////////////////////////////////////////////////////////

var shardList = function (dbName, collectionName) {
  let ci = global.ArangoClusterInfo.getCollectionInfo(dbName, collectionName);

  if (ci === undefined || typeof ci !== 'object') {
    throw "unable to determine shard list for '" + dbName + '/' + collectionName + "'";
  }

  let shards = [];
  for (let shard in ci.shards) {
    if (ci.shards.hasOwnProperty(shard)) {
      shards.push(shard);
    }
  }

  if (shards.length === 0 && isEnterprise) {
    if (isEnterprise) {
      return require('@arangodb/clusterEE').getSmartShards(dbName, collectionName, ci);
    } else {
      raiseError(arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code,
        arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message);
    }
  }

  return shards;
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief wait for a distributed response
// /////////////////////////////////////////////////////////////////////////////

var waitForDistributedResponse = function (data, numberOfRequests, ignoreHttpErrors) {
  var received = [];
  try {
    while (received.length < numberOfRequests) {
      var result = global.ArangoClusterComm.wait(data);
      var status = result.status;

      if (status === 'ERROR') {
        raiseError(arangodb.errors.ERROR_INTERNAL.code,
          'received an error from a DB server: ' + JSON.stringify(result));
      } else if (status === 'TIMEOUT') {
        raiseError(arangodb.errors.ERROR_CLUSTER_TIMEOUT.code,
          arangodb.errors.ERROR_CLUSTER_TIMEOUT.message);
      } else if (status === 'DROPPED') {
        raiseError(arangodb.errors.ERROR_INTERNAL.code,
          'the operation was dropped');
      } else if (status === 'RECEIVED') {
        received.push(result);

        if (result.headers && result.headers.hasOwnProperty('x-arango-response-code')) {
          var code = parseInt(result.headers['x-arango-response-code'].substr(0, 3), 10);
          result.statusCode = code;

          if (code >= 400 && !ignoreHttpErrors) {
            var body;

            try {
              body = JSON.parse(result.body);
            } catch (err) {
              raiseError(arangodb.errors.ERROR_INTERNAL.code,
                'error parsing JSON received from a DB server: ' + err.message);
            }

            raiseError(body.errorNum,
              body.errorMessage);
          }
        }
      } else {
        // something else... wait without GC
        require('internal').wait(0.1, false);
      }
    }
  } finally {
    global.ArangoClusterComm.drop(data);
  }
  return received;
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief whether or not clustering is enabled
// /////////////////////////////////////////////////////////////////////////////

var isCluster = function () {
  var role = global.ArangoServerState.role();

  return (role !== undefined && role !== 'SINGLE' && role !== 'AGENT');
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief whether or not we are a coordinator
// /////////////////////////////////////////////////////////////////////////////

var isCoordinator = function () {
  return global.ArangoServerState.isCoordinator();
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief role
// /////////////////////////////////////////////////////////////////////////////

var role = function () {
  var role = global.ArangoServerState.role();

  if (role !== 'SINGLE') {
    return role;
  }
  return undefined;
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief status
// /////////////////////////////////////////////////////////////////////////////

var status = function () {
  if (!isCluster() || !global.ArangoServerState.initialized()) {
    return undefined;
  }

  return global.ArangoServerState.status();
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief isCoordinatorRequest
// /////////////////////////////////////////////////////////////////////////////

var isCoordinatorRequest = function (req) {
  if (!req || !req.hasOwnProperty('headers')) {
    return false;
  }

  return req.headers.hasOwnProperty('x-arango-coordinator');
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief handlePlanChange
// /////////////////////////////////////////////////////////////////////////////

var handlePlanChange = function (plan, current) {
  // This is never called on a coordinator, we still make sure that it
  // is not executed on a single server or coordinator, just to be sure:
  if (!isCluster() || isCoordinator() || !global.ArangoServerState.initialized()) {
    return true;
  }

  let versions = {
    plan: plan.Version,
    current: current.Version
  };

  console.topic('heartbeat=debug', 'handlePlanChange:', plan.Version, current.Version);
  try {
    versions.success = handleChanges(plan, current);

    console.topic('heartbeat=debug', 'plan change handling successful');
  } catch (err) {
    console.topic('heartbeat=error', 'error details: %s', JSON.stringify(err));
    console.topic('heartbeat=error', 'error stack: %s', err.stack);
    console.topic('heartbeat=error', 'plan change handling failed');
    versions.success = false;
  }
  console.topic('heartbeat=debug', 'handlePlanChange: done');
  return versions;
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief coordinatorId
// /////////////////////////////////////////////////////////////////////////////

var coordinatorId = function () {
  if (!isCoordinator()) {
    console.topic('heartbeat=error', 'not a coordinator');
  }
  return global.ArangoServerState.id();
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief shard distribution
// /////////////////////////////////////////////////////////////////////////////

function format (x) {
  var r = {};
  var keys = Object.keys(x);
  for (var i = 0; i < keys.length; ++i) {
    var y = x[keys[i]];
    r[keys[i]] = { leader: y[0], followers: y.slice(1) };
  }
  return r;
}

function shardDistribution () {
  var db = require('internal').db;
  var dbName = db._name();
  var colls = db._collections();
  var result = {};
  for (var i = 0; i < colls.length; ++i) {
    var collName = colls[i].name();
    var collInfo = global.ArangoClusterInfo.getCollectionInfo(dbName, collName);
    var shards = collInfo.shards;
    var collInfoCurrent = {};
    var shardNames = Object.keys(shards);
    for (var j = 0; j < shardNames.length; ++j) {
      collInfoCurrent[shardNames[j]] =
        global.ArangoClusterInfo.getCollectionInfoCurrent(
          dbName, collName, shardNames[j]).shorts;
    }
    result[collName] = {Plan: format(collInfo.shardShorts),
    Current: format(collInfoCurrent)};
  }

  return {
    results: result
  };
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief move shard
// /////////////////////////////////////////////////////////////////////////////

function moveShard (info) {
  var isLeader;
  var collInfo;
  try {
    // First translate server names from short names to long names:
    var servers = global.ArangoClusterInfo.getDBServers();
    for (let i = 0; i < servers.length; i++) {
      if (servers[i].serverId !== info.fromServer) {
        if (servers[i].serverName === info.fromServer) {
          info.fromServer = servers[i].serverId;
        }
      }
      if (servers[i].serverId !== info.toServer) {
        if (servers[i].serverName === info.toServer) {
          info.toServer = servers[i].serverId;
        }
      }
    }
    collInfo = global.ArangoClusterInfo.getCollectionInfo(info.database,
      info.collection);
    if (collInfo.distributeShardsLike !== undefined) {
      return {error:true, errorMessage:'MoveShard only allowed for collections which have distributeShardsLike unset.'};
    }
    var shards = collInfo.shards;
    var shard = shards[info.shard];
    var pos = shard.indexOf(info.fromServer);
    if (pos === -1) {
      throw 'Banana';
    } else if (pos === 0) {
      isLeader = true;
    } else {
      isLeader = false;
    }
  } catch (e2) {
    return {error:true, errorMessage:'Combination of database, collection, shard and fromServer does not make sense.'};
  }

  var id;
  try {
    id = global.ArangoClusterInfo.uniqid();
    var todo = { 'type': 'moveShard',
      'database': info.database,
      'collection': collInfo.id,
      'shard': info.shard,
      'fromServer': info.fromServer,
      'toServer': info.toServer,
      'jobId': id,
      'timeCreated': (new Date()).toISOString(),
      'creator': ArangoServerState.id(),
      'isLeader': isLeader };
    global.ArangoAgency.set('Target/ToDo/' + id, todo);
  } catch (e1) {
    return {error: true, errorMessage: 'Cannot write to agency.'};
  }

  return {error: false, id: id};
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief rebalance shards
// /////////////////////////////////////////////////////////////////////////////

function rebalanceShards () {
  var dbServers = global.ArangoClusterInfo.getDBServers();
  var dbTab = {};
  var i, j, k, l;
  for (i = 0; i < dbServers.length; ++i) {
    dbTab[dbServers[i].serverId] = [];
  }
  var shardMap = {};

  // First count and collect:
  var db = require('internal').db;

  var colls = db._collections();
  for (j = 0; j < colls.length; ++j) {
    var collName = colls[j].name();
    if (collName.substr(0, 1) === '_') {
      continue;
    }
    var collInfo = global.ArangoClusterInfo.getCollectionInfo(
      db._name(), collName);
    if (collInfo.distributeShardsLike === undefined) {
      // Only consider those collections that do not follow another one
      // w.r.t. their shard distribution.
      var shardNames = Object.keys(collInfo.shards);
      for (k = 0; k < shardNames.length; k++) {
        var shardName = shardNames[k];
        shardMap[shardName] = {
          database: db._name(), collection: collName,
          servers: collInfo.shards[shardName], weight: 1 };
        dbTab[collInfo.shards[shardName][0]].push(
          { shard: shardName, leader: true,
            weight: shardMap[shardName].weight });
        for (l = 1; l < collInfo.shards[shardName].length; ++l) {
          dbTab[collInfo.shards[shardName][l]].push(
            { shard: shardName, leader: false,
              weight: shardMap[shardName].weight });
        }
      }
    }
  }
  
  console.topic('heartbeat=debug', "Rebalancing shards");
  console.topic('heartbeat=debug', shardMap);
  console.topic('heartbeat=debug', dbTab);

  // Compute total weight for each DBServer:
  var totalWeight = [];
  for (i = 0; i < dbServers.length; ++i) {
    totalWeight.push({'server': dbServers[i].serverId,
      'weight': _.reduce(dbTab[dbServers[i].serverId],
      (sum, x) => sum + x.weight, 0)});
  }
  totalWeight = _.sortBy(totalWeight, x => x.weight);

  var shardList = Object.keys(shardMap);
  var countMoved = 0;

  for (i = 0; i < shardList.length; i++) {
    var last = totalWeight.length - 1;
    var fullest = totalWeight[last].server;
    var emptiest = totalWeight[0].server;
    var weightDiff = totalWeight[last].weight - totalWeight[0].weight;
    if (weightDiff < 1.0) {
      console.topic('heartbeat=info', 'rebalanceShards: cluster is balanced');
      return true;
    }
    var shard = shardList[i];
    console.topic('heartbeat=info', 'rebalanceShards: considering shard', shard,
      'totalWeight=', totalWeight);
    if (shardMap[shard].servers.indexOf(fullest) >= 0 &&
      shardMap[shard].servers.indexOf(emptiest) === -1 &&
      shardMap[shard].weight < 0.9 * weightDiff) {
      var shardInfo = shardMap[shard];
      var todo = { database: shardInfo.database,
        collection: shardInfo.collection,
        shard: shard,
        fromServer: fullest,
      toServer: emptiest };
      var res = moveShard(todo);
      if (!res.error) {
        console.topic('heartbeat=debug', 'rebalanceShards: moveShard(', todo, ')');
        totalWeight[last].weight -= shardInfo.weight;
        totalWeight[0].weight += shardInfo.weight;
        totalWeight = _.sortBy(totalWeight, x => x.weight);
        countMoved += 1;
        if (countMoved >= 10) {
          break;
        }
      } else {
        console.topic('heartbeat=error', 'rebalanceShards: moveShard(', todo, ') produced:',
                      res.errorMessage);
      }
    }
  }
  if (countMoved === 0) {
    console.topic('heartbeat=info', 'rebalanceShards: no sensible moves found');
    return true;
  }
  console.topic('heartbeat=info', 'rebalanceShards: scheduled', countMoved, ' shard moves.');
  return true;
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief supervision state
// /////////////////////////////////////////////////////////////////////////////

function supervisionState () {
  try {
    var result = global.ArangoAgency.get('Target');
    result = result.arango.Target;
    var proj = { ToDo: result.ToDo, Pending: result.Pending,
      Failed: result.Failed, Finished: result.Finished,
    error: false };
    return proj;
  } catch (err) {
    return { error: true, errorMsg: 'could not read /Target in agency',
    exception: err };
  }
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief wait for synchronous replication to settle
// /////////////////////////////////////////////////////////////////////////////

function checkForSyncReplOneCollection (dbName, collName) {
  try {
    let cinfo = global.ArangoClusterInfo.getCollectionInfo(dbName, collName);
    let shards = Object.keys(cinfo.shards);
    let ccinfo = shards.map(function (s) {
      return global.ArangoClusterInfo.getCollectionInfoCurrent(dbName,
        collName, s).servers;
    });
    console.topic('heartbeat=debug', 'checkForSyncReplOneCollection:', dbName, collName, shards,
                  cinfo.shards, ccinfo);
    let ok = true;
    for (let i = 0; i < shards.length; ++i) {
      if (cinfo.shards[shards[i]].length !== ccinfo[i].length) {
        ok = false;
      }
    }
    if (ok) {
      console.topic('heartbeat=debug', 'checkForSyncReplOneCollection: OK:', dbName, collName,
                    shards);
      return true;
    }
    console.topic('heartbeat=debug', 'checkForSyncReplOneCollection: not yet:', dbName, collName,
                  shards);
    return false;
  } catch (err) {
    console.topic('heartbeat=error', 'checkForSyncReplOneCollection: exception:', dbName, collName,
                  JSON.stringify(err));
  }
  return false;
}

function waitForSyncReplOneCollection (dbName, collName) {
  console.topic('heartbeat=debug', 'waitForSyncRepl:', dbName, collName);
  let count = 60;
  while (--count > 0) {
    let ok = checkForSyncReplOneCollection(dbName, collName);
    if (ok) {
      return true;
    }
    require('internal').wait(1);
  }
  console.topic('heartbeat=warn', 'waitForSyncReplOneCollection:', dbName, collName, ': BAD');
  return false;
}

function waitForSyncRepl (dbName, collList) {
  if (!isCoordinator()) {
    return true;
  }
  let n = collList.length;
  let count = 30 * n;   // wait for up to 30 * collList.length seconds
                        // usually, this is much faster, but under load
                        // when many unittests run, things may take longer
  let ok = [...Array(n)].map(v => false);
  while (--count > 0) {
    let allOk = true;
    for (var i = 0; i < n; ++i) {
      if (!ok[i]) {
        ok[i] = checkForSyncReplOneCollection(dbName, collList[i].name());
        allOk = allOk && ok[i];
      }
    }
    if (allOk) {
      return true;
    }
    require('internal').wait(1);
  }
  console.topic('heartbeat=warn', 'waitForSyncRepl: timeout:', dbName, collList);
  return false;
}

function endpoints() {
  try {
    let coords = global.ArangoClusterInfo.getCoordinators();
    let endpoints = coords.map(c => global.ArangoClusterInfo.getServerEndpoint(c));
    return { "endpoints": endpoints.map(function(e) {
                                          return {"endpoint": e};
                                        }) };
  } catch (err) {
    return { error: true, exception: err };
  }
}

exports.coordinatorId = coordinatorId;
exports.handlePlanChange = handlePlanChange;
exports.isCluster = isCluster;
exports.isCoordinator = isCoordinator;
exports.isCoordinatorRequest = isCoordinatorRequest;
exports.role = role;
exports.shardList = shardList;
exports.status = status;
exports.wait = waitForDistributedResponse;
exports.endpointToURL = endpointToURL;
exports.synchronizeOneShard = synchronizeOneShard;
exports.shardDistribution = shardDistribution;
exports.rebalanceShards = rebalanceShards;
exports.moveShard = moveShard;
exports.supervisionState = supervisionState;
exports.waitForSyncRepl = waitForSyncRepl;
exports.endpoints = endpoints;
exports.fetchKey = fetchKey;

exports.executePlanForDatabases = executePlanForDatabases;
exports.executePlanForCollections = executePlanForCollections;
exports.updateCurrentForDatabases = updateCurrentForDatabases;
exports.updateCurrentForCollections = updateCurrentForCollections;
