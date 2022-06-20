/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual, JSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Pregel Tests
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
// / @author Simon Grätzer
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var graph_module = require("@arangodb/general-graph");
var internal = require("internal");
var console = require("console");
let pregel = require("@arangodb/pregel");
let graphGeneration = require("@arangodb/graph/graphs-generation");
const {makeEdge} = require("../../../../js/common/modules/@arangodb/graph/graphs-generation");

// TODO make this more flexible: input maxWaitTimeSecs and sleepIntervalSecs
const pregelRunSmallInstance = function (algName, graphName, parameters) {
  const pid = pregel.start("wcc", graphName, parameters);
  const maxWaitTimeSecs = 120;
  const sleepIntervalSecs = 0.2;
  let wakeupsLeft = maxWaitTimeSecs / sleepIntervalSecs;
  while (pregel.status(pid).state !== "done" && wakeupsLeft > 0) {
    wakeupsLeft--;
    internal.sleep(0.2);
  }
  assertEqual(pregel.status(pid).state, "done", "Pregel Job did never succeed.");

  // Now test the result.
  const query = `
        FOR v IN ${vColl}
          COLLECT component = v.result WITH COUNT INTO size
          SORT size DESC
          RETURN {component, size}
      `;
  return db._query(query).toArray();
};

// componentsSizes should be an array of distinct sizes for components
// createComponent should be a function that gets (<number of vertices>, <vertex collection name>, <name_prefix>)
// and returns {vertices: <array of vertices>, edges: <array of edges>} where the graph induced by vertices and edges
// is, indeed , a component (of whatever kind)
const testComponentsAlgorithmOnDisjointComponents = function(componentSizes, createComponent, algorithm_name = "wcc") {
  // we expect that '_' appears in the string (should it not, return the whole string)
  const extract_name_prefix = function(v) {
    return v.substr(0, v.indexOf('_'));
  };

  // expected components: [2_0, 2_1], [10_0,..., 10_9], [5_0,...,5_4], [23_0,...,23_22]
  // We need for the test later that all sizes are distinct.
  // Let's check that we didn't make a mistake.
  const sortedComponentSizes = componentSizes.sort(function(a, b) { return b - a;});
  for (let i = 0; i < sortedComponentSizes.size - 1; ++i) {
    assertNotEqual(sortedComponentSizes[i], sortedComponentSizes[i+1],
        `Test error: component sizes should be pairwise distinct, instead got ${componentSizes}`);
  }

  // Produce the graph.
  for (let i of sortedComponentSizes) {
    let {vertices, edges} = createComponent(i, vColl, i.toString());
    db[vColl].save(vertices);
    db[eColl].save(edges);
  }

  // Get the result of Pregel's algorithm.
  const computedComponents = pregelRunSmallInstance(algorithm_name, graphName, { resultField: "result", store: true });
  // assertEqual(computedComponents.length, sortedComponentSizes.length,
  //     `We expected ${sortedComponentSizes.length} components, instead got ${JSON.stringify(computedComponents)}`);

  // Test component sizes
  // Note that query guarantees that computedComponents is sorted
  // for (let i = 0; i < computedComponents.length; ++i) {
  //   assertEqual(computedComponents[i].size, sortedComponentSizes[i]);
  // }

  // Test that components contain correct vertices:
  //   we saved the components such that a component of size s has vertices
  //   [`${s}_0`,..., `${s}_${s-1}`].
  // We use that all sizes are distinct.
  for (let component of computedComponents) {
    const componentId = component.component;
    // get the vertices of the computed component
    const queryVerticesOfComponent = `
        FOR v IN ${vColl}
        FILTER v.result == ${componentId}
          RETURN v._key
      `;
    const verticesOfComponent = db._query(queryVerticesOfComponent).toArray();
    const sizeOfComponent = component.size;
    // Test that the name prefixes of vertex names (i.e., `${s}` in `${s}_{i}`) are correct.
    for (let v of verticesOfComponent) {
      assertEqual(extract_name_prefix(v), `${sizeOfComponent}`);
    }

    // Test the second parts of vertex names.
    // For this, convert vertex keys of the form `${s}_${i}` to i.
    // We need this to sort them properly, e.g., "23_2" should be < than "23_10".
    let secondComponentsOfVertices = (verticesOfComponent.map(v => parseInt(v.substr(`${sizeOfComponent}`.length + 1))));
    secondComponentsOfVertices.sort(function(a, b) { return a - b;});
    for (let i = 0; i < sizeOfComponent; ++i) {
      assertEqual(secondComponentsOfVertices[i], i);
    }
  }
};

const graphName = "UnitTest_pregel";
const vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

function componentsTestSuite() {
  'use strict';

  const numComponents = 20; // components
  const n = 200; // vertices
  const m = 300; // edges
  const problematicGraphName = 'problematic';

  // a simple LCG to create deterministic pseudorandom numbers, 
  // from https://gist.github.com/Protonk/5389384
  let createRand = function(seed) {
    const m = 25;
    const a = 11;
    const c = 17;

    let z = seed || 3;
    return function() {
      z = (a * z + c) % m;
      return z / m;
    };
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {

      console.log("Beginning to insert test data with " + (numComponents * n) + 
                  " vertices, " + (numComponents * (m + n)) + " edges");

      // var exists = graph_module._list().indexOf("random") !== -1;
      // if (exists || db.demo_v) {
      //   return;
      // }
      var graph = graph_module._create(graphName);
      db._create(vColl, { numberOfShards: 4 });
      graph._addVertexCollection(vColl);
      db._createEdgeCollection(eColl, {
        numberOfShards: 4,
        replicationFactor: 1,
        shardKeys: ["vertex"],
        distributeShardsLike: vColl
      });

      console.log("created graph");

      var rel = graph_module._relation(eColl, [vColl], [vColl]);
      graph._extendEdgeDefinitions(rel);

      console.log("extended edge definition");

      for (let c = 0; c < numComponents; c++) {
        let x = 0;
        let vertices = [];
        while (x < n) {
          vertices.push({ _key: String(c) + ":" + String(x++) });
        }
        db[vColl].insert(vertices);
      }

      assertEqual(db[vColl].count(), numComponents * n);

      console.log("Done inserting vertices, inserting edges");

      let lcg = createRand();

      for (let c = 0; c < numComponents; c++) {
        let edges = [];
        for (let x = 0; x < m; x++) {
          let fromID = String(c) + ":" + Math.floor(lcg() * n);
          let toID = String(c) + ":" + Math.floor(lcg() * n);
          let from = vColl + '/' + fromID;
          let to = vColl + '/' + toID;
          edges.push({ _from: from, _to: to, vertex: String(fromID) });
        }
        db[eColl].insert(edges);

        for (let x = 0; x < n; x++) {
          let fromID = String(c) + ":" + x;
          let toID = String(c) + ":" + (x + 1);
          let from = vColl + '/' + fromID;
          let to = vColl + '/' + toID;
          db[eColl].insert({ _from: from, _to: to, vertex: String(fromID) });
        }
      }
      
      console.log("Got %s edges", db[eColl].count());
      assertEqual(db[eColl].count(), numComponents * m + numComponents * n);

      {
        const v = 'problematic_components';
        const e = 'problematic_connections';

        const edges = [
          ["A1", "A2"],
          ["A2", "A3"],
          ["A3", "A4"],
          ["A4", "A1"],
          ["B1", "B3"],
          ["B2", "B4"],
          ["B3", "B6"],
          ["B4", "B3"],
          ["B4", "B5"],
          ["B6", "B7"],
          ["B7", "B8"],
          ["B7", "B9"],
          ["B7", "B10"],
          ["B7", "B19"],
          ["B11", "B10"],
          ["B12", "B11"],
          ["B13", "B12"],
          ["B13", "B20"],
          ["B14", "B13"],
          ["B15", "B14"],
          ["B15", "B16"],
          ["B17", "B15"],
          ["B17", "B18"],
          ["B19", "B17"],
          ["B20", "B21"],
          ["B20", "B22"],
          ["C1", "C2"],
          ["C2", "C3"],
          ["C3", "C4"],
          ["C4", "C5"],
          ["C4", "C7"],
          ["C5", "C6"],
          ["C5", "C7"],
          ["C7", "C8"],
          ["C8", "C9"],
          ["C8", "C10"],
        ];

        const vertices = new Set(edges.flat());

        try {
          graph_module._drop(problematicGraphName, true);
        } catch (err) { }

        const graph = graph_module._create(problematicGraphName, [graph_module._relation(e, v, v)]);

        vertices.forEach(vertex => {
          graph[v].save({ _key: vertex });
        });

        edges.forEach(([from, to]) => {
          graph[e].save({ _from: `${v}/${from}`, _to: `${v}/${to}` });
        });
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      graph_module._drop(graphName, true);
      graph_module._drop(problematicGraphName, true);
    },

    testWCC: function () {
      var pid = pregel.start("wcc", graphName, { resultField: "result", store: true });
      var i = 10000;
      do {
        internal.sleep(0.2);
        let stats = pregel.status(pid);
        if (stats.state !== "running" && stats.state !== "storing") {
          assertEqual(stats.vertexCount, numComponents * n, stats);
          assertEqual(stats.edgeCount, numComponents * (m + n), stats);

          let c = db[vColl].all();
          let mySet = new Set();
          while (c.hasNext()) {
            let doc = c.next();
            assertTrue(doc.result !== undefined, doc);
            mySet.add(doc.result);
          }
          assertEqual(mySet.size, numComponents);

          break;
        }
      } while (i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in WCC execution");
      }
    },

    testWCC2: function() {
      const v = 'problematic_components';

      if (internal.isCluster()) {
        return;
      }

      // weakly connected components algorithm
      var handle = pregel.start('wcc', problematicGraphName, {
        maxGSS: 250, resultField: 'component'
      });

      while (true) {
        var status = pregel.status(handle);
        if (status.state !== 'running' && status.state !== 'storing') {
          console.log(status);
          break;
        } else {
          console.log('Waiting for Pregel result...');
          internal.sleep(1);
        }
      }

      const counts = db._query(
        `FOR vert IN @@v
          COLLECT component = vert.component
          WITH COUNT INTO count
          SORT count DESC
          RETURN count`,
        { "@v": v }
      ).toArray();

      assertEqual(counts.length, 3);
      assertEqual(counts[0], 22);
      assertEqual(counts[1], 10);
      assertEqual(counts[2], 4);
    }
  };
}

function wccRegressionTestSuite() {
  'use strict';

  const makeEdge = (from, to) => {
    return {_from: `${vColl}/${from}`, _to: `${vColl}/${to}`, vertex: `${from}`};
  };
  return {

    setUp: function() {
      db._create(vColl, { numberOfShards: 4 });
      db._createEdgeCollection(eColl, {
        numberOfShards: 4,
        replicationFactor: 1,
        shardKeys: ["vertex"],
        distributeShardsLike: vColl
      });

      graph_module._create(graphName, [graph_module._relation(eColl, vColl, vColl)], []);
    },

    tearDown: function() {
      graph_module._drop(graphName, true);
    },

    testWCCLineComponent: function() {
      const vertices = [];
      const edges = [];

      // We create one forward path 100 -> 120 (100 will be component ID, and needs to be propagated outbound)
      for (let i = 100; i < 120; ++i) {
        vertices.push({_key: `${i}`});
        if (i > 100) {
          edges.push(makeEdge(i-1, i));
        }
      }

      // We create one backward path 200 <- 220 (200 will be component ID, and needs to be propagated inbound)
      for (let i = 200; i < 220; ++i) {
        vertices.push({_key: `${i}`});
        if (i > 200) {
          edges.push(makeEdge(i, i - 1));
        }
      }
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const pid = pregel.start("wcc", graphName, { resultField: "result", store: true });
      const maxWaitTimeSecs = 120;
      const sleepIntervalSecs = 0.2;
      let wakeupsLeft = maxWaitTimeSecs / sleepIntervalSecs;
      while (pregel.status(pid).state !== "done" && wakeupsLeft > 0) {
        wakeupsLeft--;
        internal.sleep(0.2);
      }
      const status = pregel.status(pid);
      assertEqual(status.state, "done", "Pregel Job did never succeed.");

      // Now test the result.
      // We expect two components
      const query = `
        FOR v IN ${vColl}
          COLLECT component = v.result WITH COUNT INTO size
          RETURN {component, size}
      `;
      const computedComponents = db._query(query).toArray();
      assertEqual(computedComponents.length, 2, `We expected 2 components instead got ${JSON.stringify(computedComponents)}`);
      // Both have 20 elements
      assertEqual(computedComponents[0].size, 20);
      assertEqual(computedComponents[1].size, 20);
    },

    testWCCLostBackwardsConnection: function() {
      const vertices = [];
      const edges = [];

      /*
       * This test's background is a bit tricky and tests a deep technical
       * detail.
       * The idea in this test is the following:
       * We have two Subgroups A and B where there are only OUTBOUND
       * Edges from A to B.
       * In total and ignoring those edges A has a higher ComponentID then B.
       * Now comes the Tricky Part: 
       * 1) The Smallest ID in B, needs to have a a long Distance to all contact points to A.
       * 2) The ContactPoints A -> B need to have the second Smallest IDs
       * This now creates the following Effect:
       * The contact points only communitcate in the beginning, as they will not see smaller IDs
       * until the SmallestID arrives.
       * In the implementation showing this Bug, the INBOUND connection was not retained,
       * so as soon as the Smallest ID arrived at B it was not communicated back into the A
       * Cluster, which resulted in two components instead of a single one.
       */

      // We create 20 vertices
      for (let i = 1; i <= 20; ++i) {
        vertices.push({_key: `${i}`});
      }
      // By convention the lowest keys will have the Lowest ids.
      // We need to pick two second lowest Vertices as the ContactPoints of our two clusters:
      edges.push(makeEdge(2, 3));
      // Generate the A side
      edges.push(makeEdge(4,2));
      edges.push(makeEdge(4,5));
      edges.push(makeEdge(5,2));
      // Generate the B side with a Long path to 3
      // A path 6->7->...->20 (the 6 is already low, so not much updates happing)
      for (let i = 7; i <= 20; ++i) {
        edges.push(makeEdge(i - 1, i));
      }
      // Now connect 3 -> 6 and 20 -> 1
      edges.push(makeEdge(3, 6));
      edges.push(makeEdge(20, 1));
      
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const pid = pregel.start("wcc", graphName, { resultField: "result", store: true });
      const maxWaitTimeSecs = 120;
      const sleepIntervalSecs = 0.2;
      let wakeupsLeft = maxWaitTimeSecs / sleepIntervalSecs;
      while (pregel.status(pid).state !== "done" && wakeupsLeft > 0) {
        wakeupsLeft--;
        internal.sleep(0.2);
      }
      const status = pregel.status(pid);
      assertEqual(status.state, "done", "Pregel Job did never succeed.");

      // Now test the result.
      // We expect two components
      const query = `
        FOR v IN ${vColl}
          COLLECT component = v.result WITH COUNT INTO size
          RETURN {component, size}
      `;
      const computedComponents = db._query(query).toArray();
      assertEqual(computedComponents.length, 1, `We expected 1 component instead got ${JSON.stringify(computedComponents)}`);
      // we have all 20 elements
      assertEqual(computedComponents[0].size, 20);
    },

  };
}

function wccTestSuite() {
  'use strict';

  return {

    setUp: function() {
      db._create(vColl, { numberOfShards: 4 });
      db._createEdgeCollection(eColl, {
        numberOfShards: 4,
        replicationFactor: 1,
        shardKeys: ["vertex"],
        distributeShardsLike: vColl
      });

      graph_module._create(graphName, [graph_module._relation(eColl, vColl, vColl)], []);
    },

    tearDown: function() {
      graph_module._drop(graphName, true);
    },

    testWCCFourDirectedCycles: function() {
      testComponentsAlgorithmOnDisjointComponents([2, 10, 5, 23], graphGeneration.createDirectedCycle);
    },

    testWCC20DirectedCycles: function() {
      let componentSizes = [];
      // cycles of length smaller than 2 cannot be produced
      for (let i=2; i<22; ++i) {
        componentSizes.push(i);
      }
      testComponentsAlgorithmOnDisjointComponents(componentSizes, graphGeneration.createDirectedCycle);
    },

    testWCC20AlternatingCycles() {
      let componentSizes = [];
      // cycles of length smaller than 2 cannot be produced
      for (let i=2; i<22; ++i) {
        componentSizes.push(i);
      }
      testComponentsAlgorithmOnDisjointComponents(componentSizes, graphGeneration.createAlternatingCycle);
    },

    testWCC10BidirectedCliques() {
      let treeDepths = [];
      for (let i=120; i<130; ++i) {
        treeDepths.push(i);
      }
      testComponentsAlgorithmOnDisjointComponents(treeDepths, graphGeneration.createBidirectedClique);
    },

    testWCCOneSingleVertex: function() {
      let {vertices, edges} = graphGeneration.createSingleVertex("v");
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("wcc", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 1, `We expected 1 components, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, 1, `We expected 1 element, instead got ${JSON.stringify(computedComponents[0])}`);
    },

    testWCCTwoIsolatedVertices: function() {
      let isolatedVertex0 = graphGeneration.makeVertex(0, "v");
      let isolatedVertex1 = graphGeneration.makeVertex(1, "v");
      const vertices = [isolatedVertex0, isolatedVertex1];
      db[vColl].save(vertices);
      const edges = [];
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("wcc", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 2, `We expected 2 components, instead got ${JSON.stringify(computedComponents)}`);
      // Both have 20 elements
      assertEqual(computedComponents[0].size, 1);
      assertEqual(computedComponents[1].size, 1);
    },

    testWCCOneDirectedTree: function()  {
      const depth = 3;
      let {vertices, edges} = graphGeneration.createFullBinaryTree(depth, vColl, "v", false);
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("wcc", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 1, `We expected 1 component, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, Math.pow(2, depth + 1) - 1); // number of vertices in a full binary tree

    },

    testWCCOneDepth3AlternatingTree: function() {
      const depth = 3;
      let {vertices, edges} = graphGeneration.createFullBinaryTree(depth, vColl, "v", true);
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("wcc", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 1, `We expected 1 component, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, Math.pow(2, depth + 1) - 1); // number of vertices in a full binary tree
    },

    testWCCOneDepth4AlternatingTree: function() {
      const depth = 4;
      let {vertices, edges} = graphGeneration.createFullBinaryTree(depth, vColl, "v", true);
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("wcc", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 1, `We expected 1 component, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, Math.pow(2, depth + 1) - 1); // number of vertices in a full binary tree
    },

    testWCCAlternatingTreeAlternatingCycle: function() {
      // tree
      const depth = 3;
      const {vertices, edges} = graphGeneration.createFullBinaryTree(depth, vColl, "t", true);
      db[vColl].save(vertices);
      db[eColl].save(edges);

      // cycle
      const length = 5;
      const resultC = graphGeneration.createAlternatingCycle(length, vColl, "c");
      db[vColl].save(resultC.vertices);
      db[eColl].save(resultC.edges);

      const computedComponents = pregelRunSmallInstance("wcc", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 2, `We expected 2 component, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, Math.pow(2, depth + 1) - 1); // number of vertices in a full binary tree
      assertEqual(computedComponents[1].size, length);
    },

  };
}

function sccTestSuite() {
  'use strict';


  return {

    setUp: function() {
      db._create(vColl, { numberOfShards: 4 });
      db._createEdgeCollection(eColl, {
        numberOfShards: 4,
        replicationFactor: 1,
        shardKeys: ["vertex"],
        distributeShardsLike: vColl
      });

      graph_module._create(graphName, [graph_module._relation(eColl, vColl, vColl)], []);
    },

    tearDown: function() {
      graph_module._drop(graphName, true);
    },

    testSCCFourDirectedCycles: function() {
      testComponentsAlgorithmOnDisjointComponents([2, 10, 5, 23], graphGeneration.createDirectedCycle, "scc");
    },

    testSCC20DirectedCycles: function() {
      let componentSizes = [];
      // cycles of length smaller than 2 cannot be produced
      for (let i=2; i<22; ++i) {
        componentSizes.push(i);
      }
      testComponentsAlgorithmOnDisjointComponents(componentSizes, graphGeneration.createDirectedCycle, "scc");
    },

    testSCC10BidirectedCliques() {
      let treeDepths = [];
      for (let i=120; i<130; ++i) {
        treeDepths.push(i);
      }
      testComponentsAlgorithmOnDisjointComponents(treeDepths, graphGeneration.createBidirectedClique, "scc");
    },

    testSCCOneSingleVertex: function() {
      let {vertices, edges} = graphGeneration.createSingleVertex("v");
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 1, `We expected 1 components, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, 1, `We expected 1 element, instead got ${JSON.stringify(computedComponents[0])}`);
    },

    testSCCTwoIsolatedVertices: function() {
      let isolatedVertex0 = graphGeneration.makeVertex(0, "v");
      let isolatedVertex1 = graphGeneration.makeVertex(1, "v");
      const vertices = [isolatedVertex0, isolatedVertex1];
      db[vColl].save(vertices);
      const edges = [];
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 2, `We expected 2 components, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, 1);
      assertEqual(computedComponents[1].size, 1);
    },

    testSCCOneEdge: function() {
      let vertex0 = graphGeneration.makeVertex(0, "v");
      let vertex1 = graphGeneration.makeVertex(1, "v");
      const vertices = [vertex0, vertex1];
      db[vColl].save(vertices);
      const edges = [makeEdge(0, 1, vColl, "v")];
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 2, `We expected 2 components, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, 1);
      assertEqual(computedComponents[1].size, 1);
    },

    testSCCOneDirected10Path: function() {
      const length = 10;
      const {vertices, edges} = graphGeneration.createPath(length, vColl, "v");
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, length,
          `We expected ${length} components, instead got ${JSON.stringify(computedComponents)}`);
      for (const component of computedComponents) {
        assertEqual(component.size, 1);
      }
    },

    testSCCOneBidirected10Path: function() {
      const length = 10;
      const {vertices, edges} = graphGeneration.createPath(length, vColl, "v", "bidirected");
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, 1,
          `We expected ${length} components, instead got ${JSON.stringify(computedComponents)}`);
      assertEqual(computedComponents[0].size, length);
    },

    testSCCOneAlternated10Path: function() {
      const length = 10;
      const {vertices, edges} = graphGeneration.createPath(length, vColl, "v", "alternating");
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, length,
          `We expected ${length} components, instead got ${JSON.stringify(computedComponents)}`);
      for (const component of computedComponents) {
        assertEqual(component.size, 1);
      }
    },

    testSCCOneDirectedTree: function()  {
      // Each vertex induces its own strongly connected component.
      const depth = 3;
      let {vertices, edges} = graphGeneration.createFullBinaryTree(depth, vColl, "v", false);
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      // number of vertices in a full binary tree
      const numVertices = Math.pow(2, depth + 1) - 1;
      assertEqual(computedComponents.length, numVertices,
          `We expected ${numVertices} components, instead got ${JSON.stringify(computedComponents)}`);

      for (const component of computedComponents) {
        assertEqual(component.size, 1);
      }

    },

    testSCCOneDepth3AlternatingTree: function() {
      // Each vertex induces its own strongly connected component.
      const depth = 3;
      let {vertices, edges} = graphGeneration.createFullBinaryTree(depth, vColl, "v", true);
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      // number of vertices in a full binary tree
      const numVertices = Math.pow(2, depth + 1) - 1;
      assertEqual(computedComponents.length, numVertices,
          `We expected ${numVertices} component, instead got ${JSON.stringify(computedComponents)}`);

      for (const component of computedComponents) {
        assertEqual(component.size, 1);
      }
    },

    testSCCOneDepth4AlternatingTree: function() {
      const depth = 4;
      // Each vertex induces its own strongly connected component.
      let {vertices, edges} = graphGeneration.createFullBinaryTree(depth, vColl, "v", true);
      db[vColl].save(vertices);
      db[eColl].save(edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      // number of vertices in a full binary tree
      const numVertices = Math.pow(2, depth + 1) - 1;
      assertEqual(computedComponents.length, numVertices,
          `We expected ${numVertices} component, instead got ${JSON.stringify(computedComponents)}`);

      for (const component of computedComponents) {
        assertEqual(component.size, 1);
      }
    },

    testSCCAlternatingTreeAlternatingCycle: function() {
      // tree
      const depth = 3;
      const {vertices, edges} = graphGeneration.createFullBinaryTree(depth, vColl, "t", true);
      db[vColl].save(vertices);
      db[eColl].save(edges);

      // cycle
      const length = 5;
      const resultC = graphGeneration.createAlternatingCycle(length, vColl, "c");
      db[vColl].save(resultC.vertices);
      db[eColl].save(resultC.edges);

      const computedComponents = pregelRunSmallInstance("SCC", graphName, { resultField: "result", store: true });
      assertEqual(computedComponents.length, depth + length,
          `We expected ${depth + length} components, instead got ${JSON.stringify(computedComponents)}`);

      for (let component of computedComponents) {
        assertEqual(component.size, 1);
      }
    },

  };
}

jsunity.run(componentsTestSuite);
jsunity.run(wccRegressionTestSuite);
jsunity.run(wccTestSuite);
jsunity.run(sccTestSuite);
return jsunity.done();
