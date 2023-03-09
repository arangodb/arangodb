/* jshint esnext: true */
/* global assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for the AQL FOR x IN GRAPH name statement
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2023 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {debug} = require('@arangodb/aql/explainer');
const {db} = require("@arangodb");


function explainerDebugOneShardBaseSuite(dbSharding) {
    const testGraphs = [];
    const testViews = [];


    const makeGraph = (type) => {
        const name = type;
        const options = {
            numberOfShards: 3
        };
        const module = (() => {
            if (type === "Smart" || type === "DisjointSmart"){
                options.isSmart = true;
                options.smartGraphAttribute = "sga";
                options.isDisjoint = type === "DisjointSmart";
                return require("@arangodb/smart-graph");
            }
            if ( type === "Enterprise") {
                options.isSmart = true;
                return require("@arangodb/enterprise-graph");
            }
            if (type === "Satellite") {
                options.numberOfShards = 1;
                return require("@arangodb/satellite-graph");
            }
            return require("@arangodb/general-graph");
        })();
        const graph = `Unittest${name}Graph`;
        const vertex = `Unittest${name}Vertex`;
        const startVertex = `${vertex}/1`;
        const edge = `Unittest${name}Edge`;
        const orphan = `Unittest${name}Orphan`;
        const relations = [module._relation(edge, vertex, vertex)];

        const setup = () => {
            module._create(graph, relations, [orphan], options);
            require("internal").print(db._graphs.document(graph));
        };

        const tearDown = () => {
            try {
                module._drop(graph, true);
            } catch (e) {
            }
        };
        return {setup, tearDown, graph, vertex, edge, orphan, name, startVertex};
    };

    testGraphs.push(makeGraph("Community"));
    testGraphs.push(makeGraph("Satellite"));
    testGraphs.push(makeGraph("Smart"));
    testGraphs.push(makeGraph("DisjointSmart"));
    testGraphs.push(makeGraph("Enterprise"));

    const makeView = () => {
        // TODO: if we have different views, we need to give other names
        const name = "";
        const col1 = `UnittestViewCol1`;
        const col2 = `UnittestViewCol2`;
        const view = `UnittestView`;

        const setup = () => {
            const colOptions = {numberOfShards: 3};
            db._create(col1, colOptions);
            db._create(col2, colOptions);
            db._createView(view, "arangosearch", {
                links: {
                    [col1]: {includeAllFields: true},
                    [col2]: {includeAllFields: true}
                }
            });
        };

        const tearDown = () => {
            try {
                db._dropView(view);
            } catch (e) {}
            try {
                db._drop(col2);
            } catch (e) {}
            try {
                db._drop(col1);
            } catch (e) {}
        };
        return {setup, tearDown, col1, col2, view, name};
    };

    testViews.push(makeView());

    const suite = {
        setUpAll: function () {
            db._createDatabase("UnitTestDatabase", {sharding: dbSharding});
            db._useDatabase("UnitTestDatabase");
            for (const g of testGraphs) {
                g.setup();
            }
            for (const v of testViews) {
                v.setup();
            }

        },
        tearDownAll: function () {
            for (const g of testGraphs) {
                g.tearDown();
            }
            for (const v of testViews) {
                v.tearDown();
            }
            db._useDatabase("_system");
            db._dropDatabase("UnitTestDatabase");
        }
    };

    const checkOnlyExpectedCollectionsInDump = (collections, expected) => {
        for (const name of Object.keys(collections)) {
            assertTrue(expected.has(name), `Expecting collection: "${name}", but only have ${JSON.stringify([...expected.keys()])}`);
            // We do not really care that we get correct collection types
            // We just want to make sure they are referenced properly
            expected.delete(name);
        }
        assertEqual(expected.size, 0, `Did not reference the following collections ${JSON.stringify([...expected.keys()])}`);
    };
    const checkOnlyExpectedGraphsInDump = (graphs, expected) => {
        for (const name of Object.keys(graphs)) {
            assertTrue(expected.has(name), `Found graph: "${name}", but only expecting ${JSON.stringify([...expected.keys()])}`);
            expected.delete(name);
        }
        assertEqual(expected.size, 0, `Did not reference the following graphs ${JSON.stringify([...expected.keys()])}`);
    };
    const checkOnlyExpectedViewsInDump = (views, expected) => {
        for (const name of Object.keys(views)) {
            assertTrue(expected.has(name), `Found view: "${name}", but only expecting ${JSON.stringify([...expected.keys()])}`);
            expected.delete(name);
        }
        assertEqual(expected.size, 0, `Did not reference the following views ${JSON.stringify([...expected.keys()])}`);
    };

    for (const g of testGraphs) {
        suite[`test${dbSharding}Traversal${g.name}`] = function() {
            const query = `
              FOR v IN 1 OUTBOUND "${g.startVertex}" GRAPH ${g.graph}
              RETURN v`;
            const {collections, graphs, views} = debug(query);
            require("internal").print(graphs);
            checkOnlyExpectedCollectionsInDump(collections, new Set([g.vertex, g.edge, g.orphan]));
            checkOnlyExpectedGraphsInDump(graphs, new Set([g.graph]));
            checkOnlyExpectedViewsInDump(views, new Set([]));
        };

        suite[`test${dbSharding}Traversal${g.name}Filter`] = function() {
            const query = `
              FOR v IN 1 OUTBOUND "${g.startVertex}" GRAPH ${g.graph}
              FILTER v.foo == "bar"
              RETURN v`;
            const {collections, graphs, views} = debug(query);
            checkOnlyExpectedCollectionsInDump(collections, new Set([g.vertex, g.edge, g.orphan]));
            checkOnlyExpectedGraphsInDump(graphs, new Set([g.graph]));
            checkOnlyExpectedViewsInDump(views, new Set([]));
        };

        suite[`test${dbSharding}Traversal${g.name}_anonymous`] = function() {
            const query = `
              FOR v IN 1 OUTBOUND "${g.startVertex}" ${g.edge}
              RETURN v`;
            const {collections, graphs, views} = debug(query);
            checkOnlyExpectedCollectionsInDump(collections, new Set([g.edge]));
            checkOnlyExpectedGraphsInDump(graphs, new Set([]));
            checkOnlyExpectedViewsInDump(views, new Set([]));
        };
    }

    for (const v of testViews) {
        suite[`test${dbSharding}View${v.name}`] = function () {
            const query = `
              FOR v IN ${v.view}
              RETURN v`;
            const {collections, graphs, views} = debug(query);
            checkOnlyExpectedCollectionsInDump(collections, new Set([v.col1, v.col2]));
            checkOnlyExpectedGraphsInDump(graphs, new Set([]));
            checkOnlyExpectedViewsInDump(views, new Set([v.view]));
        };
    }

    return suite;
}

function explainerDebugSuite() {
    return explainerDebugOneShardBaseSuite("");
}
function explainerDebugOneShardSuite () {
    return explainerDebugOneShardBaseSuite("single");
}

jsunity.run(explainerDebugSuite);
jsunity.run(explainerDebugOneShardSuite);
return jsunity.done();