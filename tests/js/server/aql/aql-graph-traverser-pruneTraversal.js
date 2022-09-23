/* jshint esnext: true */
/* global AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for the AQL FOR x IN GRAPH name statement
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, fail} = jsunity.jsUnity.assertions;

var _ = require('lodash');
const internal = require('internal');
const db = internal.db;
const errors = require('@arangodb').errors;
const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';
const isCluster = require('@arangodb/cluster').isCluster();
const removeCost = require('@arangodb/aql-helper').removeCost;

const gh = require('@arangodb/graph/helpers');

function pruneTraversalSuite() {
  const optionsToTest = {
    DFS: {bfs: false},
    BFS: {bfs: true},
    Neighbors: {bfs: true, uniqueVertices: 'global'}
  };

  // We have identical tests for all traversal options.
  const appendTests = (testObj, name, opts) => {
    
    testObj['testRenameConditionVariablesNested'] = () => {
      const q = `
        WITH ${vn}
        LET sub = NOOPT({ value: 'C' })
        LET sub2 = sub.value
        LET sub3 = sub.value
        LET sub4 = sub.value
          FOR v, e, p IN 1..3 ANY "${gh.vertex.B}" ${en}
            PRUNE v._key == 'C'
            OPTIONS ${JSON.stringify(opts)}
            FILTER p.edges[*]._key ALL != sub2 
            FILTER p.edges[*]._key ALL != sub3 
            FILTER p.edges[*]._key ALL != sub4 
            FILTER p.vertices[*]._key ALL != sub2 
            FILTER p.vertices[*]._key ALL != sub3
            FILTER p.vertices[*]._key ALL != sub4
            FILTER p.edges[0]._key != sub2 
            FILTER p.edges[0]._key != sub3 
            FILTER p.edges[0]._key != sub4 
            FILTER p.vertices[0]._key != sub2 
            FILTER p.vertices[0]._key != sub3
            FILTER p.vertices[0]._key != sub4
            RETURN v._key
      `;
      const res = db._query(q);

      if (name === "Neighbors") {
        assertEqual(res.count(), 3, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'E', 'F'].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 5, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'C', 'E', 'F'].sort(), `In query ${q}`);
      }
    };
    
    testObj['testRenamePruneVariablesNested'] = () => {
      const q = `
        WITH ${vn}
        LET sub = (FOR s IN ['C'] RETURN s)
        FOR sub2 IN sub
          FOR v IN 1..3 ANY "${gh.vertex.B}" ${en}
            PRUNE v._key == sub2
            OPTIONS ${JSON.stringify(opts)}
            RETURN v._key
      `;
      const res = db._query(q);

      if (name === "Neighbors") {
        assertEqual(res.count(), 4, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'E', 'F'].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 5, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'C', 'E', 'F'].sort(), `In query ${q}`);
      }
    };

    testObj['testPruneVariableReferringToOuter'] = () => {
      const q = `
        WITH ${vn}
        LET sub = (FOR s IN ['C'] RETURN s)
        FOR v IN 1..3 ANY "${gh.vertex.B}" ${en}
          PRUNE v._key IN sub 
          OPTIONS ${JSON.stringify(opts)}
          RETURN v._key
      `;
      const res = db._query(q);

      if (name === "Neighbors") {
        assertEqual(res.count(), 4, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'E', 'F'].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 5, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'C', 'E', 'F'].sort(), `In query ${q}`);
      }
    };

    testObj[`testAllowPruningOnV${name}`] = () => {
      const q = `
        WITH ${vn}
        FOR v IN 1..3 ANY "${gh.vertex.B}" ${en}
          PRUNE v._key == "C"
          OPTIONS ${JSON.stringify(opts)}
          RETURN v._key
      `;
      const res = db._query(q);

      if (name === "Neighbors") {
        assertEqual(res.count(), 4, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'E', 'F'].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 5, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'C', 'E', 'F'].sort(), `In query ${q}`);
      }
    };

    testObj[`testAllowPruningOnE${name}`] = () => {
      const q = `
        WITH ${vn}
        FOR v, e IN 1..3 ANY "${gh.vertex.B}" ${en}
          PRUNE e._to == "${gh.vertex.C}"
          OPTIONS ${JSON.stringify(opts)}
          RETURN v._key
      `;
      const res = db._query(q);
      if (name === "Neighbors") {
        assertEqual(res.count(), 4, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'E', 'F'].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 5, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'C', 'E', 'F'].sort(), `In query ${q}`);
      }
    };

    testObj[`testAllowPruningOnP${name}`] = () => {
      const q = `
                WITH ${vn}
                FOR v, e, p IN 1..3 ANY "${gh.vertex.B}" ${en}
                  PRUNE p.vertices[1]._key == "C"
                  OPTIONS ${JSON.stringify(opts)}
                  RETURN v._key
              `;
      const res = db._query(q);
      if (name === "Neighbors") {
        assertEqual(res.count(), 4, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'E', 'F'].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 5, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['A', 'C', 'C', 'E', 'F'].sort(), `In query ${q}`);
      }
    };

    testObj[`testAllowPruningOnPReturnE${name}`] = () => {
      const q = `
                WITH ${vn}
                FOR v, e, p IN 1..3 ANY "${gh.vertex.B}" ${en}
                  PRUNE p.vertices[1]._key == "C"
                  OPTIONS ${JSON.stringify(opts)}
                  RETURN e._id
              `;
      const res = db._query(q);
      if (name === "Neighbors") {
        assertEqual(res.count(), 4, `In query ${q}`);
        assertEqual(res.toArray().sort(), [gh.edge.AB, gh.edge.BC, gh.edge.EB, gh.edge.FE].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 5, `In query ${q}`);
        assertEqual(res.toArray().sort(), [gh.edge.AB, gh.edge.BC, gh.edge.EB, gh.edge.FE, gh.edge.CF].sort(), `In query ${q}`);
      }
    };

    testObj[`testAllowPruningOnOuterVar${name}`] = () => {
      const q = `
                WITH ${vn} 
                FOR key IN ["C", "F"]
                FOR v IN 1..3 ANY "${gh.vertex.B}" ${en}
                  PRUNE v._key == key
                  OPTIONS ${JSON.stringify(opts)}
                  RETURN v._key
              `;
      const res = db._query(q);

      if (name === "Neighbors") {
        const resKeyC = ['A', 'C', 'E', 'F'];
        const resKeyF = ['A', 'C', 'D', 'F', 'E'];
        assertEqual(res.count(), resKeyC.length + resKeyF.length, `In query ${q}`);
        assertEqual(res.toArray().sort(), resKeyC.concat(resKeyF).sort(), `In query ${q}`);
      } else {
        const resKeyC = ['A', 'C', 'C', 'E', 'F'];
        const resKeyF = ['A', 'C', 'D', 'F', 'E', 'F'];
        assertEqual(res.count(), resKeyC.length + resKeyF.length, `In query ${q}`);
        assertEqual(res.toArray().sort(), resKeyC.concat(resKeyF).sort(), `In query ${q}`);
      }
    };

    testObj[`testAllowPruningOnVBelowMinDepth${name}`] = () => {
      const q = `
        WITH ${vn}
        FOR v IN 3 ANY "${gh.vertex.B}" ${en}
          PRUNE v._key == "C"
          OPTIONS ${JSON.stringify(opts)}
          RETURN v._key
      `;
      const res = db._query(q);

      if (name === "Neighbors") {
        assertEqual(res.count(), 0, `In query ${q}`);
        assertEqual(res.toArray().sort(), [].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 1, `In query ${q}`);
        assertEqual(res.toArray().sort(), ['C'].sort(), `In query ${q}`);
      }
    };

    testObj[`testMultipleCoordinatorParts${name}`] = () => {
      // This is intendent to test Cluster to/from VPack function of traverser Nodes
      // On SingleServer this tests pruning on the startVertex
      const q = `
        WITH ${vn}
        FOR v IN 1 ANY "${gh.vertex.B}" ${en}
          PRUNE v._key == "C" /* this actually does not prune */
          OPTIONS ${JSON.stringify(opts)}
          FOR source IN ${vn}
            FILTER source._key == v._key
            FOR k IN 2 ANY source ${en}
            PRUNE k._key == "C"
            OPTIONS ${JSON.stringify(opts)}
            RETURN k._key
      `;

      // The first traversal will find A, C, E
      // The Primary Index Scan in the middle is actually
      // a noop and only enforces a walk thorugh DBServer
      // The Second traversal will find:
      // A => C,E
      // C => [] // it shall prune the startvertex
      // E => A,C,C 
      const res = db._query(q);

      if (name === "Neighbors") {
        // The E part does not find C twice
        assertEqual(res.count(), 4, `In query ${q}`);
        assertEqual(res.toArray().sort(), ["C", "E", "A", "C"].sort(), `In query ${q}`);
      } else {
        assertEqual(res.count(), 5, `In query ${q}`);
        assertEqual(res.toArray().sort(), ["C", "E", "A", "C", "C"].sort(), `In query ${q}`);
      }
    };

  };

  const testObj = {
    setUpAll: () => {
      gh.cleanup();
      gh.createBaseGraph();
    },

    tearDownAll: () => {
      gh.cleanup();
    }
  };

  for (let [name, opts] of Object.entries(optionsToTest)) {
    appendTests(testObj, name, opts);
  }

  return testObj;
}

jsunity.run(pruneTraversalSuite);
return jsunity.done();
