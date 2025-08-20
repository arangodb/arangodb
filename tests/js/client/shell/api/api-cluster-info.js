/* jshint strict: false */
/* global assertTrue, assertFalse, assertEqual, fail, arango, db, GLOBAL */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
// / @author Wilfried Goesgens
// / @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const { errors } = require('@arangodb');
const ci = require('@arangodb/cluster-info');
const IM = GLOBAL.instanceManager;

function verifyClusterInfoSuite() {
  'use strict';
  return {
    setUp: function () {
    },

    tearDown: function () {
    },

    testFlush: function () {
      assertTrue(ci.flush());
    },
    testgetCollectionInfo: function () {
      try {
        const ret = ci.getCollectionInfo(0, 0);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
      const ret = ci.getCollectionInfo("_system", "_graphs");
      Object.keys(ret.shards).forEach(shard => {
        assertEqual(ret.shards[shard].length, 2, ret);
      });
    },
    testgetCollectionInfoCurrent: function () {
      try {
        const ret = ci.getCollectionInfoCurrent(0, 0, 0);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_BAD_PARAMETER.code);
      }
      const ret = ci.getCollectionInfoCurrent('_system', '_users', db._users.shards()[0]);
      assertEqual(ret.indexes.length, 2, ret);
      assertEqual(ret.shorts.length, 2, ret);
      assertEqual(ret.servers.length, 2, ret);
      assertEqual(ret.failoverCandidates.length, 2, ret);
    },
    testgetResponsibleServers: function () {
      let shardIDs = [];
      ["_users", "_graphs", "_apps"].forEach(col => {
        shardIDs.push(db[col].shards()[0]);
      });
      let ret = ci.getResponsibleServers(shardIDs);
      shardIDs.forEach(shard => {
        assertTrue(ret[shard] != null, ret);
      });

      try {
        ci.getResponsibleServers(['S00000']);
        fail();
      } catch(err) {
        assertEqual(err.errorNum, errors.ERROR_BAD_PARAMETER.code);
      }
      try {
        ci.getResponsibleServers([]);
        fail();
      } catch(err) {
        assertEqual(err.errorNum, errors.ERROR_BAD_PARAMETER.code);
      }
      try {
        ci.getResponsibleServers(null);
        fail();
      } catch(err) {
        assertEqual(err.errorNum, errors.ERROR_BAD_PARAMETER.code);
      }
    },
    testgetResponsibleShard: function () {
      try {
        db._create('test');
        db.test.save({_key: 'test', foo: true});
        ci.getResponsibleShard("test", {_key: "test"}, false);
      } finally {
        db._drop('test');
      }
    },
    testgetAnalyzersRevision: function () {
      const ret = ci.getAnalyzersRevision('_system');
      assertTrue(ret.revision >= 0, ret);
      assertTrue(ret.buildingRevision >= 0, ret);
    },
    testwaitForPlanVersion: function () {
      const ret = ci.waitForPlanVersion();
    },
    testgetMaxNumberOfShards: function() {
      const ret = ci.getMaxNumberOfShards();
    },
    testgetMaxReplicationFactor: function() {
      const ret = ci.getMaxReplicationFactor();
    },
    testgetMinReplicationFactor: function() {
      const ret = ci.getMinReplicationFactor();
    }
  };
};

jsunity.run(verifyClusterInfoSuite);

return jsunity.done();
