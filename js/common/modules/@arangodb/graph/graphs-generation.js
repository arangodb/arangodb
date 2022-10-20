/*jshint globalstrict:false, strict:false */
/*global assertTrue */
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

const communityGenerator = function (vColl, label) {
    return {
        makeEdge: function (from, to) {
            return {
                _from: `${vColl}/${label}_${from}`,
                _to: `${vColl}/${label}_${to}`,
                vertex: `${label}_${from}`
            };
        }, makeVertex: function (name) {
            return {
                _key: `${label}_${name}`,
                label: `${label}`
            };
        }
    };
};

const graphGenerator = function (verticesEdgesGenerator) {
    const {makeVertex, makeEdge} = verticesEdgesGenerator;

    const makeVertices = function (length) {
        let vertices = [];
        for (let i = 0; i < length; ++i) {
            vertices.push(makeVertex(i));
        }
        return vertices;
    };

    const makeOneVertex = function () {
        return makeVertices(1)[0];
    };

    const makeSingleVertexNoEdges = function () {
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
            edges.push(makeEdge(i, i + 1));
        }
        edges.push(makeEdge(length - 1, 0));
        return {vertices, edges};
    };


    const makeUnDirectedCycle = function (length) {
        if (length < 2) {
            console.error(`createDirectedCycle: error: length must be at least 2, instead got ${length}`);
            assertTrue(false);
        }
        let vertices = makeVertices(length);
        let edges = [];
        for (let i = 0; i < length - 1; ++i) {
            edges.push(makeEdge(i, i + 1));
            edges.push(makeEdge(i + 1, i));
        }
        edges.push(makeEdge(length - 1, 0));
        edges.push(makeEdge(0, length - 1));
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
    const makeClique = function (size, kind = "bidirected") {
        let vertices = makeVertices(size);
        let edges = [];
        for (let v = 0; v < size; ++v) {
            for (let w = v + 1; w < size; ++w) {
                switch (kind) {
                    case "bidirected":
                        edges.push(makeEdge(v, w));
                        edges.push(makeEdge(w, v));
                        break;
                    case "linear":
                        edges.push(makeEdge(v, w));
                        break;
                }
            }
        }
        return {vertices, edges};
    };

    // a wrapper to unify the call of createDirectedCycle, createAlternatingCycle, createFullBinaryTree
    const makeBidirectedClique = function (size) {
        return makeClique(size, "bidirected");
    };

    // Creates a path with length many vertices.
    // The parameter kind has the following meaning:
    //  - "directed" (default):  directed path
    //  - "bidirected": as "directed" but for every edge (v,w), we also have the edge (w,v)
    //  - "alternating": as "directed" but every second edge is inverted
    const makePath = function (length, kind = "directed") {
        let vertices = makeVertices(length);
        let edges = [];
        for (let v = 0; v < length - 1; ++v) {
            switch (kind) {
                case "directed":
                    edges.push(makeEdge(v, v + 1));
                    break;
                case "bidirected":
                    edges.push(makeEdge(v, v + 1));
                    edges.push(makeEdge(v + 1, v));
                    break;
                case "alternating":
                    const inverted = v % 2 === 0;
                    if (inverted) {
                        edges.push(makeEdge(v + 1, v));
                    } else {
                        edges.push(makeEdge(v, v + 1));
                    }
                    break;
            }

        }
        return {vertices, edges};
    };

    // Creates a star with numberLeaves many rays. The center has index 0.
    // The parameter kind has the following meaning:
    //  - "fromCenter":  the edges point from the center to the leaves
    //  - "toCenter":  the edges point from the leaves to the center
    //  - "bidirected" (default): as "fromCenter" but for every edge (v,w), we also have the edge (w,v)
    //  - "fromAndToCenter": the half of the edges point as with "fromCenter", the other half vice versa; if
    //      numberLeaves is odd, one more edge points from the center to a leaf
    const makeStar = function (numberLeaves, kind = "bidirected") {
        let vertices = makeVertices(numberLeaves + 1);
        let edges = [];
        switch (kind) {
            case "fromCenter":
                for (let v = 1; v <= numberLeaves; ++v) {
                    edges.push(makeEdge(0, v));
                }
                break;
            case "toCenter":
                for (let v = 1; v <= numberLeaves; ++v) {
                    edges.push(makeEdge(v, 0));
                }
                break;
            case "bidirected":
                for (let v = 1; v <= numberLeaves; ++v) {
                    edges.push(makeEdge(0, v));
                    edges.push(makeEdge(v, 0));
                }
                break;
            case "fromAndToCenter":
                for (let v = 1; v < Math.floor(numberLeaves / 2); ++v) {
                    edges.push(makeEdge(v, 0));
                }
                for (let v = Math.floor(numberLeaves / 2); v <= numberLeaves; ++v) {
                    edges.push(makeEdge(0, v));
                }
                break;
        }
        return {vertices, edges};
    };

    /**
     * Creates a grid with dimensions numberLayers x thickness. If parameter kind is "directed", the edges go from
     * Layer i to Layer i+1. Edges between vertices of the same layer all go to the same direction and edges of two
     * layers go to the same direction. If parameter kind is "zigzag", edges within one layer go to the same direction,
     * but edges between neighbor layers go to the opposite directions. If parameter
     * kind is "bidirected", the graph is bidirected. The vertices are enumerated layer-wise: the first layer gets
     * indexes 0, 1, ..., thickness - 1, the second one thickness, thickness + 1, 2*thickness -1 etc.
     * @param numberLayers the number of layers between source and target
     * @param thickness the number of vertices in each layer
     * @param kind "directed", "zigzag" or "bidirected"
     */

    const makeGrid = function (numberLayers, thickness, kind) {
        assertTrue(numberLayers > 1,
            `makeGrid: numberLayers should be at least 2, it is ${numberLayers}`);
        assertTrue(thickness > 1,
            `makeGrid: thickness should be at least 2, it is ${thickness}`);
        assertTrue(kind === "directed" || kind === "zigzag" || kind === "bidirected",
            `makeGrid: kind should one of {"directed", "zigzag", "bidirected"}, it is ${kind}`);
        let vertices = makeVertices(numberLayers * thickness);
        let edges = [];
        for (let layer = 0; layer < numberLayers; ++layer) {
            // edges within one layer
            for (let i = 0; i < thickness - 1; ++i) {
                const s = layer * thickness + i;
                const t = s + 1;
                switch (kind) {
                    case "directed":
                        edges.push(makeEdge(s, t));
                        break;
                    case "zigzag":
                        if (layer % 2 === 0) {
                            edges.push(makeEdge(s, t));
                        } else {
                            edges.push(makeEdge(t, s));
                        }
                        break;
                    case "bidirected":
                        edges.push(makeEdge(s, t));
                        edges.push(makeEdge(t, s));
                        break;
                }
            }
            // edges between layers: from previous layer to current layer (and back if "bidirected")
            if (layer === 0) {
                continue;
            }
            for (let i = 0; i < thickness; ++i) {
                const t = layer * thickness + i;
                const s = t - thickness;
                edges.push(makeEdge(s, t));
                if (kind === "bidirected") {
                    edges.push(makeEdge(t, s));
                }
            }
        }
        return {vertices, edges};
    };

    /**
     * Creates a graph consisting of a grid with dimensions numberLayers x thickness and, additionally,
     * two vertices source and target. Vertex source is connected to each of thickness many vertices of the first layer.
     * Each of thickness many vertices of the last layer is connected to vertex target. If parameter kind is "directed",
     * the edges go from source to the first layer, from Layer i to Layer i+1 and from the last layer to target. Edges
     * between vertices of the same layer all go to the same direction and edges of two layers go to the same direction.
     * If parameter kind is "zigzag", edges between source, the layers and target are as for "directed", edges within
     * one layer go to the same direction, but edges between neighbor layers go to the opposite directions. If parameter
     * kind is "bidirected", the graph is bidirected. The vertices are enumerated layer-wise: the first layer gets
     * indexes 0, 1, ..., thickness - 1, the second one thickness, thickness + 1, 2*thickness -1 etc. Source gets index
     * numberLayers x thickness and target gets index numberLayers x thickness + 1.
     * @param numberLayers the number of layers between source and target
     * @param thickness the number of vertices in each layer
     * @param kind "directed", "zigzag" or "bidirected"
     */
    const makeCrystal = function (numberLayers, thickness, kind) {
        let {vertices, edges} = makeGrid(numberLayers, thickness, kind);
        const source = vertices.length;
        const target = vertices.length + 1;
        vertices.push(makeVertex(source));
        vertices.push(makeVertex(target));
        for (let i = 0; i < thickness; ++i) {
            edges.push(makeEdge(source, i));
            edges.push(makeEdge(numberLayers * (thickness - 1) + i, target));
        }
        return {vertices, edges};
    };


    return {
        makeVertex,
        makeVertices,
        makeOneVertex,
        makeSingleVertexNoEdges,
        makeDirectedCycle,
        makeUnDirectedCycle,
        makeAlternatingCycle,
        makeFullBinaryTree,
        makeClique,
        makeBidirectedClique,
        makePath,
        makeStar,
        makeGrid,
        makeCrystal
    };
};

const makeEdgeBetweenVertices = function (vColl, from, fromLabel, to, toLabel) {
    return {
        _from: `${vColl}/${fromLabel}_${from}`,
        _to: `${vColl}/${toLabel}_${to}`,
        vertex: `${fromLabel}_${from}`
    };
};

/**
 * Make the graph that is the disjoint union of given subgraphs. it is assumed that the
 * subgraphs are disjoint.
 * @param subgraphs a document containing "vertices" and "edges"
 * @returns {{vertices: *[], edges: *[]}}
 */
const unionGraph = function (subgraphs) {
    let vertices = [];
    for (const subgraph of subgraphs) {
        vertices = vertices.concat(subgraph.vertices);
    }
    let edges = [];
    for (const subgraph of subgraphs) {
        edges = edges.concat(subgraph.edges);
    }
    return {vertices, edges};
};

/**
 * Prints the topology of a graph (for manual testing of generated graphs).
 * @param vertices
 * @param edges
 */
const printTopology = function (vertices, edges) {
    console.warn("VERTICES:");
    for (const v of vertices) {
        console.warn(`    ${v._key}`);
    }
    console.warn("EDGES:");
    for (const e of edges) {
        console.warn(`    ${e._from.substr(e._from.indexOf('/') + 1)} ${e._to.substr(e._to.indexOf('/') + 1)}`);
    }
};

const loadGraphGenerators = function (isSmart) {
    if (isSmart) {
        return {
            makeEdgeBetweenVertices:
            require("@arangodb/graph/graphs-generation-enterprise").enterpriseMakeEdgeBetweenVertices,
            verticesEdgesGenerator:
            require("@arangodb/graph/graphs-generation-enterprise").enterpriseGenerator
        };
    }
    return {
        makeEdgeBetweenVertices: makeEdgeBetweenVertices,
        verticesEdgesGenerator: communityGenerator
    };
};

class Vertex {

    constructor(key, label, value = 0) {
        this.outEdges = [];
        this.outNeighbors = new Set();
        this.inEdges = [];
        this.inNeighbors = new Set();
        this._key = key;
        this.label = label;
        this.value = value;
    }

    outDegree() {
        return this.outNeighbors.size;
    }
}

class Graph {

    vertex(key) {
        return this.vertices.get(key);
    }

    printVertices(onlyKeys = false) {
        if (onlyKeys) {
            let listVertices = [];
            for (const [vKey,] of this.vertices) {
                listVertices.push(vKey);
            }
            console.warn(listVertices);
        } else {
            console.warn(`Vertices: `);
            for (const [vKey, v] of this.vertices) {
                console.warn(`  ${vKey} : `);
                console.warn(`    label: ${v.label}`);
                console.warn(`    value: ${JSON.stringify(v.value)}$`);
                console.warn(`    outNeighbs: ${JSON.stringify(v.outNeighbors)}`);
                console.warn(`    inNeighbs:  ${JSON.stringify(v.inNeighbors)}`);
            }
            console.warn("End vertices");
        }
    }

    /**
     * Constructs a graph.
     * @param vertices Array of objects containing attributes "_key" (unique vertex key) and "result".
     * @param edges Array of objects representing edges, each object should contain attributes "_from" and "_to"
     *      and for each value of "_from" and "_to" there should be a vertex whose _key is this value.
     */
    constructor(vertices, edges) {
        const getKey = function (v) {
            return v.substr(v.indexOf('/') + 1);
        };

        this.vertices = new Map(); // maps vertex key to the vertex object
        for (const v of vertices) {
            this.vertices.set(v._key, new Vertex(v._key, v.label));
        }

        this.edges = new Set();
        for (const e of edges) {
            const from = getKey(e._from);
            const to = getKey(e._to);
            assertTrue(this.vertices.has(from),
                `vertices do not contain the _from of ${JSON.stringify(e)} (which is ${from})`);
            assertTrue(this.vertices.has(`${to}`),
                `vertices do not contain the _to of ${JSON.stringify(e)} (which is ${to})`);

            this.edges.add(e);

            this.vertices.get(from).outEdges.push(e);
            this.vertices.get(to).inEdges.push(e);
            if (from !== to) {
                this.vertices.get(from).outNeighbors.add(to);
                this.vertices.get(to).inNeighbors.add(from);
            }
        }
        // this.printVertices();
    }


    /**
     * Performs BFS from source, calls cbOnFindVertex on a vertex when the vertex is reached and cbOnPopVertex
     * when the vertex is popped from the queue.
     * @param source The key of the vertex to start the search from
     * @param cbOnFindVertex Either undefiend or the callback function to run on a vertex
     *              that is being pushed on the queue. The function can have one or two arguments:
     *              the vertex being pushed and its parent in the search. The second argument can be undefined.
     * @param cbOnPopVertex Either undefined or the callback function to run on a vertex
     *              that is being popped from the queue. The function must have one argument: the vertex being popped.
     */
    bfs(source, cbOnFindVertex, cbOnPopVertex) {
        assertTrue(typeof (source) === `string`, `${source} is not a string`);
        assertTrue(this.vertices.has(`${source}`), `bfs: the given source "${JSON.stringify(source)}" is not a vertex`);
        let visited = new Set();
        visited.add(source);
        if (cbOnFindVertex !== undefined) {
            cbOnFindVertex(this.vertex(source));
        }
        let queue = [source];
        while (queue.length !== 0) {
            const vKey = queue.shift();
            const v = this.vertex(vKey);
            if (cbOnPopVertex !== undefined) {
                cbOnPopVertex(v);
            }
            for (const wKey of v.outNeighbors) {
                if (!visited.has(wKey)) {
                    visited.add(wKey);
                    if (cbOnFindVertex !== undefined) {
                        cbOnFindVertex(this.vertex(wKey), v);
                    }
                    queue.push(wKey);
                }
            }
        }
    }

    /**
     * Performs DFS from source, calls cbOnFindVertex on a vertex when the vertex is reached and cbOnPopVertex
     * when the vertex is popped from the queue.
     */
    dfs(source, cbOnFindVertex, cbOnPopVertex) {
        assertTrue(this.vertices.has(source), `dfs: the given source "${source}" is not a vertex`);
        let visited = new Set();
        visited.add(source);
        let stack = [source];
        if (cbOnFindVertex !== undefined) {
            cbOnFindVertex(this.vertex(source));
        }
        while (stack.length !== 0) {
            const vKey = stack.pop();
            const v = this.vertex(vKey);
            if (cbOnPopVertex !== undefined) {
                cbOnPopVertex(v);
            }
            for (const wKey of v.outNeighbors) {
                if (!visited.has(wKey)) {
                    visited.add(wKey);
                    if (cbOnFindVertex !== undefined) {
                        cbOnFindVertex(this.vertex(wKey), v);
                    }
                    stack.push(wKey);
                }
            }
        }
    }
}


exports.communityGenerator = communityGenerator;
exports.graphGenerator = graphGenerator;
exports.makeEdgeBetweenVertices = makeEdgeBetweenVertices;
exports.unionGraph = unionGraph;
exports.printTopology = printTopology;
exports.unionGraph = unionGraph;
exports.loadGraphGenerators = loadGraphGenerators;
exports.Graph = Graph;