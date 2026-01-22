/* jshint globalstrict:false, strict:false, unused: false */
/* global arango, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNotNull, ARGUMENTS */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const analyzers = require("@arangodb/analyzers");
const { deriveTestSuite, getMetric, versionHas } = require('@arangodb/test-helper');
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const db = arangodb.db;
const _ = require('lodash');

const replication = require('@arangodb/replication');
const internal = require('internal');

const cn = 'UnitTestsReplication';
const sysCn = '_UnitTestsReplication';
const userManager = require("@arangodb/users");
let IM = global.instanceManager;
const leaderEndpoint = IM.arangods[0].endpoint;
const followerEndpoint = IM.arangods[1].endpoint;

const {
  connectToLeader,
  connectToFollower,
  collectionChecksum,
  clearFailurePoints } = require(fs.join('tests', 'js', 'client', 'replication', 'sync', 'replication-sync.inc'));

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite for key conflicts in incremental sync
// //////////////////////////////////////////////////////////////////////////////

function ReplicationIncrementalKeyConflict () {
  'use strict';

  return {

    setUp: function () {
      connectToLeader();
      db._drop(cn);
    },

    tearDown: function () {
      connectToLeader();
      db._drop(cn);
      db._flushCache();
      userManager.all().forEach(user => {
        if (user.user !== "root") {
          userManager.remove(user.user);
        }
      });

      connectToFollower();
      db._drop(cn);
      db._flushCache();
      userManager.all().forEach(user => {
        if (user.user !== "root") {
          userManager.remove(user.user);
        }
      });
    },

    testKeyConflictsIncremental: function () {
      var c = db._create(cn);
      c.ensureIndex({
        type: 'hash',
        fields: ['value'],
        unique: true
      });

      c.insert({
        _key: 'x',
        value: 1
      });
      c.insert({
        _key: 'y',
        value: 2
      });
      c.insert({
        _key: 'z',
        value: 3
      });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        username: "root",
        password: "",
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual('hash', c.indexes()[1].type);
      assertTrue(c.indexes()[1].unique);

      assertEqual(3, c.count());
      assertEqual(1, c.document('x').value);
      assertEqual(2, c.document('y').value);
      assertEqual(3, c.document('z').value);

      connectToLeader();
      c = db._collection(cn);
      c.remove('z');
      c.insert({
        _key: 'w',
        value: 3
      });

      assertEqual(3, c.count());
      assertEqual(3, c.document('w').value);
      assertEqual(1, c.document('x').value);
      assertEqual(2, c.document('y').value);

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true,
        username: "root",
        password: "",
      });

      db._flushCache();

      assertEqual('hash', c.indexes()[1].type);
      assertTrue(c.indexes()[1].unique);

      c = db._collection(cn);
      assertEqual(3, c.count());
      assertEqual(3, c.document('w').value);
      assertEqual(1, c.document('x').value);
      assertEqual(2, c.document('y').value);

      
      connectToLeader();
      c = db._collection(cn);

      c.remove('w');
      c.insert({
        _key: 'z',
        value: 3
      });
      
      assertEqual(3, c.count());
      assertEqual(1, c.document('x').value);
      assertEqual(2, c.document('y').value);
      assertEqual(3, c.document('z').value);

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true,
        username: "root",
        password: "",
      });

      db._flushCache();

      c = db._collection(cn);
      assertEqual(3, c.count());
      assertEqual(1, c.document('x').value);
      assertEqual(2, c.document('y').value);
      assertEqual(3, c.document('z').value);
    },
    
    testKeyConflictsRandom: function () {
      var c = db._create(cn);
      c.ensureIndex({
        type: 'hash',
        fields: ['value'],
        unique: true
      });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        username: "root",
        password: "",
      });
      db._flushCache();
      c = db._collection(cn);
     
      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(internal.genRandomAlphaNumbers(10));
      }

      keys.forEach(function(key, i) {
        c.insert({ _key: key, value: i });
      });

      connectToLeader();
      c = db._collection(cn);
     
      function shuffle(array) {
        for (let i = array.length - 1; i > 0; i--) {
          const j = Math.floor(Math.random() * (i + 1));
          [array[i], array[j]] = [array[j], array[i]];
        }
      }
      shuffle(keys);

      keys.forEach(function(key, i) {
        c.insert({ _key: key, value: i });
      });
      
      assertEqual(1000, c.count());
      let checksum = collectionChecksum(c.name());

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true,
        username: "root",
        password: "",
      });

      db._flushCache();

      assertEqual('hash', c.indexes()[1].type);
      assertTrue(c.indexes()[1].unique);

      c = db._collection(cn);
      assertEqual(1000, c.count());
      assertEqual(checksum, collectionChecksum(c.name()));
    },
    
    testKeyConflictsRandomDiverged: function () {
      var c = db._create(cn);
      c.ensureIndex({
        type: 'hash',
        fields: ['value'],
        unique: true
      });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        username: "root",
        password: "",
      });
      db._flushCache();
      c = db._collection(cn);
     
      for (let i = 0; i < 1000; ++i) {
        c.insert({ _key: internal.genRandomAlphaNumbers(10), value: i });
      }

      connectToLeader();
      c = db._collection(cn);
      
      for (let i = 0; i < 1000; ++i) {
        c.insert({ _key: internal.genRandomAlphaNumbers(10), value: i });
      }
     
      assertEqual(1000, c.count());
      let checksum = collectionChecksum(c.name());

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true,
        username: "root",
        password: "",
      });

      db._flushCache();

      assertEqual('hash', c.indexes()[1].type);
      assertTrue(c.indexes()[1].unique);

      c = db._collection(cn);
      assertEqual(1000, c.count());
      assertEqual(checksum, collectionChecksum(c.name()));
    },
    
    testKeyConflictsIncrementalManyDocuments: function () {
      var c = db._create(cn);
      var i;
      c.ensureIndex({
        type: 'hash',
        fields: ['value'],
        unique: true
      });

      for (i = 0; i < 10000; ++i) {
        c.insert({
          _key: 'test' + i,
          value: i
        });
      }

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        username: "root",
        password: "",
      });
      db._flushCache();
      c = db._collection(cn);

      assertEqual(10000, c.count());

      assertEqual('hash', c.indexes()[1].type);
      assertTrue(c.indexes()[1].unique);

      connectToLeader();
      c = db._collection(cn);

      c.remove('test0');
      c.remove('test1');
      c.remove('test9998');
      c.remove('test9999');

      c.insert({
        _key: 'test0',
        value: 9999
      });
      c.insert({
        _key: 'test1',
        value: 9998
      });
      c.insert({
        _key: 'test9998',
        value: 1
      });
      c.insert({
        _key: 'test9999',
        value: 0
      });

      assertEqual(10000, c.count());

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true,
        username: "root",
        password: "",
      });

      db._flushCache();

      c = db._collection(cn);
      assertEqual(10000, c.count());

      assertEqual('hash', c.indexes()[1].type);
      assertTrue(c.indexes()[1].unique);
    }
  };
}

jsunity.run(ReplicationIncrementalKeyConflict);

return jsunity.done();
