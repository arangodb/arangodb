/* jshint strict: false */
/* global assertTrue, assertFalse, assertEqual, fail, arango, GLOBAL */

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

    testdoesDatabaseExist: function () {
      const dbn = 'testdb';
      try {
        db._createDatabase(dbn);
        const res = ci.doesDatabaseExist(dbn);
        assertTrue(res, dbn + " exists.");
      } finally {
        db._useDatabase('_system');
        db._dropDatabase(dbn);
      }
      const res = ci.doesDatabaseExist(dbn);
      assertFalse(res, dbn + " no longer exists.");
    },
    testFlush: function () {
      assertTrue(ci.flush());
    },
    testDatabases: function () {
      const dbn = 'testdb';
      const ret = ci.databases();
      assertEqual(ret.length, 1, ret);
      try {
        db._createDatabase(dbn);
        const ret = ci.databases();
        assertEqual(ret.length, 2, ret);
      } finally {
        db._useDatabase('_system');
        db._dropDatabase(dbn);
      }
    },
    testgetCollectionInfo: function () {
      try {
        const ret = ci.getCollectionInfo(0, 0);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
      const ret = ci.getCollectionInfo("_system", "_graphs");
      Object.keys(ret).forEach(shard => {
        assertEqual(ret[shard].length, 2, ret);
      });
    },
    testgetCollectionInfoCurrent: function () {
      try {
        const ret = ci.getCollectionInfoCurrent(0, 0, 0);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
      const ret = ci.getCollectionInfoCurrent('_system', '_users', db._users.shards()[0]);
      assertEqual(ret.indexes.length, 2, ret);
      assertEqual(ret.shorts.length, 2, ret);
      assertEqual(ret.servers.length, 2, ret);
      assertEqual(ret.failoverCandidates.length, 2, ret);
    },
    testgetResponsibleServer: function () {
      let ret = ci.getResponsibleServer(db._users.shards()[0]);
      assertEqual(ret.length, 2, ret);
      ret = ci.getResponsibleServer("xxx");
      assertEqual(ret.length, 0, ret);
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
      // TODO vocbase
      // const ret = ci.getResponsibleShard(collectionID, documentKey, parseSuccess);
    },
    testgetServerEndpoint: function () {
      // aim for db-servers, these got server ids:
      const ret = ci.getServerEndpoint(IM.arangods[3].id);
      assertEqual(ret, IM.arangods[3].endpoint);
      try {
        ci.getServerEndpoint("PRMR-00000000-0000-0000-0000-000000000000");
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_HTTP_NOT_FOUND.code);
      }
      try {
        ci.getServerEndpoint("not valid");
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_HTTP_SERVICE_UNAVAILABLE.code);
      }
    },
    testgetServerName: function () {
      assertEqual(ci.getServerName(IM.arangods[3].endpoint), IM.arangods[3].id)
      assertEqual(ci.getServerName("xxx"), "");
    },
    testgetDBServers: function () {
      const ret = ci.getDBServers();
      assertEqual(ret.length, IM.options.dbServers, ret);
    },
    testgetCoordinators: function () {
      const ret = ci.getCoordinators();
      assertEqual(ret.length, IM.options.coordinators, ret);
    },
    testUniqid: function () {
      const ret1 = ci.uniqid(100);
      const ret2 = ci.uniqid(100);
      assertTrue(ret2 - ret1 >= 100);
    },
    testgetAnalyzersRevision: function () {
      const ret = ci.getAnalyzersRevision('_system');
      assertTrue(ret.revision >= 0, ret);
      assertTrue(ret.buildingRevision >= 0, ret);
    },
    testwaitForPlanVersion: function () {
      const ret = ci.waitForPlanVersion();
    }
  };
};

jsunity.run(verifyClusterInfoSuite);

return jsunity.done();
