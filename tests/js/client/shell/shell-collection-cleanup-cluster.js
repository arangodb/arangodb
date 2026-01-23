/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertMatch, assertTrue, assertFalse, arango, db */

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
/// @author Lars Maier
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const IM = global.instanceManager;
const AM = IM.agencyMgr;

let { getServersByType, isEnterprise } = require('@arangodb/test-helper');

const wait = require("internal").wait;

const primaryEndpoint = arango.getEndpoint();

function collectionCleanupSuite() {
  'use strict';

  let agentEndpoints = [];
  let agencyLeader = "";
  let coordinatorId;
  let coordinatorRebootId;
  const maintenanceURL = "/_admin/cluster/maintenance";

  let getCoordinatorRebootId = function() {
    let coords = getServersByType("coordinator");
    coordinatorId = coords[0].id;
    let res = AM.call("read", [["/arango/Current/ServersKnown"]]);
    coordinatorRebootId = res[0].arango.Current.ServersKnown[coordinatorId];
  };

  let waitForCollectionRemoval = function(collId) {
    // Wait until collection is removed from Plan
    let planKey = `/arango/Plan/Collections/_system/${collId}`;
    for (let counter = 0; counter < 30; ++counter) {
      wait(1);
      let plan = AM.call("read", [[planKey]]);
      if (!plan[0].arango.Plan.Collections._system.hasOwnProperty(collId)) {
        return;
      }
    }
    throw "Lost patience with collection removal from Plan";
  };

  let waitForCollectionStaysInPlan = function(collId) {
    // Wait and verify collection stays in Plan
    let planKey = `/arango/Plan/Collections/_system/${collId}`;
    for (let counter = 0; counter < 10; ++counter) {
      wait(1);
      let plan = AM.call("read", [[planKey]]);
      if (!plan[0].arango.Plan.Collections._system.hasOwnProperty(collId)) {
        throw "Collection was cleaned up, but coordinator still there!";
      }
    }
    // All good, collection still there
  };

  return {

    testCollectionCreationAndCleanup: function() {
      // The supervision in the agency should cleanup a collection which had errors
      // in creation, if the coordinator is rebooted.

      getCoordinatorRebootId();

      let res = arango.PUT(maintenanceURL, '"on"');
      assertFalse(res.error);

      try {
        let collId = "999999";
        let planKey = `/arango/Plan/Collections/_system/${collId}`;

        // Create a broken collection in the Plan
        let body = {"/arango/Plan/Version": {op: "increment"}};
        body[planKey] = {
          op: "set",
          new: {
            id: collId,
            name: "BrokenCollection",
            type: 2,
            replicationFactor: 2,
            numberOfShards: 1,
            shards: {
              "s999999": ["PRMR-00000000"]
            },
            coordinator: coordinatorId,
            coordinatorRebootId: 0,  // 0 is old, coordinator has rebooted
            isBuilding: true
          }
        };
        AM.call("write", [[body]]);

        res = arango.PUT(maintenanceURL, '"off"');
        assertFalse(res.error);

        // wait until collection is removed from Plan
        waitForCollectionRemoval(collId);

      } finally {
        arango.PUT(maintenanceURL, '"off"');
      }
    },

    testCollectionCreationNoCleanup: function() {
      // The supervision in the agency should not cleanup a collection
      // which had errors in creation, if the coordinator is still there.

      getCoordinatorRebootId();

      let res = arango.PUT(maintenanceURL, '"on"');
      assertFalse(res.error);

      try {
        let collId = "999998";
        let planKey = `/arango/Plan/Collections/_system/${collId}`;

        // Create a broken collection in the Plan with current reboot ID
        let body = {"/arango/Plan/Version": {op: "increment"}};
        body[planKey] = {
          op: "set",
          new: {
            id: collId,
            name: "BrokenCollectionNoCleanup",
            type: 2,
            replicationFactor: 2,
            numberOfShards: 1,
            shards: {
              "s999998": ["PRMR-00000000"]
            },
            coordinator: coordinatorId,
            coordinatorRebootId: coordinatorRebootId,  // current reboot ID
            isBuilding: true
          }
        };
        AM.call("write", [[body]]);

        res = arango.PUT(maintenanceURL, '"off"');
        assertFalse(res.error);

        // verify collection stays in Plan
        waitForCollectionStaysInPlan(collId);

        // Clean up
        res = arango.PUT(maintenanceURL, '"on"');
        assertFalse(res.error);
        let deleteBody = {"/arango/Plan/Version": {op: "increment"}};
        deleteBody[planKey] = {op: "delete"};
        AM.call("write", [[deleteBody]]);
        res = arango.PUT(maintenanceURL, '"off"');
        assertFalse(res.error);

      } finally {
        arango.PUT(maintenanceURL, '"off"');
      }
    },

    testSmartEdgeCollectionCreationAndCleanup: function() {
      // The supervision should cleanup a broken smart edge collection
      // along with its shadow collections when the coordinator dies.
      // This test only runs in enterprise edition.

      if (!isEnterprise()) {
        return; // skip test in community edition
      }

      getCoordinatorRebootId();

      let res = arango.PUT(maintenanceURL, '"on"');
      assertFalse(res.error);

      try {
        let collId = "999997";
        let shadowCollId1 = "999996";
        let shadowCollId2 = "999995";
        let shadowCollId3 = "999994";
        let planKey = `/arango/Plan/Collections/_system/${collId}`;
        let shadowPlanKey1 = `/arango/Plan/Collections/_system/${shadowCollId1}`;
        let shadowPlanKey2 = `/arango/Plan/Collections/_system/${shadowCollId2}`;
        let shadowPlanKey3 = `/arango/Plan/Collections/_system/${shadowCollId3}`;

        // Create a broken smart edge collection with shadow collections
        let body = {"/arango/Plan/Version": {op: "increment"}};
        body[planKey] = {
          op: "set",
          new: {
            id: collId,
            name: "BrokenSmartEdgeCollection",
            type: 3,  // edge collection
            replicationFactor: 2,
            numberOfShards: 1,
            isSmart: true,
            shards: {
              "s999997": ["PRMR-00000000"]
            },
            shadowCollections: [parseInt(shadowCollId1), parseInt(shadowCollId2), parseInt(shadowCollId3)],
            coordinator: coordinatorId,
            coordinatorRebootId: 0,  // 0 is old, coordinator has rebooted
            isBuilding: true
          }
        };

        // Add shadow collections
        body[shadowPlanKey1] = {
          op: "set",
          new: {
            id: shadowCollId1,
            name: "BrokenSmartEdgeCollection_shadow1",
            type: 2,
            replicationFactor: 2,
            numberOfShards: 1,
            shards: {
              "s999996": ["PRMR-00000000"]
            }
          }
        };

        body[shadowPlanKey2] = {
          op: "set",
          new: {
            id: shadowCollId2,
            name: "BrokenSmartEdgeCollection_shadow2",
            type: 2,
            replicationFactor: 2,
            numberOfShards: 1,
            shards: {
              "s999995": ["PRMR-00000000"]
            }
          }
        };

        body[shadowPlanKey3] = {
          op: "set",
          new: {
            id: shadowCollId3,
            name: "BrokenSmartEdgeCollection_shadow3",
            type: 2,
            replicationFactor: 2,
            numberOfShards: 1,
            shards: {
              "s999994": ["PRMR-00000000"]
            }
          }
        };

        AM.call("write", [[body]]);

        res = arango.PUT(maintenanceURL, '"off"');
        assertFalse(res.error);

        // wait until main collection is removed from Plan
        waitForCollectionRemoval(collId);

        // verify shadow collections are also removed
        waitForCollectionRemoval(shadowCollId1);
        waitForCollectionRemoval(shadowCollId2);
        waitForCollectionRemoval(shadowCollId3);

      } finally {
        arango.PUT(maintenanceURL, '"off"');
      }
    },
  };
}

if (db._properties().replicationVersion !== "2") {
  // In replication two the supervision cleanup works differently
  jsunity.run(collectionCleanupSuite);
}

return jsunity.done();
