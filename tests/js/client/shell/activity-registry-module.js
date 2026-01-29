/* jshint esnext: true */

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
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const { assertTrue } = jsunity.jsUnity.assertions;

const internal = require('internal');
const db = require('internal').db;
const activitiesModule = require('@arangodb/activity-registry');
const IM = global.instanceManager;

const c = "my_collection";

function activityRegistryModuleSuite() {
  return {
    setUpAll: function () {
    },

    tearDownAll: function () {
    },

    testPrintsOutASingleActivity: function () {
      const lines = activitiesModule.pretty_print([
        {
          "id" : "0x7a1c8743c780", 
          "name" : "ActivityRegistryRestHandler", 
          "state" : "Active", 
          "parent" : {}, 
          "metadata" : { 
            "method" : "GET", 
            "url" : "/_admin/activity-registry" 
          } 
        }
      ]);
      assertEqual(lines, '── ActivityRegistryRestHandler: {"method":"GET","url":"/_admin/activity-registry"}');
    },
    testPrintsOutSeparateActivities: function () {
      const lines = activitiesModule.pretty_print([
        {
          "id" : "0x7a1c8743c780", 
          "name" : "ActivityRegistryRestHandler", 
          "state" : "Active", 
          "parent" : {}, 
          "metadata" : { 
            "method" : "GET", 
            "url" : "/_admin/activity-registry" 
          } 
        },
        {
          "id": "0x7b9050e26140",
          "name": "RestDumpHandler",
          "state": "Active",
          "parent": {},
          "metadata": {
            "method": "POST",
            "url": "/_api/dump/start"
          }
        }
      ]).split('\n');
      assertEqual(lines.length, 2);
      assertTrue(lines.includes('── ActivityRegistryRestHandler: {"method":"GET","url":"/_admin/activity-registry"}'), JSON.stringify(lines));
      assertTrue(lines.includes('── RestDumpHandler: {"method":"POST","url":"/_api/dump/start"}'), JSON.stringify(lines));
    },
    testShowsDependenciesAsTrees: function () {
      const lines = activitiesModule.pretty_print([
        {
          "id": "0x76620a63b140",
          "name": "RestDumpHandler",
          "state": "Active",
          "parent": {},
          "metadata": {
            "method": "POST",
            "url": "/_api/dump/start"
          }
        },
        {
          "id": "0x766238412000",
          "name": "dump context",
          "state": "Active",
          "parent": {
            "id": "0x76620a63b140"
          },
          "metadata": {
            "database": "_system",
            "user": "root",
            "id": "dump-1855655160184832000"
          }
        }
      ]).split('\n');
      assertEqual(lines.length, 2);
      assertEqual(lines[0], '── RestDumpHandler: {"method":"POST","url":"/_api/dump/start"}');
      assertEqual(lines[1], '   └── dump context: {"database":"_system","user":"root","id":"dump-1855655160184832000"}');
    },

    testForest: function () {
      const forest = activitiesModule.createForest([
        { "id": "0", "parent": null },
        { "id": "1", "parent": "0" },
        { "id": "2", "parent": "0" },
        { "id": "3", "parent": "9" },
        { "id": "4", "parent": "3" },
        { "id": "5", "parent": "3" },
        { "id": "6", "parent": "5" },
      ]);
      assertEqual(forest.items, new Map([["0", { "id": "0", "parent": null, "children": [] }] , ["1", { "id": "1", "parent": "0" }], ["2", { "id": "2", "parent": "0" }], ["3", { "id": "3", "parent": "9" }], ["4", { "id": "4", "parent": "3" }], ["5", { "id": "5", "parent": "3" }], ["6", { "id": "6", "parent": "5" }]]));
      assertEqual(forest.roots, new Set(["0", "3"]));
      // assertEqual(forest.children, new Map([["0", ["1","2"]], 
      // print(JSON.stringify(forest));
      // items:
      // children: null:[0], 0:[1,2], 3:[4,5], 5:[6], 9:[3]

      // leaves: 0:[1,2], 1:[], 2:[], 3:[4,5], 4:[], 5:[6], 6:[] - items with children
      // roots: 0, 3 - all ids that are not children
    }
  };
}

jsunity.run(activityRegistryModuleSuite);
return jsunity.done();
