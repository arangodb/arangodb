/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertMatch, assertTrue, assertFalse, arango, db */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');

let { getServersByType,
      agency }
  = require('@arangodb/test-helper');

const wait = require("internal").wait;

const primaryEndpoint = arango.getEndpoint();

function indexCleanupSuite() {
  'use strict';

  let coll = null;
  let agentEndpoints = [];
  let agencyLeader = "";
  let coordinatorId;
  let coordinatorRebootId;
  const collName = "UnitTestIndexCleanup";
  const maintenanceURL = "/_admin/cluster/maintenance";

  let getCoordinatorRebootId = function() {
    let coords = getServersByType("coordinator");
    coordinatorId = coords[0].id;
    let res = agency.call("read", [["/arango/Current/ServersKnown"]]);
    coordinatorRebootId = res[0].arango.Current.ServersKnown[coordinatorId];
  };

  let waitForIndexesInCurrent = function(nrIndexes, collId) {
    // Wait until Current entries are done
    let curKey = `/arango/Current/Collections/_system/${collId}`;
    for (let counter = 0; counter < 30; ++counter) {
      wait(1);
      let cur = agency.call("read",[[curKey]]);
      let curColl = cur[0].arango.Current.Collections._system[collId];
      let shardIds = Object.keys(curColl);
      let nrOk = 0;
      for (let s of shardIds) {
        if (curColl[s].indexes.length === nrIndexes) {
          nrOk += 1;
        }
      }
      if (nrOk === shardIds.length) {
        return;
      }
    }
    throw "Lost patience with indexes in Current";
  };

  return {

    setUp: function() {
      coll = db._create(collName, {replicationFactor:2, numberOfShards:3});
    },

    tearDown: function() {
      // Just in case we failed when maintenance mode was on:
      arango.PUT(maintenanceURL, '"off"');
      try {
        db._drop(collName);
      } catch(e) {
      }
    },

    testIndexCreationAndFinalization: function() {
      // The supervision in the agency should finish an index which is properly
      // created.

      getCoordinatorRebootId();

      let res = arango.PUT(maintenanceURL, '"on"');
      assertFalse(res.error);

      let planKey = `/arango/Plan/Collections/_system/${coll._id}/indexes`;

      // First check how many indexes there are now:
      let oldPlan = agency.call("read", [[planKey]])[0].arango.Plan.Collections._system[coll._id].indexes;
      let nrIndexes = oldPlan.length;

      // Now add one in the Plan:
      let body = {"/arango/Plan/Version":{op:"increment"}};
      body[planKey] = {"op":"push", "new": {
        id:"147",
        type:"persistent",
        name:"Paul",
        fields:["Hello"],
        unique:false,
        sparse:false,
        coordinator: coordinatorId,
        coordinatorRebootId,
        isBuilding:true}};
      agency.call("write", [[body]]);

      waitForIndexesInCurrent(nrIndexes + 1, coll._id);

      res = arango.PUT(maintenanceURL, '"off"');
      assertFalse(res.error);

      // wait until index is no longer isBuilding in Plan
      for (let i = 0; i < 30; ++i) {
        let res = agency.call("read", [[planKey]]);
        let indexes = res[0].arango.Plan.Collections._system[coll._id].indexes;
        assertEqual(indexes.length, nrIndexes + 1);
        if (!indexes[nrIndexes].hasOwnProperty("isBuilding") &&
            !indexes[nrIndexes].hasOwnProperty("coordinator") &&
            !indexes[nrIndexes].hasOwnProperty("coordinatorRebootId")) {
          return;   // all good
        }
        wait(1);
      }
      throw "Lost patience with isBuilding flag in index";
    },

    testIndexCreationAndCleanup: function() {
      // The supervision in the agency should cleanup an index which had errors
      // in creation, if the coordinator is rebooted.

      getCoordinatorRebootId();

      let res = arango.PUT(maintenanceURL, '"on"');
      assertFalse(res.error);

      let planKey = `/arango/Plan/Collections/_system/${coll._id}/indexes`;

      // First check how many indexes there are now:
      let oldPlan = agency.call("read", [[planKey]])[0].arango.Plan.Collections._system[coll._id].indexes;
      let nrIndexes = oldPlan.length;

      // Now add one in the Plan:
      let body = {"/arango/Plan/Version":{op:"increment"}};
      body[planKey] = {"op":"push", "new": {
        id:"147",
        type:"persistent",
        name:"primary",    // name is duplicate, so error
        fields:["Hello"],
        unique:false,
        sparse:false,
        coordinator: coordinatorId,
        coordinatorRebootId: 0,    // 0 is old
        isBuilding:true}};
      agency.call("write", [[body]]);

      waitForIndexesInCurrent(nrIndexes + 1, coll._id);

      res = arango.PUT(maintenanceURL, '"off"');
      assertFalse(res.error);

      // wait until index is no longer isBuilding in Plan
      for (let i = 0; i < 30; ++i) {
        let res = agency.call("read", [[planKey]]);
        let indexes = res[0].arango.Plan.Collections._system[coll._id].indexes;
        if (indexes.length === nrIndexes) {
          return;   // all good, our index has vanished again
        }
        wait(1);
      }
      throw "Lost patience with isBuilding flag in index";
    },

    testIndexCreationNoCleanup: function() {
      // The supervision in the agency should not cleanup an index 
      // which had errors in creation, if the coordinator is still there.

      getCoordinatorRebootId();

      let res = arango.PUT(maintenanceURL, '"on"');
      assertFalse(res.error);

      let planKey = `/arango/Plan/Collections/_system/${coll._id}/indexes`;

      // First check how many indexes there are now:
      let oldPlan = agency.call("read", [[planKey]])[0].arango.Plan.Collections._system[coll._id].indexes;
      let nrIndexes = oldPlan.length;

      // Now add one in the Plan:
      let body = {"/arango/Plan/Version":{op:"increment"}};
      body[planKey] = {"op":"push", "new": {
        id:"147",
        type:"persistent",
        name:"primary",    // name is duplicate, so error
        fields:["Hello"],
        unique:false,
        sparse:false,
        coordinator: coordinatorId,
        coordinatorRebootId,
        isBuilding:true}};
      agency.call("write", [[body]]);

      waitForIndexesInCurrent(nrIndexes + 1, coll._id);

      res = arango.PUT(maintenanceURL, '"off"');
      assertFalse(res.error);

      // wait until index is no longer isBuilding in Plan
      for (let i = 0; i < 10; ++i) {
        let res = agency.call("read", [[planKey]]);
        let indexes = res[0].arango.Plan.Collections._system[coll._id].indexes;
        if (indexes.length === nrIndexes) {
          throw "Index was cleaned up, but coordinator still there!";
        }
        wait(1);
      }
      // All good here, no cleanup has happened for 10 seconds!
    },
  };
}

// only run when there is a coordinator

if (db._properties().replicationVersion !== "2") {
  // In replication two the Index is written to Target in the agency
  // the Coordinator is not involved with index creation anymore.
  // Making this test suite obsolete.
  jsunity.run(indexCleanupSuite);
}

return jsunity.done();

