/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global AQL_EXPLAIN, assertTrue, assertFalse, assertEqual */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for optimizer rule that transforms last access on path to vertex and edge
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const gm = require("@arangodb/general-graph");
const {db} = require("@arangodb");
const ruleName = "optimize-traversal-last-element-access";

function TraversalOptimizeLastPathAccessTestSuite() {
    const graphName = "UnitTestGraph";
    const vc = "UnitTestVertexCollection";
    const ec = "UnitTestEdgeCollection";
    const startVertex = "UnitTestVertexCollection/1";

    const injectData = () => {
        // There is nothing special to this dataset, just something that matches the filters as used in the tests later;
        // The only thing: We have the startVertex included
        const vertices = [];
        for (let i = 0; i < 100; ++i) {
            vertices.push({_key: `${i}`, name: `${i % 2 === 0 ? "test" : "foo"}`});
        }
        db[vc].save(vertices);

        const edges = [];
        for (let i = 0; i < 100; ++i) {
            for (let j = 0; j < 100; j += i + 2) {
                edges.push({_from: `${vc}/${i}`, _to: `${vc}/${j}`, label: `${i % 2 === 0 ? "test" : "foo"}`, timestamp: j});
            }
        }
        db[ec].save(edges);
    };

    const makePath = (length, keyOffset = 0) => {
        const vertices = [];
        const edges = [];
        for (let i = keyOffset; i < keyOffset + length; ++i) {
            vertices.push({_key: `${i}`, _id: `v/${i}`});
            if (i > 0) {
                edges.push({_key: `${i - 1}-${i}`, _id: `e/${i - 1}-${i}`, _from: `v/${i - 1}`, _to: `v/${i}`});
            }
        }
        return {vertices, edges};
    };
    // These paths are used to "simulate" traversal output, to make sure optimizer does not do funny stuff
    const hardCodedPaths = [
        makePath(1, 0),
        makePath(2, 0),
        makePath(3, 0),
        makePath(1, 10),
        makePath(2, 10),
        makePath(3, 10),
    ];

    const isReferenceToPath = (refNode) => {
        const {name, type} = refNode;
        // Only Reference(45) to variable (p) are allowed
        return type === "reference" && name === "p";
    };
    const isLastElementAccess = (indexedAccess) => {
        if (indexedAccess.subNodes.length !== 2) {
            // Index access needs exactly 2 members
            return false;
        }
        const [path, index] = indexedAccess.subNodes;
        {
            // Test index
            const {value, type} = index;
            if (type !== "value" || value !== -1) {
                // Only type "value"(40) and access to last element "-1" can pass
                return false;
            }
        }
        {
            // Test attribute access path
            const {name, subNodes, type} = path;
            if (type !== "attribute access" || !(name === "vertices" || name === "edges") || subNodes.length !== 1 || !isReferenceToPath(subNodes[0])) {
                return false;
            }
        }
        return true;
    };

    const planContainsPathAccess = (nodes) => {
        // Pick all Indexed Access nodes (`LET v = <var>[<index>]`
        const indexedAccesses = nodes.filter(n => n.type === "CalculationNode").map(c => c.expression).filter(e => e.type === "indexed access");
        return indexedAccesses.filter(isLastElementAccess).length > 0;
    };
    const ruleIsApplied = (rules) => {
        return rules.indexOf(ruleName) !== -1;
    };

    const assertPlanContainsPathAccess = (nodes) => {
        assertTrue(planContainsPathAccess(nodes), `We do not have access to path variable p`);
    };
    const assertPlanDoesNotContainPathAccess = (nodes) => {
        assertFalse(planContainsPathAccess(nodes), `We still have access to path variable p`);
    };
    const assertRuleIsApplied = (rules) => {
        assertTrue(ruleIsApplied(rules), `Rule ${ruleName} not found in ${rules}`);
    };
    const assertRuleIsNotApplied = (rules) => {
        assertFalse(ruleIsApplied(rules), `Rule ${ruleName} has been found in ${rules}`);
    };

    const assertPlanDoesNotContainFilter = (nodes) => {
        const filters = nodes.filter(n => n.type === "FilterNode");
        assertEqual(filters.length, 0, `We still have a FILTER node`);
    };

    const assertResultsAreNotModifiedByRule = (query) => {
        // Ordering in the result is not relevant for this test.
        const sortResults = (a, b) => {
            if (a._key < b._key) {
                return -1;
            }
            if (a._key > b._key) {
                return 1;
            }
            return 0;
        };
        const originalResult = db._query(query,{},{optimizer:{rules: [`-${ruleName}`]}}).toArray().sort(sortResults);
        const optimizedResult = db._query(query,{},{optimizer:{rules: [`+${ruleName}`]}}).toArray().sort(sortResults);
        assertEqual(originalResult, optimizedResult);
    };

    const assertHasProjection = (nodes, type, attribute) => {
        const traversalNodes = nodes.filter(n => n.type === "TraversalNode");
        assertEqual(traversalNodes.length, 1, `We do not have expected number of Traversal Nodes, please check the test query, it should contain exactly one traversal statement`);
        const projections = traversalNodes[0].options[type];
        assertTrue(Array.isArray(projections), `Projections not found, or unexpected format`);
        assertEqual(projections.filter(p => p === attribute).length, 1, `Expected Projection ${attribute} not found in ${projections}`);
    };

    return {
        setUpAll: function () {
            gm._create(graphName, [gm._relation(ec, vc, vc)], [], {numberOfShards: 3});
            injectData();
        },

        tearDownAll: function () {
            gm._drop(graphName, true);
        },

        testShouldNotImpactNonTraversals: function () {
            const queriesToTest = [
                `FOR p IN ${JSON.stringify(hardCodedPaths)} RETURN p.vertices[-1]`,
                `FOR p IN ${JSON.stringify(hardCodedPaths)} RETURN p.edges[-1]`,
                `FOR p IN ${vc} RETURN p.vertices[-1]`,
                `FOR p IN ${vc} RETURN p.edges[-1]`
            ];
            for (const q of queriesToTest) {
                // We need to exclude the move filter into rule, otherwise we cannot simply find the path access anymore
                const {nodes, rules} = AQL_EXPLAIN(q).plan;
                assertPlanContainsPathAccess(nodes);
                assertRuleIsNotApplied(rules);

            }
        },

        testShouldImpactTraversals: function () {
            const queriesToTest = [
                `FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" GRAPH "${graphName}" RETURN p.vertices[-1]`,
                `FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" GRAPH "${graphName}" RETURN p.edges[-1]`,
                `WITH ${vc} FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} RETURN p.vertices[-1]`,
                `WITH ${vc} FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} RETURN p.edges[-1]`,
                `WITH ${vc} FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} FILTER p.vertices[-1].name == "test" RETURN v`,
                `WITH ${vc} FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} FILTER p.edges[-1].label == "foo" RETURN e`,
                `WITH ${vc} FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} FILTER p.edges[-1].label == "foo" && p.vertices[-1].name == "test" RETURN v`,
                `WITH ${vc} FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} SORT p.edges[-1].timestamp RETURN e`,
            ];
            for (const q of queriesToTest) {
                const {nodes, rules} = AQL_EXPLAIN(q).plan;
                assertPlanDoesNotContainPathAccess(nodes);
                assertRuleIsApplied(rules);
                assertResultsAreNotModifiedByRule(q);
            }
        },

        testCombinationWithProjection: function () {
            const queriesToTest = [
                [`FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} RETURN p.vertices[-1].name`,"vertexProjections", "name"],
                [`FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} RETURN p.edges[-1].label`, "edgeProjections", "label"],
            ];
            for (const [q, type, attribute] of queriesToTest) {
                const {nodes, rules} = AQL_EXPLAIN(q).plan;
                assertPlanDoesNotContainPathAccess(nodes);
                assertRuleIsApplied(rules);
                assertHasProjection(nodes, type, attribute);
            }
        },

        testCombinationWithInlineFilter: function () {
            const queriesToTest = [
                `FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} FILTER p.vertices[-1].name == "test" RETURN v`,
                `FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} FILTER p.edges[-1].label == "foo" RETURN e`
            ];
            for (const q of queriesToTest) {
                const {nodes, rules} = AQL_EXPLAIN(q).plan;
                assertPlanDoesNotContainPathAccess(nodes);
                assertRuleIsApplied(rules);
                assertPlanDoesNotContainFilter(nodes);
            }
        },
    };
}


jsunity.run(TraversalOptimizeLastPathAccessTestSuite);

return jsunity.done();
