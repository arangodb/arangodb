/*jshint globalstrict:true, strict:true, esnext: true */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotEqual, assertException, assertNotNull}
  = jsunity.jsUnity.assertions;

const internal = require("internal");
const db = internal.db;
const aql = require("@arangodb").aql;
const protoGraphs = require('@arangodb/aql-graph-traversal-generic-graphs').protoGraphs;
const _ = require("lodash");


// Rose tree
class Node {
  constructor(vertex, children = []) {
    this.vertex = vertex;
    this.children = children;
  }

  // For debugging
  toObj() {
    return {[this.vertex]: this.children.map(n => n.toObj())};
  }
}


/**
 * @brief This function asserts that a list of paths is in a given DFS order
 *
 * @param paths  An array of paths to be checked.
 * @param tree   A rose tree (given as a Node) specifying the valid DFS
 *               orders. The root node must be the root of the DFS
 *               traversal. Each node's children correspond to the possible
 *               next vertices the DFS may visit.
 * @param stack  During recursion, holds the path from the root to the
 *               current vertex as an array of vertices.
 *
 * Note that the type of vertices here is irrelevant, as long as its the
 * same in the given paths and tree. E.g. paths[i][j] and, for each Node n
 * in tree, n.vertex must be of the same type (or at least comparable).
 */
const checkResIsValidDfsOf = (paths, tree, stack = []) => {
  // const debugPrint = (...args) => internal.print('> '.repeat(stack.length + 1), ...args);
  // debugPrint(`checkResIsValidDfsOf(${JSON.stringify({paths, tree: tree.toObj(), stack})})`);

  assertTrue(!_.isEmpty(paths));
  const vertex = tree.vertex;
  const curPath = _.head(paths);
  let remainingPaths = _.tail(paths);
  // push without modifying `stack`
  const curStack = stack.concat([vertex]);

  // Assert that our current position in the tree matches the current path.
  // Except for the very first call where stack === [], this should not
  // trigger, as the recursive call is only done with a correct next path.
  assertEqual(curStack, curPath);

  let remainingChildren = tree.children.slice();
  while (remainingChildren.length > 0) {
    assertFalse(remainingPaths.length === 0,
      'Not enough paths given. '
      + 'The current path/stack is ' + JSON.stringify(curStack) + '. '
      + 'Expected next would be one of: '
      + JSON.stringify(remainingChildren.map(node => curStack.concat([node.vertex])))
    );
    // Peek at the next path to check the right child next
    const nextPath = _.head(remainingPaths);
    assertNotNull(nextPath, 'No more remaining paths, but expecting one of '
      + JSON.stringify(remainingChildren.map(c => c.toObj())));
    const nextChildIndex = _.findIndex(remainingChildren, (node) =>
      _.isEqual(nextPath, curStack.concat([node.vertex]))
    );
    assertNotEqual(-1, nextChildIndex, JSON.stringify({
      nextPath: nextPath,
      curStack: curStack,
      remainingChildren: remainingChildren.map(node => node.vertex),
    }));
    const [nextChild] = remainingChildren.splice(nextChildIndex, 1);

    remainingPaths = checkResIsValidDfsOf(remainingPaths, nextChild, curStack);
  }

  // When the stack is empty, we must have checked all paths by now.
  assertTrue(!_.isEmpty(stack) || _.isEmpty(remainingPaths),
    `There are unchecked paths remaining: ${JSON.stringify(remainingPaths)}`);

  return remainingPaths;
};


/**
 * @brief This function asserts that a list of paths is in BFS order and matches
 *        an expected path list.
 *
 * @param expectedPaths An array of expected paths.
 * @param actualPaths   An array of paths to be checked.
 *
 * Checks that the paths in both arrays are the same, and that the paths in
 * expectedPaths are increasing in length. Apart from that, the order of paths
 * in both arguments is ignored.
 */
const checkResIsValidBfsOf = (expectedPaths, actualPaths) => {
  assertTrue(!_.isEmpty(actualPaths));

  const pathLengths = actualPaths.map(p => p.length);
  const adjacentPairs = _.zip(pathLengths, _.tail(pathLengths));
  adjacentPairs.pop(); // we don't want the last element, because the tail is shorter
  assertTrue(adjacentPairs.every(([a, b]) => a <= b),
    `Paths are not increasing in length: ${JSON.stringify(actualPaths)}`);

  // sort paths by length first
  actualPaths = _.sortBy(actualPaths, p => [p.length, p]);
  expectedPaths = _.sortBy(expectedPaths, p => [p.length, p]);
  const missingPaths = _.difference(expectedPaths, actualPaths);
  const spuriousPaths = _.difference(actualPaths, expectedPaths);

  const messages = [];
  if (missingPaths.length > 0) {
    messages.push('The following paths are missing: ' + JSON.stringify(missingPaths));
  }
  if (spuriousPaths.length > 0) {
    messages.push('The following paths are wrong: ' + JSON.stringify(spuriousPaths));
  }

  assertEqual(expectedPaths, actualPaths, messages.join('; '));
};

/**
 * @brief Tests the function checkResIsValidDfsOf(), which is used in the tests and
 *        non-trivial. This function tests cases that should be valid.
 */
function testMetaDfsValid() {
  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
      new Node("C", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
    ]);
  let paths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  checkResIsValidDfsOf(paths, expectedPathsAsTree);
  paths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "F"],
    ["A", "B", "D", "E"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  checkResIsValidDfsOf(paths, expectedPathsAsTree);
  paths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "F"],
    ["A", "C", "D", "E"],
  ];
  checkResIsValidDfsOf(paths, expectedPathsAsTree);
  paths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "F"],
    ["A", "B", "D", "E"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "F"],
    ["A", "C", "D", "E"],
  ];
  checkResIsValidDfsOf(paths, expectedPathsAsTree);
  paths = [
    ["A"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
  ];
  checkResIsValidDfsOf(paths, expectedPathsAsTree);
  paths = [
    ["A"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
  ];
  checkResIsValidDfsOf(paths, expectedPathsAsTree);
  paths = [
    ["A"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "F"],
    ["A", "B", "D", "E"],
  ];
  checkResIsValidDfsOf(paths, expectedPathsAsTree);
  paths = [
    ["A"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "F"],
    ["A", "B", "D", "E"],
  ];
  checkResIsValidDfsOf(paths, expectedPathsAsTree);
}


/**
 * @brief Tests the function checkResIsValidDfsOf(), which is used in the tests and
 *        non-trivial. This function tests cases that should be invalid.
 */
function testMetaDfsInvalid() {
  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
      new Node("C", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
    ]);
  const validPaths = [
    ["A"],                // [0]
    ["A", "B"],           // [1]
    ["A", "B", "D"],      // [2]
    ["A", "B", "D", "E"], // [3]
    ["A", "B", "D", "F"], // [4]
    ["A", "C"],           // [5]
    ["A", "C", "D"],      // [6]
    ["A", "C", "D", "E"], // [7]
    ["A", "C", "D", "F"], // [8]
  ];
  let paths;
  // Missing paths:

  // first path missing
  paths = validPaths.slice();
  paths.splice(0, 1);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  // length 3 path missing
  paths = validPaths.slice();
  paths.splice(2, 1);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  // subtree missing
  paths = validPaths.slice();
  paths.splice(2, 3);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  // children missing
  paths = validPaths.slice();
  paths.splice(7, 2);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  // last path missing
  paths = validPaths.slice();
  paths.splice(paths.length - 1, 1);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));

  // Superfluous paths:

  // first path duplicate
  paths = validPaths.slice();
  paths.splice(0, 0, paths[0]);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  // path without children duplicate
  paths = validPaths.slice();
  paths.splice(3, 0, paths[3]);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  // invalid paths added
  paths = validPaths.concat(["B"]);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  paths = validPaths.concat(["A", "B", "E"]);
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));

  // Invalid orders:

  paths = validPaths.slice().reverse();
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  paths = validPaths.slice();
  [paths[0], paths[1]] = [paths[1], paths[0]];
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  paths = validPaths.slice();
  [paths[1], paths[5]] = [paths[5], paths[1]];
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  paths = validPaths.slice();
  [paths[4], paths[8]] = [paths[8], paths[4]];
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));

  // Paths with trailing elements:

  paths = validPaths.slice();
  paths[0] = paths[0].concat("A");
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  paths = validPaths.slice();
  paths[0] = paths[0].concat("B");
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  paths = validPaths.slice();
  paths[3] = paths[3].concat("F");
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
  paths = validPaths.slice();
  paths[8] = paths[8].concat("F");
  assertException(() => checkResIsValidDfsOf(paths, expectedPathsAsTree));
}

function testMetaBfsValid() {
  let expectedPaths;
  let actualPaths;

  expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  checkResIsValidBfsOf(expectedPaths, actualPaths);
  actualPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  checkResIsValidBfsOf(expectedPaths, actualPaths);
  actualPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "C", "D"],
    ["A", "B", "D"],
    ["A", "C", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "F"],
    ["A", "B", "D", "E"],
  ];
  checkResIsValidBfsOf(expectedPaths, actualPaths);

  expectedPaths = [
    ["A", "B", "D"],
    ["A", "B"],
    ["A", "B", "D", "E"],
    ["A", "C"],
    ["A", "B", "D", "F"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testMetaBfsInvalid() {
  let expectedPaths;
  let actualPaths;

  expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  assertException(() => checkResIsValidBfsOf(expectedPaths, actualPaths));
  expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  assertException(() => checkResIsValidBfsOf(expectedPaths, actualPaths));
  expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  assertException(() => checkResIsValidBfsOf(expectedPaths, actualPaths));
  expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
  ];
  assertException(() => checkResIsValidBfsOf(expectedPaths, actualPaths));
  expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  assertException(() => checkResIsValidBfsOf(expectedPaths, actualPaths));
  expectedPaths = [
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  assertException(() => checkResIsValidBfsOf(expectedPaths, actualPaths));
  expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  assertException(() => checkResIsValidBfsOf(expectedPaths, actualPaths));
  expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  assertException(() => checkResIsValidBfsOf(expectedPaths, actualPaths));
}

function testOpenDiamondDfsUniqueVerticesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
      new Node("C", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testOpenDiamondDfsUniqueVerticesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
      new Node("C", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testOpenDiamondDfsUniqueEdgesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
      new Node("C", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testOpenDiamondDfsUniqueEdgesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
      new Node("C", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testOpenDiamondDfsUniqueEdgesUniqueVerticesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "path", uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
      new Node("C", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testOpenDiamondDfsUniqueEdgesUniqueVerticesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "none", uniqueVertices: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
      new Node("C", [
        new Node("D", [
          new Node("E"),
          new Node("F"),
        ]),
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testOpenDiamondBfsUniqueVerticesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueVerticesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueVerticesGlobal(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "global", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueEdgesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueEdgesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueEdgesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueEdgesUniqueVerticesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", uniqueVertices: "path", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueEdgesUniqueVerticesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", uniqueVertices: "none", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueEdgesUniquePathVerticesGlobal(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", uniqueVertices: "global", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testOpenDiamondBfsUniqueEdgesUniqueNoneVerticesGlobal(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", uniqueVertices: "global", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testSmallCircleDfsUniqueVerticesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D")
        ])
      ])
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testSmallCircleDfsUniqueVerticesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  // no loop expected, since uniqueEdges is path by default
  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node ("A")
          ])
        ])
      ])
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testSmallCircleDfsUniqueEdgesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node ("A")
          ])
        ])
      ])
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testSmallCircleDfsUniqueEdgesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  // loop expected, since uniqueVertices is "none" by default
  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node ("A", [
              new Node ("B", [
                new Node ("C", [
                  new Node ("D", [
                    new Node("A", [
                      new Node("B")
                    ])
                  ])
                ])
              ])
            ])
          ])
        ])
      ])
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testSmallCircleDfsUniqueVerticesUniqueEdgesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()}
        OPTIONS {uniqueVertices: "path", uniqueEdges: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D")
        ])
      ])
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testSmallCircleDfsUniqueVerticesUniqueEdgesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()}
        OPTIONS {uniqueVertices: "none", uniqueEdges: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  // loop expected, since uniqueVertices is "none" by default
  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node ("A", [
              new Node ("B", [
                new Node ("C", [
                  new Node ("D", [
                    new Node("A", [
                      new Node("B", [
                        new Node("C")
                      ])
                    ])
                  ])
                ])
              ])
            ])
          ])
        ])
      ])
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(actualPaths, expectedPathsAsTree);
}

function testSmallCircleBfsUniqueVerticesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "C"],
    ["A", "B", "C", "D"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testSmallCircleBfsUniqueVerticesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "C"],
    ["A", "B", "C", "D"],
    ["A", "B", "C", "D", "A"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testSmallCircleBfsUniqueEdgesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "C"],
    ["A", "B", "C", "D"],
    ["A", "B", "C", "D", "A"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testSmallCircleBfsUniqueEdgesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "C"],
    ["A", "B", "C", "D"],
    ["A", "B", "C", "D", "A"],
    ["A", "B", "C", "D", "A", "B"],
    ["A", "B", "C", "D", "A", "B", "C"],
    ["A", "B", "C", "D", "A", "B", "C", "D"],
    ["A", "B", "C", "D", "A", "B", "C", "D", "A"],
    ["A", "B", "C", "D", "A", "B", "C", "D", "A", "B"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();
  print(actualPaths);

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testSmallCircleBfsUniqueVerticesUniqueEdgesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", uniqueEdges: "path", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "C"],
    ["A", "B", "C", "D"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

function testSmallCircleBfsUniqueVerticesUniqueEdgesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", uniqueEdges: "none", bfs: true}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "C"],
    ["A", "B", "C", "D"],
    ["A", "B", "C", "D", "A"],
    ["A", "B", "C", "D", "A", "B"],
    ["A", "B", "C", "D", "A", "B", "C"],
    ["A", "B", "C", "D", "A", "B", "C", "D"],
    ["A", "B", "C", "D", "A", "B", "C", "D", "A"],
    ["A", "B", "C", "D", "A", "B", "C", "D", "A", "B"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(actualPaths, expectedPaths);
}

/*
  Tests to write:
    - different graphs
    - with different smart attribute / sharding combinations
    - with community graph
    - different uniqueness levels
    - different traversal directions (INBOUND, OUTBOUND, ANY)
    - dfs, bfs
 */

const testsByGraph = {
  openDiamond: {
    testOpenDiamondDfsUniqueVerticesPath,
    testOpenDiamondDfsUniqueVerticesNone,
    testOpenDiamondDfsUniqueEdgesPath,
    testOpenDiamondDfsUniqueEdgesNone,
    testOpenDiamondDfsUniqueEdgesUniqueVerticesPath,
    testOpenDiamondDfsUniqueEdgesUniqueVerticesNone,
    testOpenDiamondBfsUniqueVerticesPath,
    testOpenDiamondBfsUniqueVerticesGlobal,
    testOpenDiamondBfsUniqueEdgesPath,
    testOpenDiamondBfsUniqueEdgesNone,
    testOpenDiamondBfsUniqueEdgesUniqueVerticesPath,
    testOpenDiamondBfsUniqueEdgesUniqueVerticesNone,
    testOpenDiamondBfsUniqueEdgesUniquePathVerticesGlobal,
    testOpenDiamondBfsUniqueEdgesUniqueNoneVerticesGlobal
  },
  smallCircle: {
    testSmallCircleDfsUniqueVerticesPath,
    testSmallCircleDfsUniqueVerticesNone,
    testSmallCircleDfsUniqueEdgesPath,
    testSmallCircleDfsUniqueEdgesNone,
    testSmallCircleDfsUniqueVerticesUniqueEdgesPath,
    testSmallCircleDfsUniqueVerticesUniqueEdgesNone,
    testSmallCircleBfsUniqueVerticesPath,
    testSmallCircleBfsUniqueVerticesNone,
    testSmallCircleBfsUniqueEdgesPath,
    testSmallCircleBfsUniqueEdgesNone,
    testSmallCircleBfsUniqueVerticesUniqueEdgesPath,
    testSmallCircleBfsUniqueVerticesUniqueEdgesNone
  }
};

const metaTests = {
  testMetaDfsValid: testMetaDfsValid,
  testMetaDfsInvalid: testMetaDfsInvalid,
  testMetaBfsValid: testMetaBfsValid,
  testMetaBfsInvalid: testMetaBfsInvalid,
};

exports.testsByGraph = testsByGraph;
exports.metaTests = metaTests;
