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
const { assertTrue, assertEqual } = jsunity.jsUnity.assertions;
const activitiesModule = require('@arangodb/activities');

function activityRegistryModuleSuite() {
  return {
    testPrintsOutASingleActivity: function () {
      const lines = activitiesModule.pretty_print([
        {
          "id" : "0x7a1c8743c780", 
          "type" : "ActivityRegistryRestHandler", 
          "parent" : {id: "0x0"}, 
          "metadata" : { 
            "method" : "GET", 
            "url" : "/_admin/activities" 
          } 
        }
      ]);
      assertEqual(lines, ' ── ActivityRegistryRestHandler: {"method":"GET","url":"/_admin/activities"}');
    },
    testPrintsOutSeparateActivities: function () {
      const lines = activitiesModule.pretty_print([
        {
          "id" : "0x7a1c8743c780", 
          "type" : "ActivityRegistryRestHandler", 
          "parent" : {id: "0x0"}, 
          "metadata" : { 
            "method" : "GET", 
            "url" : "/_admin/activities" 
          } 
        },
        {
          "id": "0x7b9050e26140",
          "type": "RestDumpHandler",
          "parent": {id: "0x0"},
          "metadata": {
            "method": "POST",
            "url": "/_api/dump/start"
          }
        }
      ]).split('\n');
      assertEqual(lines.length, 2);
      assertTrue(lines.includes(' ── ActivityRegistryRestHandler: {"method":"GET","url":"/_admin/activities"}'), JSON.stringify(lines));
      assertTrue(lines.includes(' ── RestDumpHandler: {"method":"POST","url":"/_api/dump/start"}'), JSON.stringify(lines));
    },

    testShowsDependenciesAsTrees: function () {
      const activities = [
        {
          "id": "0x76620a63b140",
          "type": "RestDumpHandler",
          "parent": {id: "0x0"},
          "metadata": {
            "method": "POST",
            "url": "/_api/dump/start"
          }
        },
        {
          "id": "0x766238412000",
          "type": "dump context",
          "parent": {id: "0x76620a63b140"},
          "metadata": {
            "database": "_system",
            "user": "root",
            "id": "dump-1855655160184832000"
          }
        }
      ];
      const lines = activitiesModule.pretty_print(activities).split('\n');
      assertEqual(lines.length, 2);
      assertEqual(lines[0], ' ── RestDumpHandler: {"method":"POST","url":"/_api/dump/start"}');
      assertEqual(lines[1], '    └── dump context: {"database":"_system","user":"root","id":"dump-1855655160184832000"}');
    },

    testPrettyConnectsChildrenInTrees: function () {
      const activities = [
        {
          "id": "1",
          "type": "1",
          "parent": {id: "0"},
          "metadata": {}
        },
        {
          "id": "2",
          "type": "2",
          "parent": {id: "1"},
          "metadata": {}
        },
        {
          "id": "3",
          "type": "3",
          "parent": {id: "1"},
          "metadata": {}
        },
         {
          "id": "4",
          "type": "4",
          "parent": {id: "2"},
          "metadata": {}
        },
         {
          "id": "5",
          "type": "5",
          "parent": {id: "2"},
          "metadata": {}
        }
    ];
      const lines = activitiesModule.pretty_print(activities).split('\n');
      assertEqual(lines.length, 5);
      assertEqual(lines[0], ' ── 1: {}');
      assertEqual(lines[1], '    ├── 3: {}');
      assertEqual(lines[2], '    └── 2: {}');
      assertEqual(lines[3], '        ├── 5: {}');
      assertEqual(lines[4], '        └── 4: {}');
    },

    testPrettyConnectsChildrenWithContinuationLineInTree: function () {
      const activities = [
        {
          "id": "1",
          "type": "1",
          "parent": {id: "0"},
          "metadata": {}
        },
        {
          "id": "2",
          "type": "2",
          "parent": {id: "1"},
          "metadata": {}
        },
        {
          "id": "3",
          "type": "3",
          "parent": {id: "1"},
          "metadata": {}
        },
        {
          "id": "4",
          "type": "4",
          "parent": {id: "2"},
          "metadata": {}
        },
        {
          "id": "5",
          "type": "5",
          "parent": {id: "3"},
          "metadata": {}
        },
        {
          "id": "6",
          "type": "6",
          "parent": {id: "3"},
          "metadata": {}
        },
        {
          "id": "7",
          "type": "7",
          "parent": {id: "6"},
          "metadata": {}
        },
        {
          "id": "8",
          "type": "8",
          "parent": {id: "1"},
          "metadata": {}
        }
      ];
      const lines = activitiesModule.pretty_print(activities).split('\n');
      assertEqual(lines.length, 8);
      assertEqual(lines[0], ' ── 1: {}');
      assertEqual(lines[1], '    ├── 8: {}');
      assertEqual(lines[2], '    ├── 3: {}');
      assertEqual(lines[3], '    │   ├── 6: {}');
      assertEqual(lines[4], '    │   │   └── 7: {}');
      assertEqual(lines[5], '    │   └── 5: {}');
      assertEqual(lines[6], '    └── 2: {}');
      assertEqual(lines[7], '        └── 4: {}');
    },

    testForest: function () {
      const forest = activitiesModule.createForest([
        { "id": "1", parent: {id: "0x0"}},
        { "id": "2", "parent": {id: "1"} },
        { "id": "3", "parent": {id: "1"} },
        { "id": "4", "parent": {id: "9"} },
        { "id": "5", "parent": {id: "4"} },
        { "id": "6", "parent": {id: "4"} },
        { "id": "7", "parent": {id: "6"} },
      ]);
      assertEqual(forest.items.size, 7);
      assertEqual(forest.items.get("1"), { "id": "1", parent: {id: "0x0"}, "children": ["2","3"] });
      assertEqual(forest.items.get("2"), { "id": "2", "parent": {id:"1"} , "children": []});
      assertEqual(forest.items.get("3"), { "id": "3", "parent": {id:"1"} , "children": []});
      assertEqual(forest.items.get("4"), { "id": "4", "parent": {id:"9"} , "children": ["5", "6"]});
      assertEqual(forest.items.get("5"), { "id": "5", "parent": {id:"4"} , "children": []});
      assertEqual(forest.items.get("6"), { "id": "6", "parent": {id:"4"} , "children": ["7"]});
      assertEqual(forest.items.get("7"), { "id": "7", "parent": {id:"6"} , "children": []});

      const roots = forest.roots();
      assertEqual(roots.size, 2);
      assertTrue(roots.has("1"));
      assertTrue(roots.has("4"));

      assertEqual(Array.from(forest.iter()).map(({hierarchy, item, continuations}) => ({hierarchy, item})), [
        {hierarchy: 0, item: {id: "1", parent: {id: "0x0"}, children: ["2", "3"]}},
        {hierarchy: 1, item: {id: "3", parent: {id: "1"}, children: []}},
        {hierarchy: 1, item: {id: "2", parent: {id: "1"}, children: []}},
        {hierarchy: 0, item: {id: "4", parent: {id: "9"}, children: ["5", "6"]}},
        {hierarchy: 1, item: {id: "6", parent: {id: "4"}, children: ["7"]}},
        {hierarchy: 2, item: {id: "7", parent: {id: "6"}, children: []}},
        {hierarchy: 1, item: {id: "5", parent: {id: "4"}, children: []}}
      ]);
    },

    testDFS: function () {
      const tree = new Map([
        ["0", {"id": "0", children: ["1", "2"]}],
        ["1", {"id": "1", children: []}],
        ["2", {"id": "2", children: []}],
        ["3", {"id": "3", children: ["4", "5"]}],
        ["4", {"id": "4", children: []}],
        ["5", {"id": "5", children: ["6"]}],
        ["6", {"id": "6", children: []}]
      ]);
      const DFS = new activitiesModule.DFS(tree);
      assertEqual(Array.from(DFS.iter("0")).map(({hierarchy, item, continuations}) => ({hierarchy, item})), [
        {hierarchy: 0, item: {"id": "0", children: ["1", "2"]}},
        {hierarchy: 1, item: {"id": "2", children: []}},
        {hierarchy: 1, item: {"id": "1", children: []}}
      ]);
      assertEqual(Array.from(DFS.iter("1")).map(({hierarchy, item, continuations}) => ({hierarchy, item})), [
        {hierarchy: 0, item: {"id": "1", children: []}}
      ]);
      assertEqual(Array.from(DFS.iter("2")).map(({hierarchy, item, continuations}) => ({hierarchy, item})), [
        {hierarchy: 0, item: {"id": "2", children: []}}
      ]);
      assertEqual(Array.from(DFS.iter("3")).map(({hierarchy, item, continuations}) => ({hierarchy, item})), [
        {hierarchy: 0, item: {"id": "3", children: ["4", "5"]}},
        {hierarchy: 1, item: {"id": "5", children: ["6"]}},
        {hierarchy: 2, item: {"id": "6", children: []}},
        {hierarchy: 1, item: {"id": "4", children: []}}
      ]);
      assertEqual(Array.from(DFS.iter("4")).map(({hierarchy, item, continuations}) => ({hierarchy, item})), [
        {hierarchy: 0, item: {"id": "4", children: []}}
      ]);
      assertEqual(Array.from(DFS.iter("5")).map(({hierarchy, item, continuations}) => ({hierarchy, item})), [
        {hierarchy: 0, item: {"id": "5", children: ["6"]}},
        {hierarchy: 1, item: {"id": "6", children: []}}
      ]);
      assertEqual(Array.from(DFS.iter("6")).map(({hierarchy, item, continuations}) => ({hierarchy, item})), [
        {hierarchy: 0, item: {"id": "6", children: []}}
      ]);
      assertEqual(Array.from(DFS.iter("100")).map(({hierarchy, item, continuations}) => ({hierarchy, item})), []);
    }

    
  };
}

jsunity.run(activityRegistryModuleSuite);
return jsunity.done();
