/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use it except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Copyright 2026, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {assertTrue, assertEqual} = jsunity.jsUnity.assertions;
const internal = require('internal');
const arangodb = require('@arangodb');
const db = arangodb.db;
const isCluster = internal.isCluster();
const errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief Single-server: creation of geo1/geo2 is rejected.
////////////////////////////////////////////////////////////////////////////////

function geoLegacySingleServerSuite() {
  'use strict';
  const cn = 'UnitTestsGeoLegacy';

  return {
    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testSingleServerCreateGeo1Rejected: function () {
      if (isCluster) {
        return;
      }
      const c = db._create(cn);
      try {
        c.ensureIndex({type: 'geo1', fields: ['pos']});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testSingleServerCreateGeo2Rejected: function () {
      if (isCluster) {
        return;
      }
      const c = db._create(cn);
      try {
        c.ensureIndex({type: 'geo2', fields: ['lat', 'lon']});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster: creation of geo1/geo2 is rejected.
////////////////////////////////////////////////////////////////////////////////

function geoLegacyClusterRejectionSuite() {
  'use strict';
  const cn = 'UnitTestsGeoLegacyCluster';

  return {
    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testClusterCreateGeo1Rejected: function () {
      if (!isCluster) {
        return;
      }
      const c = db._create(cn);
      try {
        c.ensureIndex({type: 'geo1', fields: ['pos']});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testClusterCreateGeo2Rejected: function () {
      if (!isCluster) {
        return;
      }
      const c = db._create(cn);
      try {
        c.ensureIndex({type: 'geo2', fields: ['lat', 'lon']});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster (with agency): When Plan has geo1/geo2: 
///        (1) loading reroutes to geo (index works);
///        (2) UpgradeTasks runs on DB servers but does NOT update the Agency,
///        so stored data (Plan) still has geo1/geo2. Requires instanceManager.
////////////////////////////////////////////////////////////////////////////////

function geoLegacyClusterAgencySuite() {
  'use strict';
  const dbName = arango.getDatabaseName();
  const cn = 'UnitTestsGeoLegacyAgency';

  return {
    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    // Plan has geo1 -> load reroutes to geo (index works); Plan is NOT rewritten.
    testClusterLoadGeo1FromPlanWorksStoredDataNotRewritten: function () {
      if (!isCluster || typeof global.instanceManager === 'undefined') {
        return;
      }
      const IM = global.instanceManager;
      const c = db._create(cn, {numberOfShards: 3});
      const collId = c._id;
      const planIndexes = [
        {id: '0', type: 'primary', name: 'primary', fields: ['_key'], sparse: false, unique: true},
        {id: '95', type: 'geo1', fields: ['loc'], geoJson: false, name: 'geo1_idx'}
      ];
      IM.agencyMgr.set(`Plan/Collections/${dbName}/${collId}/indexes`, planIndexes);
      IM.agencyMgr.increaseVersion('Plan/Version');
      internal.sleep(5);
      const indexes = c.indexes();
      const geoIdx = indexes.find(idx => idx.fields && idx.fields[0] === 'loc');
      assertTrue(geoIdx !== undefined, 'Index with field loc should exist (loading rerouted to geo)');
      assertEqual('geo', geoIdx.type, 'Loading must reroute geo1 to geo so index works');
      const plan = IM.agencyMgr.get(`Plan/Collections/${dbName}/${collId}`);
      assertTrue(plan !== undefined && plan.arango && plan.arango.Plan && plan.arango.Plan.Collections, 'Plan should be readable');
      const planColl = plan.arango.Plan.Collections[dbName] && (plan.arango.Plan.Collections[dbName][collId] || plan.arango.Plan.Collections[dbName][String(collId)]);
      assertTrue(planColl && Array.isArray(planColl.indexes), 'Plan should have indexes');
      const planGeoIdx = planColl.indexes.find(idx => idx.type === 'geo1' || (idx.fields && idx.fields[0] === 'loc'));
      assertTrue(planGeoIdx !== undefined, 'Plan should have the geo index definition');
      assertEqual('geo1', planGeoIdx.type, 'Stored data (Plan) must NOT be rewritten to geo in cluster');
    },

    // Plan has geo2 -> load reroutes to geo (index works); Plan is NOT rewritten.
    testClusterLoadGeo2FromPlanWorksStoredDataNotRewritten: function () {
      if (!isCluster || typeof global.instanceManager === 'undefined') {
        return;
      }
      const IM = global.instanceManager;
      const col = db._create(cn, {numberOfShards: 3});
      const collId = col._id;
      const planIndexes = [
        {id: '0', type: 'primary', name: 'primary', fields: ['_key'], sparse: false, unique: true},
        {id: '96', type: 'geo2', fields: ['lat', 'lon'], geoJson: false, name: 'geo2_idx'}
      ];
      IM.agencyMgr.set(`Plan/Collections/${dbName}/${collId}/indexes`, planIndexes);
      IM.agencyMgr.increaseVersion('Plan/Version');
      internal.sleep(5);
      const indexes = col.indexes();
      const geoIdx = indexes.find(idx => idx.fields && idx.fields.length === 2 && idx.fields[0] === 'lat' && idx.fields[1] === 'lon');
      assertTrue(geoIdx !== undefined, 'Index with fields lat,lon should exist (loading rerouted to geo)');
      assertEqual('geo', geoIdx.type, 'Loading must reroute geo2 to geo so index works');
      const plan = IM.agencyMgr.get(`Plan/Collections/${dbName}/${collId}`);
      const planColl = plan.arango.Plan.Collections[dbName] && (plan.arango.Plan.Collections[dbName][collId] || plan.arango.Plan.Collections[dbName][String(collId)]);
      assertTrue(planColl && Array.isArray(planColl.indexes), 'Plan should have indexes');
      const planGeoIdx = planColl.indexes.find(idx => idx.type === 'geo2' || (idx.fields && idx.fields[0] === 'lat'));
      assertTrue(planGeoIdx !== undefined, 'Plan should have the geo index definition');
      assertEqual('geo2', planGeoIdx.type, 'Stored data (Plan) must NOT be rewritten to geo in cluster');
    }
  };
}

jsunity.run(geoLegacySingleServerSuite);
jsunity.run(geoLegacyClusterRejectionSuite);
jsunity.run(geoLegacyClusterAgencySuite);

return jsunity.done();
