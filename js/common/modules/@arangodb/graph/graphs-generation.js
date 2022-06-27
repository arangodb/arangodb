/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual, JSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Pregel Tests: graph generation
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
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
// / @author Roman Rabinovich
// //////////////////////////////////////////////////////////////////////////////


var graph_module = require("@arangodb/general-graph");

const graphGenerator = function (generatorBase, name, label) {
    const {makeVertex, makeEdge} = generatorBase.with_label(label);

    const makeVertices = function (length) {
        let vertices = [];
        for (let i = 0; i < length; ++i) {
            vertices.push(makeVertex(`${name}_${i}`));
        }
        return vertices;
    };

    const makeSingleVertex = function () {
        const vertices = makeVertices(1);
        const edges = [];
        return {vertices, edges};
    };

    // length must be at least 2, shorter cycles make no sense and throw
    // because then the test is wrong
    const makeDirectedCycle = function (length) {
        if (length < 2) {
            console.error(`createDirectedCycle: error: length must be at least 2, instead got ${length}`);
            assertTrue(false);
        }
        let vertices = makeVertices(length);
        let edges = [];
        for (let i = 0; i < length - 1; ++i) {
            edges.push(makeEdge(vertices[i]._key, vertices[i + 1]._key));
        }
        edges.push(makeEdge(vertices[length - 1]._key, vertices[0]._key));
        return {vertices, edges};
    };

    // An alternating cycle is obtained from a directed cycle
    // by replacing every second edge (v,w) by (w,v).
    // Note that if length is odd,
    // there may be a sub-path of length 2: (length-1) -> 0 -> 1.
    const makeAlternatingCycle = function (length) {
        if (length < 2) {
            return {};
        }
        let vertices = makeVertices(length);
        let edges = [];
        for (let i = 0; i < length - 1; ++i) {
            if (i % 2 === 0) {
                edges.push(makeEdge(i + 1, i));
            } else {
                edges.push(makeEdge(i, i + 1));
            }
        }
        // special case: if length == 2, this would produce the edge (1, 0), but it has been already produced above
        if (length > 2) {
            if ((length - 1) % 2 === 0) {
                edges.push(makeEdge(0, length - 1));
            } else {
                edges.push(makeEdge(length - 1, 0));
            }
        }
        return {vertices, edges};
    };

    // Creates a full binary tree of depth treeDepth (0 means: only the root) with edges directed away from the root.
    // If alternating is true, on every second level of edges starting from the first, the edges are inverted:
    // (v,w) is replaced by (w,v). (I.e., if alternating is true, two edges point to the root of the tree.)
    const makeFullBinaryTree = function (treeDepth, alternating = false) {
        if (treeDepth === 0) {
            return {vertices: [0], edges: []};
        }
        if (treeDepth > 14) {
            console.warn(`createFullBinaryTree WARNING: creating ${Math.pow(2, treeDepth + 1) - 1} vertices!`);
        }
        let vertices = makeVertices(Math.pow(2, treeDepth + 1) - 1);
        // We create edges level by level top-down.
        // variable firstFree: the least index of a vertex not yet connected by an edge.
        // variable leaves: the set of current leaves (vertices on the lowest connected level). Acts as a FIFO queue.
        // We always add an edge from the first leaf to firstFree (and update them).
        let edges = [];
        let firstFree = 1; // vertex with index 1 exists as treeDepth is > 0 here
        let leaves = [0];
        for (let d = 0; d < treeDepth; ++d) {
            let leavesNestLevel = [];
            const inverted = alternating && d % 2 === 0;
            for (const leaf of leaves) {
                if (inverted) {
                    edges.push(makeEdge(firstFree, leaf));
                } else {
                    edges.push(makeEdge(leaf, firstFree));
                }
                ++firstFree;
                if (inverted) {
                    edges.push(makeEdge(firstFree, leaf));
                } else {
                    edges.push(makeEdge(leaf, firstFree));
                }
                ++firstFree;
                // the last level of vertices is not collected to leavesNestLevel: its vertices have no children
                if (d <= treeDepth - 2) {
                    leavesNestLevel.push(firstFree - 2);
                    leavesNestLevel.push(firstFree - 1);
                }
                leaves = leavesNestLevel;
            }
        }
        return {vertices, edges};
    };

    // Creates a graph with size many vertices such that for each pair (v,w) of distinct vertices,
// (v,w) or (w,v) is an edge (or both). There are no self-loops.
// The parameter kind has the following meaning:
//  - "bidirected": for all distinct vertices v,w, there are edges (v,w) and (w,v)
//  - "linear": the edges constitute a linear order <:
//              there is an edge from v to w if and only if index of v < index of w.
// Note that the number of edges grows quadratically in size!
    const makeClique = function (size, vColl, name_prefix, kind = "bidirected") {
        let vertices = makeVertices(size, name_prefix);
        let edges = [];
        for (let v = 0; v < size; ++v) {
            for (let w = v + 1; w < size; ++w) {
                switch (kind) {
                    case "bidirected":
                        edges.push(makeEdge(v, w, vColl, name_prefix));
                        edges.push(makeEdge(w, v, vColl, name_prefix));
                        break;
                    case "linear":
                        edges.push(makeEdge(v, w, vColl, name_prefix));
                        break;
                }
            }
        }
        return {vertices, edges};
    };

    // a wrapper to unify the call of createDirectedCycle, createAlternatingCycle, createFullBinaryTree
    const makeBidirectedClique = function (size, vColl, name_prefix) {
        return makeClique(size, vColl, name_prefix, "bidirected");
    };

    return {
        makeVertices,
        makeSingleVertex,
        makeDirectedCycle,
        makeAlternatingCycle,
        makeFullBinaryTree,
        makeClique,
        makeBidirectedClique
    };

};

exports.graphGenerator = graphGenerator;
