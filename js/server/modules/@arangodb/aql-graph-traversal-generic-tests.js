/*jshint globalstrict:true, strict:true, esnext: true */
/*global print */

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


/*
  TODO:
    We need more tests to cover the following things:
    - different traversal directions (that is, INBOUND and especially ANY)
    - graph modifications (see e.g. aqlModifySmartGraphSuite)
    - dump/restore (see e.g. dumpTestEnterpriseSuite)
    - there aren't any tests yet for protoGraps.moreAdvancedPath
    - currently, there are no tests with loops (edges v->v)
 */


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
 * @param tree   A rose tree (given as a Node) specifying the valid DFS
 *               orders. The root node must be the root of the DFS
 *               traversal. Each node's children correspond to the possible
 *               next vertices the DFS may visit.
 * @param paths  An array of paths to be checked.
 * @param stack  During recursion, holds the path from the root to the
 *               current vertex as an array of vertices.
 *
 * Note that the type of vertices here is irrelevant, as long as its the
 * same in the given paths and tree. E.g. paths[i][j] and, for each Node n
 * in tree, n.vertex must be of the same type (or at least comparable).
 */
const checkResIsValidDfsOf = (tree, paths, stack = []) => {
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
    assertNotEqual(-1, nextChildIndex,
      `The following path is not valid at this point: ${JSON.stringify(nextPath)}
        But one of the following is expected next: ${JSON.stringify(remainingChildren.map(node => curStack.concat([node.vertex])))}
        The last asserted path is: ${JSON.stringify(curStack)}`
    );
    const [nextChild] = remainingChildren.splice(nextChildIndex, 1);

    remainingPaths = checkResIsValidDfsOf(nextChild, remainingPaths, curStack);
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
 *
 * Please note that this check does NOT work with results of {uniqueVertices: global}!
 */
const checkResIsValidStackBasedTraversalOfFunc = function (getVertices, getCost) {
  return (expectedPaths, actualPaths) => {
    assertTrue(!_.isEmpty(actualPaths));

    const actualPathVertices = actualPaths.map(getVertices);

    const pathLengths = actualPaths.map(p => [getCost(p), getVertices(p).length]);
    const adjacentPairs = _.zip(pathLengths, _.tail(pathLengths));
    adjacentPairs.pop(); // we don't want the last element, because the tail is shorter
    // This asserts in addition to a.cost <= b.cost that if a.cost == b.cost we have a.length <= b.length
    assertTrue(adjacentPairs.every(([[a_cost, a_length], [b_cost, b_length]]) =>
            (a_cost === b_cost ? a_length <= b_length : a_cost <= b_cost)),
        `Paths are not increasing in length: ${JSON.stringify(actualPaths)}`);

    const actualPathsSet = new Map();
    for (const path of actualPathVertices) {
      const p = JSON.stringify(path);
      if (actualPathsSet.has(p)) {
        actualPathsSet.set(p, actualPathsSet.get(p) + 1);
      } else {
        actualPathsSet.set(p, 1);
      }
    }

    const expectedPathsSet = new Map();
    for (const path of expectedPaths) {
      const p = JSON.stringify(path);
      if (expectedPathsSet.has(p)) {
        expectedPathsSet.set(p, expectedPathsSet.get(p) + 1);
      } else {
        expectedPathsSet.set(p, 1);
      }
    }

    const missingPaths = Array.from(expectedPathsSet.entries())
        .map(([p, n]) => [p, n - actualPathsSet.get(p)])
        .filter(([p, n]) => n > 0);

    const spuriousPaths = Array.from(actualPathsSet.entries())
        .map(([p, n]) => [p, n - expectedPathsSet.get(p)])
        .filter(([p, n]) => n > 0);

    const messages = [];
    if (missingPaths.length > 0) {
      messages.push('The following paths are missing: ' + missingPaths.map(([p, n]) => `${n}:${p}`).join());
    }
    if (spuriousPaths.length > 0) {
      messages.push('The following paths are wrong: ' + spuriousPaths.map(([p, n]) => `${n}:${p}`).join());
    }
    // sort paths by length first
    actualPaths = _.sortBy(actualPaths, [p => getCost(p), p => getVertices(p).length, p => getVertices(p)]);
    expectedPaths = _.sortBy(expectedPaths, [p => getCost(p), p => getVertices(p).length, p => getVertices(p)]);
    assertEqual(expectedPaths, actualPaths, messages.join('; '));
    //assertTrue(_.isEqual(expectedPaths, actualPaths), messages.join('; '));
  };
};

const checkResIsValidBfsOf = checkResIsValidStackBasedTraversalOfFunc(_.identity, p => p.length);
const checkResIsValidWsOf = checkResIsValidStackBasedTraversalOfFunc(p => p.path, p => p.weight);

/**
 * @brief This function asserts that a list of paths is in BFS order and reaches
 *        exactly the provided vertices.
 *
 * @param expectedVertices An array of expected vertices.
 * @param actualPaths      An array of paths to be checked.
 *
 * Checks that the paths are increasing in length, and each vertex is the last
 * node of exactly one of the paths.
 *
 *   It does check that the path-prefixes are matching. E.g. for a graph
 *         B       E
 *       ↗   ↘   ↗
 *     A       D
 *       ↘   ↗   ↘
 *         C       F
 *   this method would regard
 *   [[a], [a, b], [a, c], [a, c, d], [a, c, d, e], [a, b, d, f]]
 *   as invalid, because the last path would have to be [a, c, d, f] with respect to
 *   the previous results.
 */
const checkResIsValidGlobalBfsOfFunc = function(getVertices, getCost) {
  return (expectedVertices, actualPaths) => {
    assertTrue(!_.isEmpty(actualPaths));
    const actualPathsVertices = actualPaths.map(getVertices);

    const pathCosts = actualPaths.map(getCost);
    const adjacentPairs = _.zip(pathCosts, _.tail(pathCosts));
    adjacentPairs.pop(); // we don't want the last element, because the tail is shorter
    assertTrue(adjacentPairs.every(([a, b]) => a <= b),
        `Paths are not increasing in length: ${JSON.stringify(actualPathsVertices)}`);

    // sort vertices before comparing
    const actualVertices = _.sortBy(actualPathsVertices.map(p => p[p.length - 1]));
    expectedVertices = _.sortBy(expectedVertices);
    const missingVertices = _.difference(expectedVertices, actualVertices);
    const spuriousVertices = _.difference(actualVertices, expectedVertices);

    const messages = [];
    if (missingVertices.length > 0) {
      messages.push('The following vertices are missing: ' + JSON.stringify(missingVertices));
    }
    if (spuriousVertices.length > 0) {
      messages.push('The following vertices are wrong: ' + JSON.stringify(spuriousVertices));
    }

    assertEqual(expectedVertices, actualVertices, messages.join('; '));

    // check that all paths start at the same vertex
    const startVertices = new Set(actualPathsVertices.map(p => p[0]));
    assertTrue(startVertices.size === 1, "paths did not start at same vertex", startVertices);

    // create a dict that contains maps a vertex to its predecessor
    const vertexPairs = _.fromPairs(actualPathsVertices.filter(p => p.length > 1).map(p => p.slice(-2).reverse()));
    // create moving windows of length 2 on all paths
    const pathEdgeList = actualPathsVertices.filter(p => p.length > 1).map(p => {
      const pairs = _.zip(p, _.tail(p));
      pairs.pop();
      return pairs;
    });

    // check that each path is a path in the dict
    const badPaths = pathEdgeList.filter(p => !p.every(q => vertexPairs[q[1]] === q[0]));
    assertTrue(badPaths.length === 0, "paths do not lie within a tree", badPaths);
  };
};

const checkResIsValidGlobalBfsOf = checkResIsValidGlobalBfsOfFunc(_.identity, p => p.length);
const checkResIsValidGlobalWsOf = checkResIsValidGlobalBfsOfFunc(p => p.path, p => p.weight);

const assertResIsContainedInPathList = (allowedPaths, actualPath) => {

  const pathFound = undefined !==  _.find(allowedPaths, (path) => _.isEqual(path, actualPath));
  if (!pathFound) {
    print("ShortestPath result not as expected!");
    print("Allowed paths are: ");
    print(allowedPaths);
    print("Actual returned path is: ");
    print(actualPath);
  }

  assertTrue(pathFound);
};

const assertResIsEqualInPathList = (necessaryPaths, foundPaths) => {
  // order does not matter, but we need to find all paths
  _.each(foundPaths, function(path) {
    assertResIsContainedInPathList(necessaryPaths, path);
  });
};

/**
 * Generates a test function that checks if the result is a valid shortest path result.
 * - getCost has one parameter (path) and should return is cost
 *
 * The returned test function only works for graphs without parallel edges.
 * allowedPaths must be a set of allowed paths and actualPaths is the result of a kShortestPath query
 * where limit is set to the limit as in the query.
 */
const checkResIsValidKShortestPathListWeightFunc  = (getCost) => {
  return (allowedPaths, actualPaths, limit) => {
    // check that we've only got as many paths as requested
    if (actualPaths.length > limit) {
      print("Unexpected amount of found paths!");
      print("Allowed paths are:");
      print(allowedPaths);
      print("Actual returned paths are: ");
      print(actualPaths);
    }
    // we're allowed to find less or equal the amount of the set limit
    assertTrue(actualPaths.length <= limit);

    // assert that there are no duplicate paths (only if there are no parallel edges)
    const stringifiedPathsSet = new Set(actualPaths.map(JSON.stringify));
    assertEqual(stringifiedPathsSet.size, actualPaths.length);

    assertTrue(_.isEqual(_.sortBy(allowedPaths, getCost), allowedPaths));
    assertTrue(allowedPaths.length >= actualPaths.length);

    _.each(actualPaths,  (path, index) => {
      const cost = getCost(path);
      const allowedCost = getCost(allowedPaths[index]);
      if (allowedCost !== cost) {
        print ("Path length not as expected: ");
        print(allowedCost , " !== ", cost);
        if (allowedCost < cost) {
          print ("Traversal missed a shorter path");
        }
      }
      assertEqual(allowedCost, cost);
      assertResIsContainedInPathList(allowedPaths, path);
    });
  };
};

const checkResIsValidKShortestPathListNoWeights = checkResIsValidKShortestPathListWeightFunc((path) => path.length);
const checkResIsValidKShortestPathListWeights = checkResIsValidKShortestPathListWeightFunc((path) => path.weight);


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
  checkResIsValidDfsOf(expectedPathsAsTree, paths);
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
  checkResIsValidDfsOf(expectedPathsAsTree, paths);
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
  checkResIsValidDfsOf(expectedPathsAsTree, paths);
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
  checkResIsValidDfsOf(expectedPathsAsTree, paths);
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
  checkResIsValidDfsOf(expectedPathsAsTree, paths);
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
  checkResIsValidDfsOf(expectedPathsAsTree, paths);
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
  checkResIsValidDfsOf(expectedPathsAsTree, paths);
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
  checkResIsValidDfsOf(expectedPathsAsTree, paths);
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
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  // length 3 path missing
  paths = validPaths.slice();
  paths.splice(2, 1);
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  // subtree missing
  paths = validPaths.slice();
  paths.splice(2, 3);
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  // children missing
  paths = validPaths.slice();
  paths.splice(7, 2);
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  // last path missing
  paths = validPaths.slice();
  paths.splice(paths.length - 1, 1);
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));

  // Superfluous paths:

  // first path duplicate
  paths = validPaths.slice();
  paths.splice(0, 0, paths[0]);
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  // path without children duplicate
  paths = validPaths.slice();
  paths.splice(3, 0, paths[3]);
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  // invalid paths added
  paths = validPaths.concat(["B"]);
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  paths = validPaths.concat(["A", "B", "E"]);
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));

  // Invalid orders:

  paths = validPaths.slice().reverse();
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  paths = validPaths.slice();
  [paths[0], paths[1]] = [paths[1], paths[0]];
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  paths = validPaths.slice();
  [paths[1], paths[5]] = [paths[5], paths[1]];
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  paths = validPaths.slice();
  [paths[4], paths[8]] = [paths[8], paths[4]];
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));

  // Paths with trailing elements:

  paths = validPaths.slice();
  paths[0] = paths[0].concat("A");
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  paths = validPaths.slice();
  paths[0] = paths[0].concat("B");
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  paths = validPaths.slice();
  paths[3] = paths[3].concat("F");
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
  paths = validPaths.slice();
  paths[8] = paths[8].concat("F");
  assertException(() => checkResIsValidDfsOf(expectedPathsAsTree, paths));
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

function testMetaBfsGlobalValid() {
  let expectedVertices;
  let actualPaths;

  expectedVertices = ["A", "B", "C", "D", "E", "F"];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
  ];
  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
  actualPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "F"],
    ["A", "B", "D", "E"],
  ];
  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "F"],
  ];
  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
  actualPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "C", "D"],
    ["A", "C", "D", "F"],
    ["A", "C", "D", "E"],
  ];
  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);

  expectedVertices = ["F", "B", "C", "E", "D", "A",];
  actualPaths = [
    ["A"],
    ["A", "C"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "B", "D", "F"],
    ["A", "B", "D", "E"],
  ];
  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testMetaBfsGlobalInvalid() {
  let expectedVertices;
  let actualPaths;

  expectedVertices = ["A", "B", "C", "D", "E", "F"];
  actualPaths = [
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));

  expectedVertices = ["A", "B", "C", "D", "E", "F"];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "B", "D", "E"],
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));

  expectedVertices = ["A", "B", "C", "D", "E", "F"];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));

  expectedVertices = ["A", "B", "C", "D", "E"];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));

  expectedVertices = ["B", "C", "D", "E", "F"];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));

  expectedVertices = ["A", "B", "C", "D", "E", "F"];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "B", "D"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));


  expectedVertices = ["A", "B", "C", "D", "E", "F"];
  actualPaths = [
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "F"],
    ["A"],
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));


  expectedVertices = ["A", "B", "C", "D", "E", "F"];
  actualPaths = [
    ["A", "B", "D", "E"],
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "B", "D"],
    ["A", "C", "D"],
    ["A", "B", "D", "F"],
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));


  expectedVertices = ["A", "B", "C", "D", "E", "F"];
  actualPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "C", "D"],
    ["A", "C", "D", "E"],
    ["A", "B", "D", "F"]
  ];
  assertException(() => checkResIsValidGlobalBfsOf(expectedVertices, actualPaths));

}

function testMetaWsValid() {
  let expectedPaths;
  let actualPaths;

  expectedPaths = [
    {path: ["A"], weight: 1},
    {path: ["A", "B"], weight: 1.5},
    {path: ["A", "C"], weight: 2},
    {path: ["A", "B", "D"], weight: 3},
    {path: ["A", "C", "D"], weight: 3},
    {path: ["A", "B", "D", "E"], weight: 4},
    {path: ["A", "B", "D", "F"], weight: 4},
    {path: ["A", "C", "D", "E"], weight: 4},
    {path: ["A", "C", "D", "F"], weight: 5},
  ];
  actualPaths = [
    {path: ["A"], weight: 1},
    {path: ["A", "B"], weight: 1.5},
    {path: ["A", "C"], weight: 2},
    {path: ["A", "C", "D"], weight: 3},
    {path: ["A", "B", "D"], weight: 3},
    {path: ["A", "C", "D", "E"], weight: 4},
    {path: ["A", "B", "D", "F"], weight: 4},
    {path: ["A", "B", "D", "E"], weight: 4},
    {path: ["A", "C", "D", "F"], weight: 5},
  ];
  checkResIsValidWsOf(expectedPaths, actualPaths);

  expectedPaths = [
    {path: ["A"], weight: 1},
    {path: ["A", "B"], weight: 1.5},
    {path: ["A", "C", "D"], weight: 2},
    {path: ["A", "C"], weight: 2},
    {path: ["A", "C", "D", "E"], weight: 2.5},
    {path: ["A", "B", "D"], weight: 3},
    {path: ["A", "B", "D", "E"], weight: 4},
    {path: ["A", "B", "D", "F"], weight: 4},
    {path: ["A", "C", "D", "F"], weight: 5},
  ];
  actualPaths = [
    {path: ["A"], weight: 1},
    {path: ["A", "B"], weight: 1.5},
    {path: ["A", "C"], weight: 2},
    {path: ["A", "C", "D"], weight: 2},
    {path: ["A", "C", "D", "E"], weight: 2.5},
    {path: ["A", "B", "D"], weight: 3},
    {path: ["A", "B", "D", "F"], weight: 4},
    {path: ["A", "B", "D", "E"], weight: 4},
    {path: ["A", "C", "D", "F"], weight: 5},
  ];
  checkResIsValidWsOf(expectedPaths, actualPaths);
}

function generateKShortestPathQuery(graph, from, to, limit) {
  return aql`
        FOR p IN OUTBOUND K_SHORTEST_PATHS ${graph.vertex(from)} TO ${graph.vertex(to)}  
        GRAPH ${graph.name()}
        LIMIT ${limit}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;
}

function generateKShortestPathQueryWithWeights(graph, from, to, limit) {
  return aql`
        FOR p IN OUTBOUND K_SHORTEST_PATHS ${graph.vertex(from)} TO ${graph.vertex(to)}  
        GRAPH ${graph.name()}
        OPTIONS {weightAttribute: ${graph.weightAttribute()}}
        LIMIT ${limit}
        RETURN {vertices: p.vertices[*].key, weight: p.weight}
      `;
}

function generateKShortestPathQueryWT(graph, from, to, limit) {
  return aql`
        FOR v, e, p IN 0..100 OUTBOUND ${graph.vertex(from)}  
        GRAPH ${graph.name()}
        PRUNE p.vertices[-1]._id == ${graph.vertex(to)}
        OPTIONS {order: "weighted", uniqueVertices: "path"}
        FILTER p.vertices[-1]._id == ${graph.vertex(to)}
        LIMIT ${limit}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;
}

function generateKShortestPathQueryWithWeightsWT(graph, from, to, limit) {
  return aql`
        FOR v, e, p IN 0..100 OUTBOUND ${graph.vertex(from)}  
        GRAPH ${graph.name()}
        PRUNE p.vertices[-1]._id == ${graph.vertex(to)}
        OPTIONS {order: "weighted", uniqueVertices: "path", weightAttribute: ${graph.weightAttribute()}}
        FILTER p.vertices[-1]._id == ${graph.vertex(to)}
        LIMIT ${limit}
        RETURN {vertices: p.vertices[*].key, weight: p.weights[-1]}
      `;
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testOpenDiamondDfsLabelVariableForwarding(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));

  const ruleList = [["-all"], ["+all"], ["-all", "+optimize-traversals"], ["+all", "-optimize-traversals"]];
  for (const rules of ruleList) {
    const query = `
        LET label = NOOPT(100)
        FOR v, e, p IN 0..3 OUTBOUND "${testGraph.vertex('A')}"
          GRAPH "${testGraph.name()}"
          FILTER p.edges[0].distance != label
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

    const expectedPathsAsTree =
      new Node("A", [
        new Node("C", [
          new Node("D", [
            new Node("E"),
            new Node("F"),
          ]),
        ])
      ]);
    const res = db._query(query, {},  { optimizer: { rules } });
    const actualPaths = res.toArray();

    checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
  }
}

function testOpenDiamondWeightedUniqueVerticesPathEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "C"], weight: 1},
    {path: ["A", "C", "D"], weight: 2},
    {path: ["A", "C", "D", "E"], weight: 3},
    {path: ["A", "C", "D", "F"], weight: 3},
    {path: ["A", "B"], weight: 100},
    {path: ["A", "B", "D"], weight: 101},
    {path: ["A", "B", "D", "E"], weight: 102},
    {path: ["A", "B", "D", "F"], weight: 102},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testOpenDiamondBfsUniqueVerticesPath = (testGraph) => testOpenDiamondModeUniqueVerticesPath(testGraph, "bfs");
const testOpenDiamondWeightedUniqueVerticesPath = (testGraph) => testOpenDiamondModeUniqueVerticesPath(testGraph, "weighted");

function testOpenDiamondModeUniqueVerticesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testOpenDiamondWeightedUniqueVerticesNoneEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "C"], weight: 1},
    {path: ["A", "C", "D"], weight: 2},
    {path: ["A", "C", "D", "E"], weight: 3},
    {path: ["A", "C", "D", "F"], weight: 3},
    {path: ["A", "B"], weight: 100},
    {path: ["A", "B", "D"], weight: 101},
    {path: ["A", "B", "D", "E"], weight: 102},
    {path: ["A", "B", "D", "F"], weight: 102},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testOpenDiamondBfsUniqueVerticesNone = (testGraph) => testOpenDiamondModeUniqueVerticesNone(testGraph, "bfs");
const testOpenDiamondWeightedUniqueVerticesNone = (testGraph) => testOpenDiamondModeUniqueVerticesNone(testGraph, "weighted");

function testOpenDiamondModeUniqueVerticesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testOpenDiamondWeightedUniqueVerticesGlobalEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "global", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;


  const expectedVertices = ["A", "C", "B", "D", "E", "F"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalWsOf(expectedVertices, actualPaths);
}

const testOpenDiamondBfsUniqueVerticesGlobal = (testGraph) => testOpenDiamondModeUniqueVerticesGlobal(testGraph, "bfs");
const testOpenDiamondWeightedUniqueVerticesGlobal = (testGraph) => testOpenDiamondModeUniqueVerticesGlobal(testGraph, "weighted");

function testOpenDiamondModeUniqueVerticesGlobal(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "global", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedVertices = ["A", "C", "B", "D", "E", "F"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testOpenDiamondWeightedUniqueEdgesPathEnableWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "C"], weight: 1},
    {path: ["A", "C", "D"], weight: 2},
    {path: ["A", "C", "D", "E"], weight: 3},
    {path: ["A", "C", "D", "F"], weight: 3},
    {path: ["A", "B"], weight: 100},
    {path: ["A", "B", "D"], weight: 101},
    {path: ["A", "B", "D", "E"], weight: 102},
    {path: ["A", "B", "D", "F"], weight: 102},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testOpenDiamondBfsUniqueEdgesPath = (testGraph) => testOpenDiamondParamUniqueEdgesPath(testGraph, "bfs");
const testOpenDiamondWeightedUniqueEdgesPath = (testGraph) => testOpenDiamondParamUniqueEdgesPath(testGraph, "weighted");

function testOpenDiamondParamUniqueEdgesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testOpenDiamondWeightedUniqueEdgesNoneEnableWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "C"], weight: 1},
    {path: ["A", "C", "D"], weight: 2},
    {path: ["A", "C", "D", "E"], weight: 3},
    {path: ["A", "C", "D", "F"], weight: 3},
    {path: ["A", "B"], weight: 100},
    {path: ["A", "B", "D"], weight: 101},
    {path: ["A", "B", "D", "E"], weight: 102},
    {path: ["A", "B", "D", "F"], weight: 102},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testOpenDiamondBfsUniqueEdgesNone = (testGraph) => testOpenDiamondModeUniqueEdgesNone(testGraph, "bfs");
const testOpenDiamondWeightedUniqueEdgesNone = (testGraph) => testOpenDiamondModeUniqueEdgesNone(testGraph, "weighted");

function testOpenDiamondModeUniqueEdgesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testOpenDiamondWeightedUniqueEdgesUniqueVerticesPathEnableWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", uniqueVertices: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "C"], weight: 1},
    {path: ["A", "C", "D"], weight: 2},
    {path: ["A", "C", "D", "E"], weight: 3},
    {path: ["A", "C", "D", "F"], weight: 3},
    {path: ["A", "B"], weight: 100},
    {path: ["A", "B", "D"], weight: 101},
    {path: ["A", "B", "D", "E"], weight: 102},
    {path: ["A", "B", "D", "F"], weight: 102},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testOpenDiamondBfsUniqueEdgesUniqueVerticesPath = (testGraph) => testOpenDiamondModeUniqueEdgesUniqueVerticesPath(testGraph, "bfs");
const testOpenDiamondWeightedUniqueEdgesUniqueVerticesPath = (testGraph) => testOpenDiamondModeUniqueEdgesUniqueVerticesPath(testGraph, "weighted");

function testOpenDiamondModeUniqueEdgesUniqueVerticesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", uniqueVertices: "path", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testOpenDiamondWeightedUniqueEdgesUniqueVerticesNoneEnableWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", uniqueVertices: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "C"], weight: 1},
    {path: ["A", "C", "D"], weight: 2},
    {path: ["A", "C", "D", "E"], weight: 3},
    {path: ["A", "C", "D", "F"], weight: 3},
    {path: ["A", "B"], weight: 100},
    {path: ["A", "B", "D"], weight: 101},
    {path: ["A", "B", "D", "E"], weight: 102},
    {path: ["A", "B", "D", "F"], weight: 102},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testOpenDiamondBfsUniqueEdgesUniqueVerticesNone = (testGraph) => testOpenDiamondModeUniqueEdgesUniqueVerticesNone(testGraph, "bfs");
const testOpenDiamondWeightedUniqueEdgesUniqueVerticesNone = (testGraph) => testOpenDiamondModeUniqueEdgesUniqueVerticesNone(testGraph, "weighted");

function testOpenDiamondModeUniqueEdgesUniqueVerticesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", uniqueVertices: "none", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testOpenDiamondWeightedUniqueEdgesUniquePathVerticesGlobalEnableWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", uniqueVertices: "global", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;


  const expectedVertices = ["A", "C", "B", "D", "E", "F"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalWsOf(expectedVertices, actualPaths);
}

const testOpenDiamondBfsUniqueEdgesUniquePathVerticesGlobal = (testGraph) => testOpenDiamondModeUniqueEdgesUniquePathVerticesGlobal(testGraph, "bfs");
const testOpenDiamondWeightedUniqueEdgesUniquePathVerticesGlobal = (testGraph) => testOpenDiamondModeUniqueEdgesUniquePathVerticesGlobal(testGraph, "weighted");

function testOpenDiamondModeUniqueEdgesUniquePathVerticesGlobal(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", uniqueVertices: "global", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedVertices = ["A", "C", "B", "D", "E", "F"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testOpenDiamondWeightedUniqueEdgesUniqueNoneVerticesGlobalEnableWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", uniqueVertices: "global", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;


  const expectedVertices = ["A", "C", "B", "D", "E", "F"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalWsOf(expectedVertices, actualPaths);
}

const testOpenDiamondBfsUniqueEdgesUniqueNoneVerticesGlobal = (testGraph) => testOpenDiamondModeUniqueEdgesUniqueNoneVerticesGlobal(testGraph, "bfs");
const testOpenDiamondWeightedUniqueEdgesUniqueNoneVerticesGlobal = (testGraph) => testOpenDiamondModeUniqueEdgesUniqueNoneVerticesGlobal(testGraph, "weighted");

function testOpenDiamondModeUniqueEdgesUniqueNoneVerticesGlobal(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", uniqueVertices: "global", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedVertices = ["A", "C", "B", "D", "E", "F"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

const testOpenDiamondBfsLabelVariableForwarding = (testGraph) => testOpenDiamondModeLabelVariableForwarding(testGraph, "bfs");
const testOpenDiamondWeightedLabelVariableForwarding = (testGraph) => testOpenDiamondModeLabelVariableForwarding(testGraph, "weighted");

function testOpenDiamondModeLabelVariableForwarding(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));

  const ruleList = [["-all"], ["+all"], ["-all", "+optimize-traversals"], ["+all", "-optimize-traversals"]];
  for (const rules of ruleList) {
    const query = `
        LET label = NOOPT(100)
        FOR v, e, p IN 0..3 OUTBOUND "${testGraph.vertex('A')}"
          GRAPH "${testGraph.name()}" OPTIONS { order: @mode }
          FILTER p.edges[0].distance != label
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

    const expectedPathsAsTree =
      new Node("A", [
        new Node("C", [
          new Node("D", [
            new Node("E"),
            new Node("F"),
          ]),
        ])
      ]);
    const res = db._query(query, {mode},  { optimizer: { rules } });
    const actualPaths = res.toArray();

    checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
  }
}

function testOpenDiamondShortestPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('F')}  
        GRAPH ${testGraph.name()} 
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "B", "D", "F"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

function testOpenDiamondShortestPathWT(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        LET paths = (
          FOR v, e, p IN 0..100 OUTBOUND ${testGraph.vertex('A')}
          GRAPH ${testGraph.name()}
          PRUNE p.vertices[-1]._id == ${testGraph.vertex('F')}
          OPTIONS {order: "weighted", uniqueVertices: "global"}
          FILTER p.vertices[-1]._id == ${testGraph.vertex('F')}
          LIMIT 1
          RETURN p
        )
        FOR p IN paths
          FOR v IN p.vertices
            RETURN v.key
      `;

  const allowedPaths = [
    ["A", "B", "D", "F"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

function testOpenDiamondKPaths(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR path IN 1..3 OUTBOUND K_PATHS ${testGraph.vertex('A')} TO ${testGraph.vertex('F')}
        GRAPH ${testGraph.name()}
        RETURN path.vertices[* RETURN CURRENT.key]
      `;

  const necessaryPaths = [
    ["A", "B", "D", "F"],
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const foundPaths = res.toArray();

  assertResIsEqualInPathList(necessaryPaths, foundPaths);
}

function testOpenDiamondShortestPathEnabledWeightCheck(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('F')}
        GRAPH ${testGraph.name()} 
        OPTIONS {weightAttribute: ${testGraph.weightAttribute()}}
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

function testOpenDiamondShortestPathEnabledWeightCheckWT(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const query = aql`
        LET paths = (
          FOR v, e, p IN 0..100 OUTBOUND ${testGraph.vertex('A')}
          GRAPH ${testGraph.name()}
          PRUNE p.vertices[-1]._id == ${testGraph.vertex('F')}
          OPTIONS {order: "weighted", uniqueVertices: "global", weightAttribute: ${testGraph.weightAttribute()}}
          FILTER p.vertices[-1]._id == ${testGraph.vertex('F')}
          LIMIT 1
          RETURN p
        )
        FOR p IN paths
          FOR v IN p.vertices
            RETURN v.key
      `;

  const allowedPaths = [
    ["A", "C", "D", "F"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

const testOpenDiamondKShortestPathWithMultipleLimits = (testGraph) => testOpenDiamondKShortestPathWithMultipleLimitsGen(testGraph, generateKShortestPathQuery);
const testOpenDiamondKShortestPathWithMultipleLimitsWT = (testGraph) => testOpenDiamondKShortestPathWithMultipleLimitsGen(testGraph, generateKShortestPathQueryWT);

function testOpenDiamondKShortestPathWithMultipleLimitsGen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const limits = [1, 2, 3, 4];

  _.each(limits, function (limit) {
    const query = generator(testGraph, 'A', 'F', limit);

    const allowedPaths = [
      ["A", "B", "D", "F"],
      ["A", "C", "D", "F"]
    ];

    const res = db._query(query);
    const actualPath = res.toArray();

    checkResIsValidKShortestPathListNoWeights(allowedPaths, actualPath, limit);
  });
}

const testOpenDiamondKShortestPathEnabledWeightCheckLimit1 = (testGraph) => testOpenDiamondKShortestPathEnabledWeightCheckLimit1Gen(testGraph, generateKShortestPathQueryWithWeights);
const testOpenDiamondKShortestPathEnabledWeightCheckLimit1WT = (testGraph) => testOpenDiamondKShortestPathEnabledWeightCheckLimit1Gen(testGraph, generateKShortestPathQueryWithWeightsWT);

function testOpenDiamondKShortestPathEnabledWeightCheckLimit1Gen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.openDiamond.name()));
  const limits = [1, 2, 3];
  _.each(limits, (limit) => {
    const query = generator(testGraph, 'A', 'F', limit);

    const allowedPaths = [
      {vertices: ["A", "C", "D", "F"], weight: 3},
      {vertices: ["A", "B", "D", "F"], weight: 102}
    ];

    const res = db._query(query);
    const actualPath = res.toArray();

    checkResIsValidKShortestPathListWeights(allowedPaths, actualPath, limit);
  });
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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
            new Node("A")
          ])
        ])
      ])
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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
            new Node("A")
          ])
        ])
      ])
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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
            new Node("A", [
              new Node("B", [
                new Node("C", [
                  new Node("D", [
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
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
            new Node("A", [
              new Node("B", [
                new Node("C", [
                  new Node("D", [
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

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

const testSmallCircleBfsUniqueVerticesPath = (testGraph) => testSmallCircleModeUniqueVerticesPath(testGraph, "bfs");
const testSmallCircleWeightedUniqueVerticesPath = (testGraph) => testSmallCircleModeUniqueVerticesPath(testGraph, "weighted");

function testSmallCircleModeUniqueVerticesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

const testSmallCircleBfsUniqueVerticesNone = (testGraph) => testSmallCircleModeUniqueVerticesNone(testGraph, "bfs");
const testSmallCircleWeightedUniqueVerticesNone = (testGraph) => testSmallCircleModeUniqueVerticesNone(testGraph, "weighted");

function testSmallCircleModeUniqueVerticesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

const testSmallCircleBfsUniqueEdgesPath = (testGraph) => testSmallCircleModeUniqueEdgesPath(testGraph, "bfs");
const testSmallCircleWeightedUniqueEdgesPath = (testGraph) => testSmallCircleModeUniqueEdgesPath(testGraph, "weighted");

function testSmallCircleModeUniqueEdgesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

const testSmallCircleBfsUniqueEdgesNone = (testGraph) => testSmallCircleModeUniqueEdgesNone(testGraph, "bfs");
const testSmallCircleWeightedUniqueEdgesNone = (testGraph) => testSmallCircleModeUniqueEdgesNone(testGraph, "weighted");

function testSmallCircleModeUniqueEdgesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

const testSmallCircleBfsUniqueVerticesUniqueEdgesPath = (testGraph) => testSmallCircleModeUniqueVerticesUniqueEdgesPath(testGraph, "bfs");
const testSmallCircleWeightedUniqueVerticesUniqueEdgesPath = (testGraph) => testSmallCircleModeUniqueVerticesUniqueEdgesPath(testGraph, "weighted");

function testSmallCircleModeUniqueVerticesUniqueEdgesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", uniqueEdges: "path", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

const testSmallCircleBfsUniqueVerticesUniqueEdgesNone = (testGraph) => testSmallCircleModeUniqueVerticesUniqueEdgesNone(testGraph, "bfs");
const testSmallCircleWeightedUniqueVerticesUniqueEdgesNone = (testGraph) => testSmallCircleModeUniqueVerticesUniqueEdgesNone(testGraph, "weighted");

function testSmallCircleModeUniqueVerticesUniqueEdgesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", uniqueEdges: "none", order: ${mode}}
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

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

const testSmallCircleBfsUniqueEdgesPathUniqueVerticesGlobal = (testGraph) => testSmallCircleModeUniqueEdgesPathUniqueVerticesGlobal(testGraph, "bfs");
const testSmallCircleWeightedUniqueEdgesPathUniqueVerticesGlobal = (testGraph) => testSmallCircleModeUniqueEdgesPathUniqueVerticesGlobal(testGraph, "weighted");

function testSmallCircleModeUniqueEdgesPathUniqueVerticesGlobal(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "global", uniqueEdges: "path", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedVertices = ["A", "B", "C", "D"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

const testSmallCircleBfsUniqueEdgesNoneUniqueVerticesGlobal = (testGraph) => testSmallCircleModeUniqueEdgesNoneUniqueVerticesGlobal(testGraph, "bfs");
const testSmallCircleWeightedUniqueEdgesNoneUniqueVerticesGlobal = (testGraph) => testSmallCircleModeUniqueEdgesNoneUniqueVerticesGlobal(testGraph, "weighted");

function testSmallCircleModeUniqueEdgesNoneUniqueVerticesGlobal(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e, p IN 0..9 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "global", uniqueEdges: "none", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedVertices = ["A", "B", "C", "D"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testSmallCircleShortestPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('D')}  
        GRAPH ${testGraph.name()} 
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "B", "C", "D"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

function testSmallCircleKPaths(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR path IN 1..3 OUTBOUND K_PATHS ${testGraph.vertex('A')} TO ${testGraph.vertex('D')}
        GRAPH ${testGraph.name()}
        RETURN path.vertices[* RETURN CURRENT.key]
      `;

  const necessaryPaths = [
    ["A", "B", "C", "D"]
  ];

  const res = db._query(query);
  const foundPaths = res.toArray();

  assertResIsEqualInPathList(necessaryPaths, foundPaths);
}

function testSmallCircleShortestPathEnabledWeightCheck(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('D')}  
        GRAPH ${testGraph.name()} 
        OPTIONS {weightAttribute: ${testGraph.weightAttribute()}}
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "B", "C", "D"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

const testSmallCircleKShortestPathWithMultipleLimits = (testGraph) => testSmallCircleKShortestPathWithMultipleLimitsGen(testGraph, generateKShortestPathQuery);
const testSmallCircleKShortestPathWithMultipleLimitsWT = (testGraph) => testSmallCircleKShortestPathWithMultipleLimitsGen(testGraph, generateKShortestPathQueryWT);

function testSmallCircleKShortestPathWithMultipleLimitsGen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const limits = [1, 2, 3, 4];

  _.each(limits, function (limit) {
    const query = generator(testGraph, 'A', 'D', limit);

    const allowedPaths = [
      ["A", "B", "C", "D"]
    ];

    const res = db._query(query);
    const actualPath = res.toArray();

    checkResIsValidKShortestPathListNoWeights(allowedPaths, actualPath, limit);
  });
}

const testSmallCircleKShortestPathEnabledWeightCheckWithMultipleLimits = (testGraph) => testSmallCircleKShortestPathEnabledWeightCheckWithMultipleLimitsGen(testGraph, generateKShortestPathQueryWithWeights);
const testSmallCircleKShortestPathEnabledWeightCheckWithMultipleLimitsWT = (testGraph) => testSmallCircleKShortestPathEnabledWeightCheckWithMultipleLimitsGen(testGraph, generateKShortestPathQueryWithWeightsWT);

function testSmallCircleKShortestPathEnabledWeightCheckWithMultipleLimitsGen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.smallCircle.name()));
  const limits = [1, 2, 3, 4];

  _.each(limits, function (limit) {
    const query = generator(testGraph, 'A', 'D', limit);

    const allowedPaths = [
      {vertices: ["A", "B", "C", "D"], weight: 3}
    ];

    const res = db._query(query);
    const actualPath = res.toArray();

    checkResIsValidKShortestPathListWeights(allowedPaths, actualPath, limit);
  });
}

function testCompleteGraphDfsUniqueVerticesPathD1(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B"),
      new Node("C"),
      new Node("D"),
      new Node("E"),
    ])
  ;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testCompleteGraphDfsUniqueEdgesPathD1(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B"),
      new Node("C"),
      new Node("D"),
      new Node("E"),
    ])
  ;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testCompleteGraphDfsUniqueVerticesPathD2(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C"),
        new Node("D"),
        new Node("E"),
      ]),
      new Node("C", [
        new Node("B"),
        new Node("D"),
        new Node("E"),
      ]),
      new Node("D", [
        new Node("B"),
        new Node("C"),
        new Node("E"),
      ]),
      new Node("E", [
        new Node("B"),
        new Node("C"),
        new Node("D"),
      ])
    ])
  ;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testCompleteGraphDfsUniqueEdgesPathD2(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("A"),
        new Node("C"),
        new Node("D"),
        new Node("E"),
      ]),
      new Node("C", [
        new Node("A"),
        new Node("B"),
        new Node("D"),
        new Node("E"),
      ]),
      new Node("D", [
        new Node("A"),
        new Node("B"),
        new Node("C"),
        new Node("E"),
      ]),
      new Node("E", [
        new Node("A"),
        new Node("B"),
        new Node("C"),
        new Node("D"),
      ])
    ])
  ;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testCompleteGraphDfsUniqueVerticesPathD3(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D"),
          new Node("E")
        ]),
        new Node("D", [
          new Node("C"),
          new Node("E")
        ]),
        new Node("E", [
          new Node("C"),
          new Node("D")
        ])
      ]),
      new Node("C", [
        new Node("B", [
          new Node("D"),
          new Node("E")
        ]),
        new Node("D", [
          new Node("B"),
          new Node("E")
        ]),
        new Node("E", [
          new Node("B"),
          new Node("D")
        ])
      ]),
      new Node("D", [
        new Node("B", [
          new Node("C"),
          new Node("E")
        ]),
        new Node("C", [
          new Node("E"),
          new Node("B")
        ]),
        new Node("E", [
          new Node("C"),
          new Node("B")
        ])
      ]),
      new Node("E", [
        new Node("B", [
          new Node("C"),
          new Node("D")
        ]),
        new Node("C", [
          new Node("B"),
          new Node("D")
        ]),
        new Node("D", [
          new Node("C"),
          new Node("B")
        ])
      ])
    ])
  ;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testCompleteGraphDfsUniqueVerticesUniqueEdgesPathD2(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "path", uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C"),
        new Node("D"),
        new Node("E"),
      ]),
      new Node("C", [
        new Node("B"),
        new Node("D"),
        new Node("E"),
      ]),
      new Node("D", [
        new Node("B"),
        new Node("C"),
        new Node("E"),
      ]),
      new Node("E", [
        new Node("B"),
        new Node("C"),
        new Node("D"),
      ])
    ])
  ;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testCompleteGraphDfsUniqueVerticesUniqueEdgesNoneD2(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "none", uniqueEdges: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("A"),
        new Node("C"),
        new Node("D"),
        new Node("E"),
      ]),
      new Node("C", [
        new Node("A"),
        new Node("B"),
        new Node("D"),
        new Node("E"),
      ]),
      new Node("D", [
        new Node("A"),
        new Node("B"),
        new Node("C"),
        new Node("E"),
      ]),
      new Node("E", [
        new Node("A"),
        new Node("B"),
        new Node("C"),
        new Node("D"),
      ])
    ])
  ;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testCompleteGraphWeightedUniqueVerticesPathD1EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "E"], weight: 1},
    {path: ["A", "B"], weight: 2},
    {path: ["A", "D"], weight: 5},
    {path: ["A", "C"], weight: 5},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueVerticesPathD1 = (testGraph) => testCompleteGraphModeUniqueVerticesPathD1(testGraph, "bfs");
const testCompleteGraphWeightedUniqueVerticesPathD1 = (testGraph) => testCompleteGraphModeUniqueVerticesPathD1(testGraph, "weighted");

function testCompleteGraphModeUniqueVerticesPathD1(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueVerticesPathD2EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "E"], weight: 1},
    {path: ["A", "B"], weight: 2},
    {path: ["A", "E", "D"], weight: 2},
    {path: ["A", "B", "C"], weight: 4},
    {path: ["A", "C"], weight: 5},
    {path: ["A", "D"], weight: 5},
    {path: ["A", "E", "B"], weight: 6},
    {path: ["A", "E", "C"], weight: 6},

    {path: ["A", "C", "B"], weight: 6},
    {path: ["A", "B", "D"], weight: 7},
    {path: ["A", "B", "E"], weight: 7},
    {path: ["A", "D", "C"], weight: 7},

    {path: ["A", "D", "E"], weight: 7},
    {path: ["A", "C", "D"], weight: 7},
    {path: ["A", "D", "B"], weight: 10},
    {path: ["A", "C", "E"], weight: 10},
  ];


  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueVerticesPathD2 = (testGraph) => testCompleteGraphModeUniqueVerticesPathD2(testGraph, "bfs");
const testCompleteGraphWeightedUniqueVerticesPathD2 = (testGraph) => testCompleteGraphModeUniqueVerticesPathD2(testGraph, "weighted");

function testCompleteGraphModeUniqueVerticesPathD2(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],
    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],
    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],
    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],
    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueVerticesPathD3EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "E"], weight: 1},
    {path: ["A", "B"], weight: 2},
    {path: ["A", "E", "D"], weight: 2},
    {path: ["A", "B", "C"], weight: 4},
    {path: ["A", "E", "D", "C"], weight: 4},
    {path: ["A", "C"], weight: 5},
    {path: ["A", "D"], weight: 5},
    {path: ["A", "C", "B"], weight: 6},
    {path: ["A", "E", "C"], weight: 6},
    {path: ["A", "B", "C", "D"], weight: 6},
    {path: ["A", "E", "B"], weight: 6},
    {path: ["A", "B", "E"], weight: 7},
    {path: ["A", "B", "D"], weight: 7},
    {path: ["A", "C", "D"], weight: 7},
    {path: ["A", "E", "D", "B"], weight: 7},
    {path: ["A", "E", "C", "B"], weight: 7},
    {path: ["A", "D", "E"], weight: 7},
    {path: ["A", "D", "C"], weight: 7},
    {path: ["A", "B", "E", "D"], weight: 8},
    {path: ["A", "E", "B", "C"], weight: 8},
    {path: ["A", "D", "C", "B"], weight: 8},
    {path: ["A", "E", "C", "D"], weight: 8},
    {path: ["A", "B", "C", "E"], weight: 9},
    {path: ["A", "B", "D", "E"], weight: 9},
    {path: ["A", "B", "D", "C"], weight: 9},
    {path: ["A", "C", "D", "E"], weight: 9},
    {path: ["A", "C", "E"], weight: 10},
    {path: ["A", "D", "B"], weight: 10},
    {path: ["A", "C", "E", "D"], weight: 11},
    {path: ["A", "C", "B", "D"], weight: 11},
    {path: ["A", "C", "B", "E"], weight: 11},
    {path: ["A", "E", "B", "D"], weight: 11},
    {path: ["A", "B", "E", "C"], weight: 12},
    {path: ["A", "C", "D", "B"], weight: 12},
    {path: ["A", "D", "B", "C"], weight: 12},
    {path: ["A", "D", "C", "E"], weight: 12},
    {path: ["A", "D", "E", "B"], weight: 12},
    {path: ["A", "D", "E", "C"], weight: 12},
    {path: ["A", "C", "E", "B"], weight: 15},
    {path: ["A", "D", "B", "E"], weight: 15},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueVerticesPathD3 = (testGraph) => testCompleteGraphModeUniqueVerticesPathD3(testGraph, "bfs");
const testCompleteGraphWeightedUniqueVerticesPathD3 = (testGraph) => testCompleteGraphModeUniqueVerticesPathD3(testGraph, "weighted");

function testCompleteGraphModeUniqueVerticesPathD3(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],
    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],
    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],
    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],
    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"],
    ["A", "B", "C", "D"],
    ["A", "B", "C", "E"],
    ["A", "B", "D", "E"],
    ["A", "B", "D", "C"],
    ["A", "B", "E", "C"],
    ["A", "B", "E", "D"],
    ["A", "C", "B", "D"],
    ["A", "C", "B", "E"],
    ["A", "C", "D", "E"],
    ["A", "C", "D", "B"],
    ["A", "C", "E", "D"],
    ["A", "C", "E", "B"],
    ["A", "D", "B", "C"],
    ["A", "D", "B", "E"],
    ["A", "D", "C", "E"],
    ["A", "D", "C", "B"],
    ["A", "D", "E", "B"],
    ["A", "D", "E", "C"],
    ["A", "E", "B", "D"],
    ["A", "E", "B", "C"],
    ["A", "E", "C", "B"],
    ["A", "E", "C", "D"],
    ["A", "E", "D", "B"],
    ["A", "E", "D", "C"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueVerticesNoneD1EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ["A"], weight: 0},
    {path: ["A", "E"], weight: 1},
    {path: ["A", "B"], weight: 2},
    {path: ["A", "C"], weight: 5},
    {path: ["A", "D"], weight: 5},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueVerticesNoneD1 = (testGraph) => testCompleteGraphModeUniqueVerticesNoneD1(testGraph, "bfs");
const testCompleteGraphWeightedUniqueVerticesNoneD1 = (testGraph) => testCompleteGraphModeUniqueVerticesNoneD1(testGraph, "weighted");

function testCompleteGraphModeUniqueVerticesNoneD1(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueVerticesNoneD2EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'E', 'D'], weight: 2},
    {path: ['A', 'B', 'A'], weight: 3},
    {path: ['A', 'E', 'A'], weight: 3},
    {path: ['A', 'B', 'C'], weight: 4},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
    {path: ['A', 'C', 'B'], weight: 6},
    {path: ['A', 'E', 'B'], weight: 6},
    {path: ['A', 'E', 'C'], weight: 6},
    {path: ['A', 'B', 'D'], weight: 7},
    {path: ['A', 'B', 'E'], weight: 7},
    {path: ['A', 'C', 'D'], weight: 7},
    {path: ['A', 'D', 'C'], weight: 7},
    {path: ['A', 'D', 'E'], weight: 7},
    {path: ['A', 'C', 'A'], weight: 10},
    {path: ['A', 'C', 'E'], weight: 10},
    {path: ['A', 'D', 'A'], weight: 10},
    {path: ['A', 'D', 'B'], weight: 10},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueVerticesNoneD2 = (testGraph) => testCompleteGraphModeUniqueVerticesNoneD2(testGraph, "bfs");
const testCompleteGraphWeightedUniqueVerticesNoneD2 = (testGraph) => testCompleteGraphModeUniqueVerticesNoneD2(testGraph, "weighted");

function testCompleteGraphModeUniqueVerticesNoneD2(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],
    ["A", "B", "A"],
    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],
    ["A", "C", "A"],
    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],
    ["A", "D", "A"],
    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],
    ["A", "E", "A"],
    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueVerticesNoneD3EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'E', 'D'], weight: 2},
    {path: ['A', 'B', 'A'], weight: 3},
    {path: ['A', 'E', 'A'], weight: 3},
    {path: ['A', 'B', 'C'], weight: 4},
    {path: ['A', 'B', 'A', 'E'], weight: 4},
    {path: ['A', 'E', 'D', 'C'], weight: 4},
    {path: ['A', 'E', 'D', 'E'], weight: 4},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
    {path: ['A', 'B', 'C', 'B'], weight: 5},
    {path: ['A', 'E', 'A', 'B'], weight: 5},
    {path: ['A', 'C', 'B'], weight: 6},
    {path: ['A', 'E', 'B'], weight: 6},
    {path: ['A', 'E', 'C'], weight: 6},
    {path: ['A', 'B', 'C', 'D'], weight: 6},
    {path: ['A', 'B', 'D'], weight: 7},
    {path: ['A', 'B', 'E'], weight: 7},
    {path: ['A', 'C', 'D'], weight: 7},
    {path: ['A', 'D', 'C'], weight: 7},
    {path: ['A', 'D', 'E'], weight: 7},
    {path: ['A', 'C', 'B', 'A'], weight: 7},
    {path: ['A', 'E', 'B', 'A'], weight: 7},
    {path: ['A', 'E', 'C', 'B'], weight: 7},
    {path: ['A', 'E', 'D', 'A'], weight: 7},
    {path: ['A', 'E', 'D', 'B'], weight: 7},
    {path: ['A', 'B', 'A', 'C'], weight: 8},
    {path: ['A', 'B', 'A', 'D'], weight: 8},
    {path: ['A', 'B', 'E', 'D'], weight: 8},
    {path: ['A', 'C', 'B', 'C'], weight: 8},
    {path: ['A', 'D', 'C', 'B'], weight: 8},
    {path: ['A', 'D', 'E', 'D'], weight: 8},
    {path: ['A', 'E', 'A', 'C'], weight: 8},
    {path: ['A', 'E', 'A', 'D'], weight: 8},
    {path: ['A', 'E', 'B', 'C'], weight: 8},
    {path: ['A', 'E', 'C', 'D'], weight: 8},
    {path: ['A', 'B', 'C', 'A'], weight: 9},
    {path: ['A', 'B', 'C', 'E'], weight: 9},
    {path: ['A', 'B', 'D', 'C'], weight: 9},
    {path: ['A', 'B', 'D', 'E'], weight: 9},
    {path: ['A', 'B', 'E', 'A'], weight: 9},
    {path: ['A', 'C', 'D', 'C'], weight: 9},
    {path: ['A', 'C', 'D', 'E'], weight: 9},
    {path: ['A', 'D', 'C', 'D'], weight: 9},
    {path: ['A', 'D', 'E', 'A'], weight: 9},
    {path: ['A', 'C', 'A'], weight: 10},
    {path: ['A', 'C', 'E'], weight: 10},
    {path: ['A', 'D', 'A'], weight: 10},
    {path: ['A', 'D', 'B'], weight: 10},
    {path: ['A', 'C', 'A', 'E'], weight: 11},
    {path: ['A', 'C', 'B', 'D'], weight: 11},
    {path: ['A', 'C', 'B', 'E'], weight: 11},
    {path: ['A', 'C', 'E', 'D'], weight: 11},
    {path: ['A', 'D', 'A', 'E'], weight: 11},
    {path: ['A', 'D', 'B', 'A'], weight: 11},
    {path: ['A', 'E', 'B', 'D'], weight: 11},
    {path: ['A', 'E', 'B', 'E'], weight: 11},
    {path: ['A', 'E', 'C', 'A'], weight: 11},
    {path: ['A', 'E', 'C', 'E'], weight: 11},
    {path: ['A', 'B', 'D', 'A'], weight: 12},
    {path: ['A', 'B', 'D', 'B'], weight: 12},
    {path: ['A', 'B', 'E', 'B'], weight: 12},
    {path: ['A', 'B', 'E', 'C'], weight: 12},
    {path: ['A', 'C', 'A', 'B'], weight: 12},
    {path: ['A', 'C', 'D', 'A'], weight: 12},
    {path: ['A', 'C', 'D', 'B'], weight: 12},
    {path: ['A', 'C', 'E', 'A'], weight: 12},
    {path: ['A', 'D', 'A', 'B'], weight: 12},
    {path: ['A', 'D', 'B', 'C'], weight: 12},
    {path: ['A', 'D', 'C', 'A'], weight: 12},
    {path: ['A', 'D', 'C', 'E'], weight: 12},
    {path: ['A', 'D', 'E', 'B'], weight: 12},
    {path: ['A', 'D', 'E', 'C'], weight: 12},
    {path: ['A', 'C', 'A', 'D'], weight: 15},
    {path: ['A', 'C', 'E', 'B'], weight: 15},
    {path: ['A', 'C', 'E', 'C'], weight: 15},
    {path: ['A', 'D', 'A', 'C'], weight: 15},
    {path: ['A', 'D', 'B', 'D'], weight: 15},
    {path: ['A', 'D', 'B', 'E'], weight: 15},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueVerticesNoneD3 = (testGraph) => testCompleteGraphModeUniqueVerticesNoneD3(testGraph, "bfs");
const testCompleteGraphWeightedUniqueVerticesNoneD3 = (testGraph) => testCompleteGraphModeUniqueVerticesNoneD3(testGraph, "weighted");

function testCompleteGraphModeUniqueVerticesNoneD3(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],

    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],

    ["A", "B", "A"],
    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],

    ["A", "C", "A"],
    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],

    ["A", "D", "A"],
    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],

    ["A", "E", "A"],
    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"],

    ["A", "B", "A", "C"],
    ["A", "B", "A", "D"],
    ["A", "B", "A", "E"],

    ["A", "C", "A", "B"],
    ["A", "C", "A", "D"],
    ["A", "C", "A", "E"],

    ["A", "D", "A", "B"],
    ["A", "D", "A", "C"],
    ["A", "D", "A", "E"],

    ["A", "E", "A", "B"],
    ["A", "E", "A", "C"],
    ["A", "E", "A", "D"],

    ["A", "B", "C", "A"],
    ["A", "B", "C", "B"],
    ["A", "B", "C", "D"],
    ["A", "B", "C", "E"],

    ["A", "B", "D", "A"],
    ["A", "B", "D", "B"],
    ["A", "B", "D", "C"],
    ["A", "B", "D", "E"],

    ["A", "B", "E", "A"],
    ["A", "B", "E", "B"],
    ["A", "B", "E", "C"],
    ["A", "B", "E", "D"],

    ["A", "C", "B", "A"],
    ["A", "C", "B", "C"],
    ["A", "C", "B", "D"],
    ["A", "C", "B", "E"],

    ["A", "C", "D", "A"],
    ["A", "C", "D", "B"],
    ["A", "C", "D", "C"],
    ["A", "C", "D", "E"],

    ["A", "C", "E", "A"],
    ["A", "C", "E", "B"],
    ["A", "C", "E", "C"],
    ["A", "C", "E", "D"],

    ["A", "D", "B", "A"],
    ["A", "D", "B", "C"],
    ["A", "D", "B", "E"],
    ["A", "D", "B", "D"],

    ["A", "D", "C", "A"],
    ["A", "D", "C", "B"],
    ["A", "D", "C", "D"],
    ["A", "D", "C", "E"],

    ["A", "D", "E", "A"],
    ["A", "D", "E", "B"],
    ["A", "D", "E", "C"],
    ["A", "D", "E", "D"],

    ["A", "E", "B", "A"],
    ["A", "E", "B", "C"],
    ["A", "E", "B", "D"],
    ["A", "E", "B", "E"],

    ["A", "E", "C", "A"],
    ["A", "E", "C", "B"],
    ["A", "E", "C", "D"],
    ["A", "E", "C", "E"],

    ["A", "E", "D", "A"],
    ["A", "E", "D", "B"],
    ["A", "E", "D", "C"],
    ["A", "E", "D", "E"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesPathD1EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesPathD1 = (testGraph) => testCompleteGraphModeUniqueEdgesPathD1(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesPathD1 = (testGraph) => testCompleteGraphModeUniqueEdgesPathD1(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesPathD1(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesPathD2EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'E', 'D'], weight: 2},
    {path: ['A', 'B', 'A'], weight: 3},
    {path: ['A', 'E', 'A'], weight: 3},
    {path: ['A', 'B', 'C'], weight: 4},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
    {path: ['A', 'C', 'B'], weight: 6},
    {path: ['A', 'E', 'B'], weight: 6},
    {path: ['A', 'E', 'C'], weight: 6},
    {path: ['A', 'B', 'D'], weight: 7},
    {path: ['A', 'B', 'E'], weight: 7},
    {path: ['A', 'C', 'D'], weight: 7},
    {path: ['A', 'D', 'C'], weight: 7},
    {path: ['A', 'D', 'E'], weight: 7},
    {path: ['A', 'C', 'A'], weight: 10},
    {path: ['A', 'C', 'E'], weight: 10},
    {path: ['A', 'D', 'A'], weight: 10},
    {path: ['A', 'D', 'B'], weight: 10},

  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesPathD2 = (testGraph) => testCompleteGraphModeUniqueEdgesPathD2(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesPathD2 = (testGraph) => testCompleteGraphModeUniqueEdgesPathD2(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesPathD2(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],

    ["A", "B", "A"],
    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],

    ["A", "C", "A"],
    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],

    ["A", "D", "A"],
    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],

    ["A", "E", "A"],
    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesPathD3EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [{path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'E', 'D'], weight: 2},
    {path: ['A', 'B', 'A'], weight: 3},
    {path: ['A', 'E', 'A'], weight: 3},
    {path: ['A', 'B', 'C'], weight: 4},
    {path: ['A', 'B', 'A', 'E'], weight: 4},
    {path: ['A', 'E', 'D', 'C'], weight: 4},
    {path: ['A', 'E', 'D', 'E'], weight: 4},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
    {path: ['A', 'B', 'C', 'B'], weight: 5},
    {path: ['A', 'E', 'A', 'B'], weight: 5},
    {path: ['A', 'C', 'B'], weight: 6},
    {path: ['A', 'E', 'B'], weight: 6},
    {path: ['A', 'E', 'C'], weight: 6},
    {path: ['A', 'B', 'C', 'D'], weight: 6},
    {path: ['A', 'B', 'D'], weight: 7},
    {path: ['A', 'B', 'E'], weight: 7},
    {path: ['A', 'C', 'D'], weight: 7},
    {path: ['A', 'D', 'C'], weight: 7},
    {path: ['A', 'D', 'E'], weight: 7},
    {path: ['A', 'C', 'B', 'A'], weight: 7},
    {path: ['A', 'E', 'B', 'A'], weight: 7},
    {path: ['A', 'E', 'C', 'B'], weight: 7},
    {path: ['A', 'E', 'D', 'A'], weight: 7},
    {path: ['A', 'E', 'D', 'B'], weight: 7},
    {path: ['A', 'B', 'A', 'C'], weight: 8},
    {path: ['A', 'B', 'A', 'D'], weight: 8},
    {path: ['A', 'B', 'E', 'D'], weight: 8},
    {path: ['A', 'C', 'B', 'C'], weight: 8},
    {path: ['A', 'D', 'C', 'B'], weight: 8},
    {path: ['A', 'D', 'E', 'D'], weight: 8},
    {path: ['A', 'E', 'A', 'C'], weight: 8},
    {path: ['A', 'E', 'A', 'D'], weight: 8},
    {path: ['A', 'E', 'B', 'C'], weight: 8},
    {path: ['A', 'E', 'C', 'D'], weight: 8},
    {path: ['A', 'B', 'C', 'A'], weight: 9},
    {path: ['A', 'B', 'C', 'E'], weight: 9},
    {path: ['A', 'B', 'D', 'C'], weight: 9},
    {path: ['A', 'B', 'D', 'E'], weight: 9},
    {path: ['A', 'B', 'E', 'A'], weight: 9},
    {path: ['A', 'C', 'D', 'C'], weight: 9},
    {path: ['A', 'C', 'D', 'E'], weight: 9},
    {path: ['A', 'D', 'C', 'D'], weight: 9},
    {path: ['A', 'D', 'E', 'A'], weight: 9},
    {path: ['A', 'C', 'A'], weight: 10},
    {path: ['A', 'C', 'E'], weight: 10},
    {path: ['A', 'D', 'A'], weight: 10},
    {path: ['A', 'D', 'B'], weight: 10},
    {path: ['A', 'C', 'A', 'E'], weight: 11},
    {path: ['A', 'C', 'B', 'D'], weight: 11},
    {path: ['A', 'C', 'B', 'E'], weight: 11},
    {path: ['A', 'C', 'E', 'D'], weight: 11},
    {path: ['A', 'D', 'A', 'E'], weight: 11},
    {path: ['A', 'D', 'B', 'A'], weight: 11},
    {path: ['A', 'E', 'B', 'D'], weight: 11},
    {path: ['A', 'E', 'B', 'E'], weight: 11},
    {path: ['A', 'E', 'C', 'A'], weight: 11},
    {path: ['A', 'E', 'C', 'E'], weight: 11},
    {path: ['A', 'B', 'D', 'A'], weight: 12},
    {path: ['A', 'B', 'D', 'B'], weight: 12},
    {path: ['A', 'B', 'E', 'B'], weight: 12},
    {path: ['A', 'B', 'E', 'C'], weight: 12},
    {path: ['A', 'C', 'A', 'B'], weight: 12},
    {path: ['A', 'C', 'D', 'A'], weight: 12},
    {path: ['A', 'C', 'D', 'B'], weight: 12},
    {path: ['A', 'C', 'E', 'A'], weight: 12},
    {path: ['A', 'D', 'A', 'B'], weight: 12},
    {path: ['A', 'D', 'B', 'C'], weight: 12},
    {path: ['A', 'D', 'C', 'A'], weight: 12},
    {path: ['A', 'D', 'C', 'E'], weight: 12},
    {path: ['A', 'D', 'E', 'B'], weight: 12},
    {path: ['A', 'D', 'E', 'C'], weight: 12},
    {path: ['A', 'C', 'A', 'D'], weight: 15},
    {path: ['A', 'C', 'E', 'B'], weight: 15},
    {path: ['A', 'C', 'E', 'C'], weight: 15},
    {path: ['A', 'D', 'A', 'C'], weight: 15},
    {path: ['A', 'D', 'B', 'D'], weight: 15},
    {path: ['A', 'D', 'B', 'E'], weight: 15},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesPathD3 = (testGraph) => testCompleteGraphModeUniqueEdgesPathD3(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesPathD3 = (testGraph) => testCompleteGraphModeUniqueEdgesPathD3(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesPathD3(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "path", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],

    ["A", "B", "A"],
    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],

    ["A", "C", "A"],
    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],

    ["A", "D", "A"],
    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],

    ["A", "E", "A"],
    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"],

    ["A", "B", "A", "C"],
    ["A", "B", "A", "D"],
    ["A", "B", "A", "E"],

    ["A", "C", "A", "B"],
    ["A", "C", "A", "D"],
    ["A", "C", "A", "E"],

    ["A", "D", "A", "B"],
    ["A", "D", "A", "C"],
    ["A", "D", "A", "E"],

    ["A", "E", "A", "B"],
    ["A", "E", "A", "C"],
    ["A", "E", "A", "D"],

    ["A", "B", "C", "A"],
    ["A", "B", "C", "B"],
    ["A", "B", "C", "D"],
    ["A", "B", "C", "E"],

    ["A", "B", "D", "E"],
    ["A", "B", "D", "C"],
    ["A", "B", "D", "A"],
    ["A", "B", "D", "B"],

    ["A", "B", "E", "C"],
    ["A", "B", "E", "D"],
    ["A", "B", "E", "A"],
    ["A", "B", "E", "B"],

    ["A", "C", "B", "D"],
    ["A", "C", "B", "E"],
    ["A", "C", "B", "A"],
    ["A", "C", "B", "C"],

    ["A", "C", "D", "E"],
    ["A", "C", "D", "B"],
    ["A", "C", "D", "A"],
    ["A", "C", "D", "C"],

    ["A", "C", "E", "D"],
    ["A", "C", "E", "B"],
    ["A", "C", "E", "A"],
    ["A", "C", "E", "C"],

    ["A", "D", "B", "A"],
    ["A", "D", "B", "C"],
    ["A", "D", "B", "E"],
    ["A", "D", "B", "D"],

    ["A", "D", "C", "A"],
    ["A", "D", "C", "B"],
    ["A", "D", "C", "D"],
    ["A", "D", "C", "E"],

    ["A", "D", "E", "A"],
    ["A", "D", "E", "B"],
    ["A", "D", "E", "C"],
    ["A", "D", "E", "D"],

    ["A", "E", "B", "A"],
    ["A", "E", "B", "D"],
    ["A", "E", "B", "C"],
    ["A", "E", "B", "E"],

    ["A", "E", "C", "A"],
    ["A", "E", "C", "B"],
    ["A", "E", "C", "D"],
    ["A", "E", "C", "E"],

    ["A", "E", "D", "A"],
    ["A", "E", "D", "B"],
    ["A", "E", "D", "E"],
    ["A", "E", "D", "C"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesNoneD1EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesNoneD1 = (testGraph) => testCompleteGraphModeUniqueEdgesNoneD1(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesNoneD1 = (testGraph) => testCompleteGraphModeUniqueEdgesNoneD1(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesNoneD1(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesNoneD2EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'E', 'D'], weight: 2},
    {path: ['A', 'B', 'A'], weight: 3},
    {path: ['A', 'E', 'A'], weight: 3},
    {path: ['A', 'B', 'C'], weight: 4},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
    {path: ['A', 'C', 'B'], weight: 6},
    {path: ['A', 'E', 'B'], weight: 6},
    {path: ['A', 'E', 'C'], weight: 6},
    {path: ['A', 'B', 'D'], weight: 7},
    {path: ['A', 'B', 'E'], weight: 7},
    {path: ['A', 'C', 'D'], weight: 7},
    {path: ['A', 'D', 'C'], weight: 7},
    {path: ['A', 'D', 'E'], weight: 7},
    {path: ['A', 'C', 'A'], weight: 10},
    {path: ['A', 'C', 'E'], weight: 10},
    {path: ['A', 'D', 'A'], weight: 10},
    {path: ['A', 'D', 'B'], weight: 10},

  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesNoneD2 = (testGraph) => testCompleteGraphModeUniqueEdgesNoneD2(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesNoneD2 = (testGraph) => testCompleteGraphModeUniqueEdgesNoneD2(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesNoneD2(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..2 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],

    ["A", "B", "A"],
    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],

    ["A", "C", "A"],
    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],

    ["A", "D", "A"],
    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],

    ["A", "E", "A"],
    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueVerticesUniqueEdgesPathD3EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", uniqueEdges: "path", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'E', 'D'], weight: 2},
    {path: ['A', 'B', 'C'], weight: 4},
    {path: ['A', 'E', 'D', 'C'], weight: 4},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
    {path: ['A', 'C', 'B'], weight: 6},
    {path: ['A', 'E', 'B'], weight: 6},
    {path: ['A', 'E', 'C'], weight: 6},
    {path: ['A', 'B', 'C', 'D'], weight: 6},
    {path: ['A', 'B', 'D'], weight: 7},
    {path: ['A', 'B', 'E'], weight: 7},
    {path: ['A', 'C', 'D'], weight: 7},
    {path: ['A', 'D', 'C'], weight: 7},
    {path: ['A', 'D', 'E'], weight: 7},
    {path: ['A', 'E', 'C', 'B'], weight: 7},
    {path: ['A', 'E', 'D', 'B'], weight: 7},
    {path: ['A', 'B', 'E', 'D'], weight: 8},
    {path: ['A', 'D', 'C', 'B'], weight: 8},
    {path: ['A', 'E', 'B', 'C'], weight: 8},
    {path: ['A', 'E', 'C', 'D'], weight: 8},
    {path: ['A', 'B', 'C', 'E'], weight: 9},
    {path: ['A', 'B', 'D', 'C'], weight: 9},
    {path: ['A', 'B', 'D', 'E'], weight: 9},
    {path: ['A', 'C', 'D', 'E'], weight: 9},
    {path: ['A', 'C', 'E'], weight: 10},
    {path: ['A', 'D', 'B'], weight: 10},
    {path: ['A', 'C', 'B', 'D'], weight: 11},
    {path: ['A', 'C', 'B', 'E'], weight: 11},
    {path: ['A', 'C', 'E', 'D'], weight: 11},
    {path: ['A', 'E', 'B', 'D'], weight: 11},
    {path: ['A', 'B', 'E', 'C'], weight: 12},
    {path: ['A', 'C', 'D', 'B'], weight: 12},
    {path: ['A', 'D', 'B', 'C'], weight: 12},
    {path: ['A', 'D', 'C', 'E'], weight: 12},
    {path: ['A', 'D', 'E', 'B'], weight: 12},
    {path: ['A', 'D', 'E', 'C'], weight: 12},
    {path: ['A', 'C', 'E', 'B'], weight: 15},
    {path: ['A', 'D', 'B', 'E'], weight: 15},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueVerticesUniqueEdgesPathD3 = (testGraph) => testCompleteGraphModeUniqueVerticesUniqueEdgesPathD3(testGraph, "bfs");
const testCompleteGraphWeightedUniqueVerticesUniqueEdgesPathD3 = (testGraph) => testCompleteGraphModeUniqueVerticesUniqueEdgesPathD3(testGraph, "weighted");

function testCompleteGraphModeUniqueVerticesUniqueEdgesPathD3(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "path", uniqueEdges: "path", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],

    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],

    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],

    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],

    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],

    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"],

    ["A", "B", "C", "D"],
    ["A", "B", "C", "E"],

    ["A", "B", "D", "C"],
    ["A", "B", "D", "E"],

    ["A", "B", "E", "C"],
    ["A", "B", "E", "D"],

    ["A", "C", "B", "D"],
    ["A", "C", "B", "E"],

    ["A", "C", "D", "B"],
    ["A", "C", "D", "E"],

    ["A", "C", "E", "B"],
    ["A", "C", "E", "D"],

    ["A", "D", "B", "C"],
    ["A", "D", "B", "E"],

    ["A", "D", "C", "B"],
    ["A", "D", "C", "E"],

    ["A", "D", "E", "B"],
    ["A", "D", "E", "C"],

    ["A", "E", "B", "C"],
    ["A", "E", "B", "D"],

    ["A", "E", "C", "B"],
    ["A", "E", "C", "D"],

    ["A", "E", "D", "B"],
    ["A", "E", "D", "C"],
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesNoneD3EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueEdges: "none", uniqueVertices: "none", order: "weighted", weightAttribute: ${testGraph.weightAttribute()}}
        RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
      `;

  const expectedPaths = [
    {path: ['A'], weight: 0},
    {path: ['A', 'E'], weight: 1},
    {path: ['A', 'B'], weight: 2},
    {path: ['A', 'E', 'D'], weight: 2},
    {path: ['A', 'B', 'A'], weight: 3},
    {path: ['A', 'E', 'A'], weight: 3},
    {path: ['A', 'B', 'C'], weight: 4},
    {path: ['A', 'B', 'A', 'E'], weight: 4},
    {path: ['A', 'E', 'A', 'E'], weight: 4},
    {path: ['A', 'E', 'D', 'C'], weight: 4},
    {path: ['A', 'E', 'D', 'E'], weight: 4},
    {path: ['A', 'C'], weight: 5},
    {path: ['A', 'D'], weight: 5},
    {path: ['A', 'B', 'A', 'B'], weight: 5},
    {path: ['A', 'B', 'C', 'B'], weight: 5},
    {path: ['A', 'E', 'A', 'B'], weight: 5},
    {path: ['A', 'C', 'B'], weight: 6},
    {path: ['A', 'E', 'B'], weight: 6},
    {path: ['A', 'E', 'C'], weight: 6},
    {path: ['A', 'B', 'C', 'D'], weight: 6},
    {path: ['A', 'B', 'D'], weight: 7},
    {path: ['A', 'B', 'E'], weight: 7},
    {path: ['A', 'C', 'D'], weight: 7},
    {path: ['A', 'D', 'C'], weight: 7},
    {path: ['A', 'D', 'E'], weight: 7},
    {path: ['A', 'C', 'B', 'A'], weight: 7},
    {path: ['A', 'E', 'B', 'A'], weight: 7},
    {path: ['A', 'E', 'C', 'B'], weight: 7},
    {path: ['A', 'E', 'D', 'A'], weight: 7},
    {path: ['A', 'E', 'D', 'B'], weight: 7},
    {path: ['A', 'B', 'A', 'C'], weight: 8},
    {path: ['A', 'B', 'A', 'D'], weight: 8},
    {path: ['A', 'B', 'E', 'D'], weight: 8},
    {path: ['A', 'C', 'B', 'C'], weight: 8},
    {path: ['A', 'D', 'C', 'B'], weight: 8},
    {path: ['A', 'D', 'E', 'D'], weight: 8},
    {path: ['A', 'E', 'A', 'C'], weight: 8},
    {path: ['A', 'E', 'A', 'D'], weight: 8},
    {path: ['A', 'E', 'B', 'C'], weight: 8},
    {path: ['A', 'E', 'C', 'D'], weight: 8},
    {path: ['A', 'B', 'C', 'A'], weight: 9},
    {path: ['A', 'B', 'C', 'E'], weight: 9},
    {path: ['A', 'B', 'D', 'C'], weight: 9},
    {path: ['A', 'B', 'D', 'E'], weight: 9},
    {path: ['A', 'B', 'E', 'A'], weight: 9},
    {path: ['A', 'C', 'D', 'C'], weight: 9},
    {path: ['A', 'C', 'D', 'E'], weight: 9},
    {path: ['A', 'D', 'C', 'D'], weight: 9},
    {path: ['A', 'D', 'E', 'A'], weight: 9},
    {path: ['A', 'C', 'A'], weight: 10},
    {path: ['A', 'C', 'E'], weight: 10},
    {path: ['A', 'D', 'A'], weight: 10},
    {path: ['A', 'D', 'B'], weight: 10},
    {path: ['A', 'C', 'A', 'E'], weight: 11},
    {path: ['A', 'C', 'B', 'D'], weight: 11},
    {path: ['A', 'C', 'B', 'E'], weight: 11},
    {path: ['A', 'C', 'E', 'D'], weight: 11},
    {path: ['A', 'D', 'A', 'E'], weight: 11},
    {path: ['A', 'D', 'B', 'A'], weight: 11},
    {path: ['A', 'E', 'B', 'D'], weight: 11},
    {path: ['A', 'E', 'B', 'E'], weight: 11},
    {path: ['A', 'E', 'C', 'A'], weight: 11},
    {path: ['A', 'E', 'C', 'E'], weight: 11},
    {path: ['A', 'B', 'D', 'A'], weight: 12},
    {path: ['A', 'B', 'D', 'B'], weight: 12},
    {path: ['A', 'B', 'E', 'B'], weight: 12},
    {path: ['A', 'B', 'E', 'C'], weight: 12},
    {path: ['A', 'C', 'A', 'B'], weight: 12},
    {path: ['A', 'C', 'D', 'A'], weight: 12},
    {path: ['A', 'C', 'D', 'B'], weight: 12},
    {path: ['A', 'C', 'E', 'A'], weight: 12},
    {path: ['A', 'D', 'A', 'B'], weight: 12},
    {path: ['A', 'D', 'B', 'C'], weight: 12},
    {path: ['A', 'D', 'C', 'A'], weight: 12},
    {path: ['A', 'D', 'C', 'E'], weight: 12},
    {path: ['A', 'D', 'E', 'B'], weight: 12},
    {path: ['A', 'D', 'E', 'C'], weight: 12},
    {path: ['A', 'C', 'A', 'C'], weight: 15},
    {path: ['A', 'C', 'A', 'D'], weight: 15},
    {path: ['A', 'C', 'E', 'B'], weight: 15},
    {path: ['A', 'C', 'E', 'C'], weight: 15},
    {path: ['A', 'D', 'A', 'C'], weight: 15},
    {path: ['A', 'D', 'A', 'D'], weight: 15},
    {path: ['A', 'D', 'B', 'D'], weight: 15},
    {path: ['A', 'D', 'B', 'E'], weight: 15},
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testCompleteGraphBfsUniqueVerticesUniqueEdgesNoneD3 = (testGraph) => testCompleteGraphModeUniqueVerticesUniqueEdgesNoneD3(testGraph, "bfs");
const testCompleteGraphWeightedUniqueVerticesUniqueEdgesNoneD3 = (testGraph) => testCompleteGraphModeUniqueVerticesUniqueEdgesNoneD3(testGraph, "weighted");

function testCompleteGraphModeUniqueVerticesUniqueEdgesNoneD3(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
        OPTIONS {uniqueVertices: "none", uniqueEdges: "none", order: ${mode}}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPaths = [
    ["A"],

    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["A", "E"],

    ["A", "B", "A"],
    ["A", "B", "C"],
    ["A", "B", "D"],
    ["A", "B", "E"],

    ["A", "C", "A"],
    ["A", "C", "B"],
    ["A", "C", "D"],
    ["A", "C", "E"],

    ["A", "D", "A"],
    ["A", "D", "B"],
    ["A", "D", "C"],
    ["A", "D", "E"],

    ["A", "E", "A"],
    ["A", "E", "B"],
    ["A", "E", "C"],
    ["A", "E", "D"],

    ["A", "B", "A", "B"],
    ["A", "B", "A", "C"],
    ["A", "B", "A", "D"],
    ["A", "B", "A", "E"],

    ["A", "C", "A", "B"],
    ["A", "C", "A", "C"],
    ["A", "C", "A", "D"],
    ["A", "C", "A", "E"],

    ["A", "D", "A", "B"],
    ["A", "D", "A", "C"],
    ["A", "D", "A", "D"],
    ["A", "D", "A", "E"],

    ["A", "E", "A", "B"],
    ["A", "E", "A", "C"],
    ["A", "E", "A", "D"],
    ["A", "E", "A", "E"],

    ["A", "B", "C", "A"],
    ["A", "B", "C", "B"],
    ["A", "B", "C", "D"],
    ["A", "B", "C", "E"],

    ["A", "B", "D", "A"],
    ["A", "B", "D", "B"],
    ["A", "B", "D", "C"],
    ["A", "B", "D", "E"],

    ["A", "B", "E", "A"],
    ["A", "B", "E", "B"],
    ["A", "B", "E", "C"],
    ["A", "B", "E", "D"],

    ["A", "C", "B", "A"],
    ["A", "C", "B", "C"],
    ["A", "C", "B", "D"],
    ["A", "C", "B", "E"],

    ["A", "C", "D", "A"],
    ["A", "C", "D", "B"],
    ["A", "C", "D", "C"],
    ["A", "C", "D", "E"],

    ["A", "C", "E", "A"],
    ["A", "C", "E", "B"],
    ["A", "C", "E", "C"],
    ["A", "C", "E", "D"],

    ["A", "D", "B", "A"],
    ["A", "D", "B", "C"],
    ["A", "D", "B", "E"],
    ["A", "D", "B", "D"],

    ["A", "D", "C", "A"],
    ["A", "D", "C", "B"],
    ["A", "D", "C", "D"],
    ["A", "D", "C", "E"],

    ["A", "D", "E", "A"],
    ["A", "D", "E", "B"],
    ["A", "D", "E", "C"],
    ["A", "D", "E", "D"],

    ["A", "E", "B", "A"],
    ["A", "E", "B", "C"],
    ["A", "E", "B", "D"],
    ["A", "E", "B", "E"],

    ["A", "E", "C", "A"],
    ["A", "E", "C", "B"],
    ["A", "E", "C", "D"],
    ["A", "E", "C", "E"],

    ["A", "E", "D", "A"],
    ["A", "E", "D", "B"],
    ["A", "E", "D", "C"],
    ["A", "E", "D", "E"]
  ];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD1EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 1..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueEdges: "path", uniqueVertices: "global", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedVertices = ["B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalWsOf(expectedVertices, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesPathUniqueVerticesGlobalD1 = (testGraph) => testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD1(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD1 = (testGraph) => testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD1(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD1(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 1..1 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: ${mode}, uniqueEdges: "path", uniqueVertices: "global"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedVertices = ["B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD3EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueEdges: "path", uniqueVertices: "global", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedVertices = ["A", "B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalWsOf(expectedVertices, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesPathUniqueVerticesGlobalD3 = (testGraph) => testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD3(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD3 = (testGraph) => testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD3(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD3(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()}
    OPTIONS {order: ${mode}, uniqueEdges: "path", uniqueVertices: "global"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedVertices = ["A", "B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesNoneUniqueVerticesGlobalD3 = (testGraph) => testCompleteGraphModeUniqueEdgesNoneUniqueVerticesGlobalD3(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesNoneUniqueVerticesGlobalD3 = (testGraph) => testCompleteGraphModeUniqueEdgesNoneUniqueVerticesGlobalD3(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesNoneUniqueVerticesGlobalD3(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 0..3 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {order: ${mode}, uniqueEdges: "none", uniqueVertices: "global"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedVertices = ["A", "B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD10EnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueEdges: "path", uniqueVertices: "global", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedVertices = ["A", "B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalWsOf(expectedVertices, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesPathUniqueVerticesGlobalD10 = (testGraph) => testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD10(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD10 = (testGraph) => testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD10(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesPathUniqueVerticesGlobalD10(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()}
    OPTIONS {order: ${mode}, uniqueEdges: "path", uniqueVertices: "global"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedVertices = ["A", "B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testCompleteGraphWeightedUniqueEdgesNoneUniqueVerticesGlobalD10EnabledWeights(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueEdges: "none", uniqueVertices: "global", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedVertices = ["A", "B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

const testCompleteGraphBfsUniqueEdgesNoneUniqueVerticesGlobalD10 = (testGraph) => testCompleteGraphModeUniqueEdgesNoneUniqueVerticesGlobalD10(testGraph, "bfs");
const testCompleteGraphWeightedUniqueEdgesNoneUniqueVerticesGlobalD10 = (testGraph) => testCompleteGraphModeUniqueEdgesNoneUniqueVerticesGlobalD10(testGraph, "weighted");

function testCompleteGraphModeUniqueEdgesNoneUniqueVerticesGlobalD10(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: ${mode}, uniqueEdges: "none", uniqueVertices: "global"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedVertices = ["A", "B", "C", "D", "E"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testCompleteGraphShortestPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('C')}  
        GRAPH ${testGraph.name()} 
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "C"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

function testCompleteGraphKPathsD1(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR path IN 1..1 OUTBOUND K_PATHS ${testGraph.vertex('A')} TO ${testGraph.vertex('C')}
        GRAPH ${testGraph.name()}
        RETURN path.vertices[* RETURN CURRENT.key]
      `;

  const necessaryPaths = [
    ["A", "C"]
  ];

  const res = db._query(query);
  const foundPaths = res.toArray();

  assertResIsEqualInPathList(necessaryPaths, foundPaths);
}

function testCompleteGraphKPathsD2(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR path IN 1..2 OUTBOUND K_PATHS ${testGraph.vertex('A')} TO ${testGraph.vertex('C')}
        GRAPH ${testGraph.name()}
        RETURN path.vertices[* RETURN CURRENT.key]
      `;

  const necessaryPaths = [
    ["A", "C"],
    ["A", "B", "C"],
    ["A", "E", "C"],
    ["A", "D", "C"]
  ];

  const res = db._query(query);
  const foundPaths = res.toArray();

  assertResIsEqualInPathList(necessaryPaths, foundPaths);
}

function testCompleteGraphKPathsD3(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR path IN 1..3 OUTBOUND K_PATHS ${testGraph.vertex('A')} TO ${testGraph.vertex('C')}
        GRAPH ${testGraph.name()}
        RETURN path.vertices[* RETURN CURRENT.key]
      `;

  const necessaryPaths = [
    ["A", "C"],

    ["A", "B", "C"],
    ["A", "B", "A", "C"],
    ["A", "B", "B", "C"],
    ["A", "B", "E", "C"],
    ["A", "B", "D", "C"],

    ["A", "E", "C"],
    ["A", "E", "A", "C"],
    ["A", "E", "B", "C"],
    ["A", "E", "D", "C"],
    ["A", "E", "E", "C"],

    ["A", "D", "C"],
    ["A", "D", "A", "C"],
    ["A", "D", "B", "C"],
    ["A", "D", "D", "C"],
    ["A", "D", "E", "C"]
  ];

  const res = db._query(query);
  const foundPaths = res.toArray();

  assertResIsEqualInPathList(necessaryPaths, foundPaths);
}

const testCompleteGraphKShortestPathLimit1 = (testGraph) => testCompleteGraphKShortestPathLimit1Gen(testGraph, generateKShortestPathQuery);
const testCompleteGraphKShortestPathLimit1WT = (testGraph) => testCompleteGraphKShortestPathLimit1Gen(testGraph, generateKShortestPathQueryWT);

function testCompleteGraphKShortestPathLimit1Gen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const limit = 1;
  const query = generator(testGraph, 'A', 'C', limit);

  const allowedPaths = [
    ["A", "C"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  checkResIsValidKShortestPathListNoWeights(allowedPaths, actualPath, limit);
}

const testCompleteGraphKShortestPathLimit3 = (testGraph) => testCompleteGraphKShortestPathLimit3Gen(testGraph, generateKShortestPathQuery);
const testCompleteGraphKShortestPathLimit3WT = (testGraph) => testCompleteGraphKShortestPathLimit3Gen(testGraph, generateKShortestPathQueryWT);

function testCompleteGraphKShortestPathLimit3Gen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const limit = 3;
  const query = generator(testGraph, 'A', 'C', limit);

  const allowedPaths = [
    ["A", "C"], ["A", "B", "C"], ["A", "D", "C"], ["A", "E", "C"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  checkResIsValidKShortestPathListNoWeights(allowedPaths, actualPath, limit);
}

function testCompleteGraphShortestPathEnabledWeightCheck(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('C')}  
        GRAPH ${testGraph.name()} 
        OPTIONS {weightAttribute: ${testGraph.weightAttribute()}}
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "B", "C"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

const testCompleteGraphKShortestPathEnabledWeightCheckMultiLimit = (testGraph) => testCompleteGraphKShortestPathEnabledWeightCheckMultiLimitGen(testGraph, generateKShortestPathQueryWithWeights);
const testCompleteGraphKShortestPathEnabledWeightCheckMultiLimitWT = (testGraph) => testCompleteGraphKShortestPathEnabledWeightCheckMultiLimitGen(testGraph, generateKShortestPathQueryWithWeightsWT);

function testCompleteGraphKShortestPathEnabledWeightCheckMultiLimitGen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.completeGraph.name()));
  const limits = [1, 2, 3];

  _.each(limits, (limit) => {
    const query = generator(testGraph, 'A', 'C', limit);

    const allowedPaths = [
      {vertices: ["A", "B", "C"], weight: 4},
      {vertices: ["A", "E", "D", "C"], weight: 4},
      { "vertices" : [ "A", "C" ], "weight" : 5 }
    ];

    const res = db._query(query);
    const actualPaths = res.toArray();

    checkResIsValidKShortestPathListWeights(allowedPaths, actualPaths, limit);
  });


}

function getExpectedBinTree() {
  const leftChild = vi => 2 * vi + 1;
  const rightChild = vi => 2 * vi + 2;
  const buildBinTree = (vi, depth) => new Node(`v${vi}`,
    depth === 0 ? []
      : [buildBinTree(leftChild(vi), depth - 1),
        buildBinTree(rightChild(vi), depth - 1)]);
  if (typeof getExpectedBinTree.expectedBinTree === 'undefined') {
    // build tree only once
    getExpectedBinTree.expectedBinTree = buildBinTree(0, 8);
  }
  return getExpectedBinTree.expectedBinTree;
}

function treeToPaths(tree, stack = []) {
  const curStack = stack.concat([tree.vertex]);
  return [curStack].concat(...tree.children.map(node => treeToPaths(node, curStack)));
}

function getExpectedBinTreePaths() {
  if (typeof getExpectedBinTreePaths.expectedPaths === 'undefined') {
    // build paths only once
    getExpectedBinTreePaths.expectedPaths = treeToPaths(getExpectedBinTree());
  }
  return getExpectedBinTreePaths.expectedPaths;
}

function testLargeBinTreeAllCombinations(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.largeBinTree.name()));
  const expectedPathsAsTree = getExpectedBinTree();
  const expectedPaths = getExpectedBinTreePaths();

  const optionsList = [
    {order: "dfs", uniqueEdges: "none", uniqueVertices: "none"},
    {order: "dfs", uniqueEdges: "path", uniqueVertices: "none"},
    {order: "dfs", uniqueEdges: "none", uniqueVertices: "path"},
    {order: "dfs", uniqueEdges: "path", uniqueVertices: "path"}, // same as above
    {order: "bfs", uniqueEdges: "none", uniqueVertices: "none"},
    {order: "bfs", uniqueEdges: "path", uniqueVertices: "none"},
    {order: "bfs", uniqueEdges: "none", uniqueVertices: "path"},
    {order: "bfs", uniqueEdges: "path", uniqueVertices: "path"}, // same as above
    {order: "bfs", uniqueEdges: "none", uniqueVertices: "global"},
    {order: "bfs", uniqueEdges: "path", uniqueVertices: "global"}, // same as above
    {order: "weighted", uniqueEdges: "path", uniqueVertices: "path"}, // same as above
    {order: "weighted", uniqueEdges: "none", uniqueVertices: "none"},
    {order: "weighted", uniqueEdges: "path", uniqueVertices: "none"},
    {order: "weighted", uniqueEdges: "none", uniqueVertices: "path"},
    {order: "weighted", uniqueEdges: "none", uniqueVertices: "global"},
    {order: "weighted", uniqueEdges: "path", uniqueVertices: "global"}, // same as above
  ];

  for (const options of optionsList) {
    const query = `
      FOR v, e, p IN 0..10 OUTBOUND @start GRAPH @graph OPTIONS ${JSON.stringify(options)}
      RETURN p.vertices[* RETURN CURRENT.key]
    `;
    const res = db._query(query, {start: testGraph.vertex('v0'), graph: testGraph.name()});
    const actualPaths = res.toArray();

    try {
      if (options.order === "dfs") {
        checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
      } else {
        checkResIsValidBfsOf(expectedPaths, actualPaths);
      }
    } catch (e) {
      // Note that we cannot prepend our message, otherwise the assertion
      if (e.hasOwnProperty('message')) {
        e.message = `${e.message}\nwhile trying the following options: ${JSON.stringify(options)}`;
        throw e;
      } else if (typeof e === 'string') {
        throw `${e}\nwhile trying the following options: ${JSON.stringify(options)}`;
      }
      throw e;
    }
  }
}

function testMetaTreeToPaths() {
  let paths;
  let tree;

  paths = [["a"]];
  tree = new Node("a");
  assertEqual(_.sortBy(paths), _.sortBy(treeToPaths(tree)));

  paths = [["a"], ["a", "b"], ["a", "b", "c"]];
  tree = new Node("a", [new Node("b", [new Node("c")])]);
  assertEqual(_.sortBy(paths), _.sortBy(treeToPaths(tree)));

  paths = [["a"], ["a", "b"], ["a", "c"]];
  tree = new Node("a", [new Node("b"), new Node("c")]);
  assertEqual(_.sortBy(paths), _.sortBy(treeToPaths(tree)));

  paths = [
    ["a"], ["a", "b"], ["a", "c"], ["a", "c", "d"], ["a", "b", "d"],
    ["a", "b", "d", "e"], ["a", "b", "d", "f"],
    ["a", "c", "d", "e"], ["a", "c", "d", "f"],
  ];
  tree =
    new Node("a", [
      new Node("b", [
        new Node("d", [
          new Node("e"),
          new Node("f"),
        ]),
      ]),
      new Node("c", [
        new Node("d", [
          new Node("e"),
          new Node("f"),
        ]),
      ]),
    ]);
  assertEqual(_.sortBy(paths), _.sortBy(treeToPaths(tree)));
}

const easyPathAsTree = new Node("A", [
  new Node("B", [
    new Node("C", [
      new Node("D", [
        new Node("E", [
          new Node("F", [
            new Node("G", [
              new Node("H", [
                new Node("I", [
                  new Node("J", [])
                ])
              ])
            ])
          ])
        ])
      ])
    ])
  ])
]);

function testEasyPathAllCombinations(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.easyPath.name()));
  const expectedPathsAsTree = easyPathAsTree;
  const expectedPaths = treeToPaths(easyPathAsTree);

  const optionsList = [
    {order: "dfs", uniqueEdges: "none", uniqueVertices: "none"},
    {order: "dfs", uniqueEdges: "path", uniqueVertices: "none"},
    {order: "dfs", uniqueEdges: "none", uniqueVertices: "path"},
    {order: "dfs", uniqueEdges: "path", uniqueVertices: "path"}, // same as above
    {order: "bfs", uniqueEdges: "none", uniqueVertices: "none"},
    {order: "bfs", uniqueEdges: "path", uniqueVertices: "none"},
    {order: "bfs", uniqueEdges: "none", uniqueVertices: "path"},
    {order: "bfs", uniqueEdges: "path", uniqueVertices: "path"}, // same as above
    {order: "bfs", uniqueEdges: "none", uniqueVertices: "global"},
    {order: "bfs", uniqueEdges: "path", uniqueVertices: "global"}, // same as above
    {order: "weighted", uniqueEdges: "none", uniqueVertices: "none"},
    {order: "weighted", uniqueEdges: "path", uniqueVertices: "none"},
    {order: "weighted", uniqueEdges: "none", uniqueVertices: "path"},
    {order: "weighted", uniqueEdges: "path", uniqueVertices: "path"}, // same as above
    {order: "weighted", uniqueEdges: "none", uniqueVertices: "global"},
    {order: "weighted", uniqueEdges: "path", uniqueVertices: "global"}, // same as above
  ];

  for (const options of optionsList) {
    const query = `
      FOR v, e, p IN 0..10 OUTBOUND @start GRAPH @graph OPTIONS ${JSON.stringify(options)}
      RETURN p.vertices[* RETURN CURRENT.key]
    `;
    const res = db._query(query, {start: testGraph.vertex('A'), graph: testGraph.name()});
    const actualPaths = res.toArray();

    try {
      if (options.bfs) {
        // Note that we use this here even for uniqueVertices: global, as for
        // this graph, there is no ambiguity.
        checkResIsValidBfsOf(expectedPaths, actualPaths);
      } else {
        checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
      }
    } catch (e) {
      // Note that we cannot prepend our message, otherwise the assertion
      if (e.hasOwnProperty('message')) {
        e.message = `${e.message}\nwhile trying the following options: ${JSON.stringify(options)}`;
        throw e;
      } else if (typeof e === 'string') {
        throw `${e}\nwhile trying the following options: ${JSON.stringify(options)}`;
      }
      throw e;
    }
  }
}

function testEasyPathShortestPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.easyPath.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('J')}  
        GRAPH ${testGraph.name()} 
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

function testEasyPathKPaths(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.easyPath.name()));
  const query = aql`
        FOR path IN 1..9 OUTBOUND K_PATHS ${testGraph.vertex('A')} TO ${testGraph.vertex('J')}
        GRAPH ${testGraph.name()}
        RETURN path.vertices[* RETURN CURRENT.key]
      `;

  const necessaryPaths = [
    ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J"]
  ];

  const res = db._query(query);
  const foundPaths = res.toArray();

  assertResIsEqualInPathList(necessaryPaths, foundPaths);
}

function testEasyPathShortestPathEnabledWeightCheck(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.easyPath.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('J')}  
        GRAPH ${testGraph.name()} 
        OPTIONS {weightAttribute: ${testGraph.weightAttribute()}}
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

const testEasyPathKShortestPathMultipleLimits = (testGraph) => testEasyPathKShortestPathMultipleLimitsGen(testGraph, generateKShortestPathQuery);
const testEasyPathKShortestPathMultipleLimitsWT = (testGraph) => testEasyPathKShortestPathMultipleLimitsGen(testGraph, generateKShortestPathQueryWT);

function testEasyPathKShortestPathMultipleLimitsGen(testGraph, generate) {
  assertTrue(testGraph.name().startsWith(protoGraphs.easyPath.name()));
  const limits = [1, 2, 3, 4];

  _.each(limits, function (limit) {
    const query = generate(testGraph, 'A', 'J', limit);

    const allowedPaths = [
      ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J"]
    ];

    const res = db._query(query);
    const actualPaths = res.toArray();

    checkResIsValidKShortestPathListNoWeights(allowedPaths, actualPaths, limit);
  });
}

const testEasyPathKShortestPathEnabledWeightCheckMultipleLimits = (testGraph) => testEasyPathKShortestPathEnabledWeightCheckMultipleLimitsGen(testGraph, generateKShortestPathQueryWithWeights);
const testEasyPathKShortestPathEnabledWeightCheckMultipleLimitsWT = (testGraph) => testEasyPathKShortestPathEnabledWeightCheckMultipleLimitsGen(testGraph, generateKShortestPathQueryWithWeightsWT);

function testEasyPathKShortestPathEnabledWeightCheckMultipleLimitsGen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.easyPath.name()));
  const limits = [1, 2, 3, 4];

  _.each(limits, function (limit) {
    const query = generator(testGraph, 'A', 'J', limit);

    const allowedPaths = [
      { vertices: ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J"], weight: 45}
    ];

    const res = db._query(query);
    const actualPaths = res.toArray();

    checkResIsValidKShortestPathListWeights(allowedPaths, actualPaths, limit);
  });
}

function testAdvancedPathDfsUniqueVerticesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node("E", [
              new Node("F", [
                new Node("G", [
                  new Node("H", [
                    new Node("I", [])
                  ])
                ])
              ]),
              new Node("H", [
                new Node("I")
              ])
            ])
          ])
        ])
      ]),
      new Node("D", [
        new Node("E", [
          new Node("F", [
            new Node("G", [
              new Node("H", [
                new Node("I", [])
              ])
            ])
          ]),
          new Node("H", [
            new Node("I")
          ])
        ])
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testAdvancedPathDfsUniqueVerticesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueVertices: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node("E", [
              new Node("F", [
                new Node("G", [
                  new Node("H", [
                    new Node("I", [])
                  ])
                ])
              ]),
              new Node("H", [
                new Node("I")
              ])
            ])
          ])
        ])
      ]),
      new Node("D", [
        new Node("E", [
          new Node("F", [
            new Node("G", [
              new Node("H", [
                new Node("I", [])
              ])
            ])
          ]),
          new Node("H", [
            new Node("I")
          ])
        ])
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testAdvancedPathDfsUniqueEdgesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node("E", [
              new Node("F", [
                new Node("G", [
                  new Node("H", [
                    new Node("I", [])
                  ])
                ])
              ]),
              new Node("H", [
                new Node("I")
              ])
            ])
          ])
        ])
      ]),
      new Node("D", [
        new Node("E", [
          new Node("F", [
            new Node("G", [
              new Node("H", [
                new Node("I", [])
              ])
            ])
          ]),
          new Node("H", [
            new Node("I")
          ])
        ])
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testAdvancedPathDfsUniqueEdgesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node("E", [
              new Node("F", [
                new Node("G", [
                  new Node("H", [
                    new Node("I", [])
                  ])
                ])
              ]),
              new Node("H", [
                new Node("I")
              ])
            ])
          ])
        ])
      ]),
      new Node("D", [
        new Node("E", [
          new Node("F", [
            new Node("G", [
              new Node("H", [
                new Node("I", [])
              ])
            ])
          ]),
          new Node("H", [
            new Node("I")
          ])
        ])
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testAdvancedPathDfsUniqueEdgesUniqueVerticesPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "path", uniqueVertices: "path"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node("E", [
              new Node("F", [
                new Node("G", [
                  new Node("H", [
                    new Node("I", [])
                  ])
                ])
              ]),
              new Node("H", [
                new Node("I")
              ])
            ])
          ])
        ])
      ]),
      new Node("D", [
        new Node("E", [
          new Node("F", [
            new Node("G", [
              new Node("H", [
                new Node("I", [])
              ])
            ])
          ]),
          new Node("H", [
            new Node("I")
          ])
        ])
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

function testAdvancedPathDfsUniqueEdgesUniqueVerticesNone(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {uniqueEdges: "none", uniqueVertices: "none"}
        RETURN p.vertices[* RETURN CURRENT.key]
      `;

  const expectedPathsAsTree =
    new Node("A", [
      new Node("B", [
        new Node("C", [
          new Node("D", [
            new Node("E", [
              new Node("F", [
                new Node("G", [
                  new Node("H", [
                    new Node("I", [])
                  ])
                ])
              ]),
              new Node("H", [
                new Node("I")
              ])
            ])
          ])
        ])
      ]),
      new Node("D", [
        new Node("E", [
          new Node("F", [
            new Node("G", [
              new Node("H", [
                new Node("I", [])
              ])
            ])
          ]),
          new Node("H", [
            new Node("I")
          ])
        ])
      ]),
    ]);

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidDfsOf(expectedPathsAsTree, actualPaths);
}

const advancedPathBfsPaths = [
  ["A"],
  ["A", "B"],
  ["A", "B", "C"],
  ["A", "B", "C", "D"],
  ["A", "B", "C", "D", "E"],
  ["A", "B", "C", "D", "E", "F"],
  ["A", "B", "C", "D", "E", "F", "G"],
  ["A", "B", "C", "D", "E", "F", "G", "H"],
  ["A", "B", "C", "D", "E", "F", "G", "H", "I"],
  ["A", "B", "C", "D", "E", "H"],
  ["A", "B", "C", "D", "E", "H", "I"],
  ["A", "D"],
  ["A", "D", "E"],
  ["A", "D", "E", "F"],
  ["A", "D", "E", "F", "G"],
  ["A", "D", "E", "F", "G", "H"],
  ["A", "D", "E", "F", "G", "H", "I"],
  ["A", "D", "E", "H"],
  ["A", "D", "E", "H", "I"],
];

const advancedPathWeightedPaths = [
  {path: ['A'], weight: 0},
  {path: ['A', 'B'], weight: 1},
  {path: ['A', 'B', 'C'], weight: 2},
  {path: ['A', 'B', 'C', 'D'], weight: 3},
  {path: ['A', 'B', 'C', 'D', 'E'], weight: 4},
  {path: ['A', 'B', 'C', 'D', 'E', 'F'], weight: 5},
  {path: ['A', 'B', 'C', 'D', 'E', 'F', 'G'], weight: 6},
  {path: ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'], weight: 7},
  {path: ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I'], weight: 8},
  {path: ['A', 'D'], weight: 10},
  {path: ['A', 'D', 'E'], weight: 11},
  {path: ['A', 'D', 'E', 'F'], weight: 12},
  {path: ['A', 'D', 'E', 'F', 'G'], weight: 13},
  {path: ['A', 'B', 'C', 'D', 'E', 'H'], weight: 14},
  {path: ['A', 'D', 'E', 'F', 'G', 'H'], weight: 14},
  {path: ['A', 'B', 'C', 'D', 'E', 'H', 'I'], weight: 15},
  {path: ['A', 'D', 'E', 'F', 'G', 'H', 'I'], weight: 15},
  {path: ['A', 'D', 'E', 'H'], weight: 21},
  {path: ['A', 'D', 'E', 'H', 'I'], weight: 22},
];

function testAdvancedPathWeightedUniqueVerticesPathEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex("A")} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueVertices: "path", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedPaths = advancedPathWeightedPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testAdvancedPathBfsUniqueVerticesPath = (testGraph) => testAdvancedPathModeUniqueVerticesPath(testGraph, "bfs");
const testAdvancedPathWeightedUniqueVerticesPath = (testGraph) => testAdvancedPathModeUniqueVerticesPath(testGraph, "weighted");

function testAdvancedPathModeUniqueVerticesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex("A")} GRAPH ${testGraph.name()} OPTIONS {order: ${mode}, uniqueVertices: "path"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedPaths = advancedPathBfsPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testAdvancedPathWeightedUniqueVerticesNoneEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex("A")} GRAPH ${testGraph.name()}
    OPTIONS {order: "weighted", uniqueVertices: "none", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedPaths = advancedPathWeightedPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testAdvancedPathBfsUniqueVerticesNone = (testGraph) => testAdvancedPathModeUniqueVerticesNone(testGraph, "bfs");
const testAdvancedPathWeightedUniqueVerticesNone = (testGraph) => testAdvancedPathModeUniqueVerticesNone(testGraph, "weighted");

function testAdvancedPathModeUniqueVerticesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: ${mode}, uniqueVertices: "none"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedPaths = advancedPathBfsPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testAdvancedPathWeightedUniqueEdgesPathEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueEdges: "path", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedPaths = advancedPathWeightedPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testAdvancedPathBfsUniqueEdgesPath = (testGraph) => testAdvancedPathModeUniqueEdgesPath(testGraph, "bfs");
const testAdvancedPathWeightedUniqueEdgesPath = (testGraph) => testAdvancedPathModeUniqueEdgesPath(testGraph, "weighted");

function testAdvancedPathModeUniqueEdgesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {order: ${mode}, uniqueEdges: "path"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedPaths = advancedPathBfsPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testAdvancedPathWeightedUniqueEdgesNoneEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()}
    OPTIONS {order: "weighted", uniqueEdges: "none", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedPaths = advancedPathWeightedPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testAdvancedPathBfsUniqueEdgesNone = (testGraph) => testAdvancedPathModeUniqueEdgesNone(testGraph, "bfs");
const testAdvancedPathWeightedUniqueEdgesNone = (testGraph) => testAdvancedPathModeUniqueEdgesNone(testGraph, "weighted");

function testAdvancedPathModeUniqueEdgesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} OPTIONS {order: ${mode}, uniqueEdges: "none"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedPaths = advancedPathBfsPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testAdvancedPathWeightedUniqueEdgesUniqueVerticesPathEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueEdges: "path", uniqueVertices: "path", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedPaths = advancedPathWeightedPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testAdvancedPathBfsUniqueEdgesUniqueVerticesPath = (testGraph) => testAdvancedPathModeUniqueEdgesUniqueVerticesPath(testGraph, "bfs");
const testAdvancedPathWeightedUniqueEdgesUniqueVerticesPath = (testGraph) => testAdvancedPathModeUniqueEdgesUniqueVerticesPath(testGraph, "weighted");

function testAdvancedPathModeUniqueEdgesUniqueVerticesPath(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: ${mode}, uniqueEdges: "path", uniqueVertices: "path"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedPaths = advancedPathBfsPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testAdvancedPathWeightedUniqueEdgesUniqueVerticesNoneEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()}
    OPTIONS {order: "weighted", uniqueEdges: "none", uniqueVertices: "none", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedPaths = advancedPathWeightedPaths;

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidWsOf(expectedPaths, actualPaths);
}

const testAdvancedPathBfsUniqueEdgesUniqueVerticesNone = (testGraph) => testAdvancedPathModeUniqueEdgesUniqueVerticesNone(testGraph, "bfs");
const testAdvancedPathWeightedUniqueEdgesUniqueVerticesNone = (testGraph) => testAdvancedPathModeUniqueEdgesUniqueVerticesNone(testGraph, "weighted");

function testAdvancedPathModeUniqueEdgesUniqueVerticesNone(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()}
    OPTIONS {order: ${mode}, uniqueEdges: "none", uniqueVertices: "none"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedPaths = advancedPathBfsPaths;
  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidBfsOf(expectedPaths, actualPaths);
}

function testAdvancedPathWeightedUniqueEdgesUniquePathVerticesGlobalEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueEdges: "path", uniqueVertices: "global", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedVertices = ["A", "B", "D", "C", "E", "F", "H", "G", "I"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalWsOf(expectedVertices, actualPaths);
}

const testAdvancedPathBfsUniqueEdgesUniquePathVerticesGlobal = (testGraph) => testAdvancedPathModeUniqueEdgesUniquePathVerticesGlobal(testGraph, "bfs");
const testAdvancedPathWeightedUniqueEdgesUniquePathVerticesGlobal = (testGraph) => testAdvancedPathModeUniqueEdgesUniquePathVerticesGlobal(testGraph, "weighted");

function testAdvancedPathModeUniqueEdgesUniquePathVerticesGlobal(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: ${mode}, uniqueEdges: "path", uniqueVertices: "global"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedVertices = ["A", "B", "D", "C", "E", "F", "H", "G", "I"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testAdvancedPathWeightedUniqueEdgesUniqueNoneVerticesGlobalEnabledWeights(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: "weighted", uniqueEdges: "none", uniqueVertices: "global", weightAttribute: ${testGraph.weightAttribute()}}
    RETURN {path: p.vertices[*].key, weight: p.weights[-1]}
  `;

  const expectedVertices = ["A", "B", "D", "C", "E", "F", "H", "G", "I"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalWsOf(expectedVertices, actualPaths);
}

const testAdvancedPathBfsUniqueEdgesUniqueNoneVerticesGlobal = (testGraph) => testAdvancedPathModeUniqueEdgesUniqueNoneVerticesGlobal(testGraph, "bfs");
const testAdvancedPathWeightedUniqueEdgesUniqueNoneVerticesGlobal = (testGraph) => testAdvancedPathModeUniqueEdgesUniqueNoneVerticesGlobal(testGraph, "weighted");

function testAdvancedPathModeUniqueEdgesUniqueNoneVerticesGlobal(testGraph, mode) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
    FOR v, e, p IN 0..10 OUTBOUND ${testGraph.vertex('A')} GRAPH ${testGraph.name()} 
    OPTIONS {order: ${mode}, uniqueEdges: "none", uniqueVertices: "global"}
    RETURN p.vertices[* RETURN CURRENT.key]
  `;

  const expectedVertices = ["A", "B", "D", "C", "E", "F", "H", "G", "I"];

  const res = db._query(query);
  const actualPaths = res.toArray();

  checkResIsValidGlobalBfsOf(expectedVertices, actualPaths);
}

function testAdvancedPathShortestPath(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('I')}  
        GRAPH ${testGraph.name()} 
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "D", "E", "H", "I"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

function testAdvancedPathKPaths(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR path IN 1..8 OUTBOUND K_PATHS ${testGraph.vertex('A')} TO ${testGraph.vertex('I')}
        GRAPH ${testGraph.name()}
        RETURN path.vertices[* RETURN CURRENT.key]
      `;

  const necessaryPaths = [
    ["A", "D", "E", "H", "I"],
    ["A", "D", "E", "F", "G", "H", "I"],
    ["A", "B", "C", "D", "E", "H", "I"],
    ["A", "B", "C", "D", "E", "F", "G", "H", "I"],
  ];

  const res = db._query(query);
  const foundPaths = res.toArray();

  assertResIsEqualInPathList(necessaryPaths, foundPaths);
}

const testAdvancedPathKShortestPathMultiLimit = (testGraph) => testAdvancedPathKShortestPathMultiLimitGen(testGraph, generateKShortestPathQuery);
const testAdvancedPathKShortestPathMultiLimitWT = (testGraph) => testAdvancedPathKShortestPathMultiLimitGen(testGraph, generateKShortestPathQueryWT);

function testAdvancedPathKShortestPathMultiLimitGen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const limits = [1, 2, 3];

  _.each(limits, (limit) => {
    const query = generator(testGraph, 'A', 'I', limit);

    const allowedPaths = [
      ["A", "D", "E", "H", "I"],
      ["A", "D", "E", "F", "G", "H", "I"],
      ["A", "B", "C", "D", "E", "H", "I"]
    ];

    const res = db._query(query);
    const actualPaths = res.toArray();

    checkResIsValidKShortestPathListNoWeights(allowedPaths, actualPaths, limit);
  });
}

function testAdvancedPathShortestPathEnabledWeightCheck(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const query = aql`
        FOR v, e IN OUTBOUND SHORTEST_PATH ${testGraph.vertex('A')} TO ${testGraph.vertex('I')}  
        GRAPH ${testGraph.name()} 
        OPTIONS {weightAttribute: ${testGraph.weightAttribute()}}
        RETURN v.key
      `;

  const allowedPaths = [
    ["A", "B", "C", "D", "E", "F", "G", "H", "I"]
  ];

  const res = db._query(query);
  const actualPath = res.toArray();

  assertResIsContainedInPathList(allowedPaths, actualPath);
}

const testAdvancedPathKShortestPathEnabledWeightCheckMultiLimit = (testGraph) => testAdvancedPathKShortestPathEnabledWeightCheckMultiLimitGen(testGraph, generateKShortestPathQueryWithWeights);
const testAdvancedPathKShortestPathEnabledWeightCheckMultiLimitWT = (testGraph) => testAdvancedPathKShortestPathEnabledWeightCheckMultiLimitGen(testGraph, generateKShortestPathQueryWithWeightsWT);

function testAdvancedPathKShortestPathEnabledWeightCheckMultiLimitGen(testGraph, generator) {
  assertTrue(testGraph.name().startsWith(protoGraphs.advancedPath.name()));
  const limits = [1, 2, 3];

  _.each(limits, (limit) => {
    const query = generator(testGraph, 'A', 'I', limit);

    const allowedPaths = [
      {vertices: ["A", "B", "C", "D", "E", "F", "G", "H", "I"], weight: 8},
      {vertices: ["A", "D", "E", "F", "G", "H", "I"], weight: 15},
      {vertices: ["A", "B", "C", "D", "E", "H", "I"], weight: 15}
    ];

    const res = db._query(query);
    const actualPath = res.toArray();

    checkResIsValidKShortestPathListWeights(allowedPaths, actualPath, limit);
  });
}

const testsByGraph = {
  openDiamond: {
    testOpenDiamondDfsUniqueVerticesPath,
    testOpenDiamondDfsUniqueVerticesNone,
    testOpenDiamondDfsUniqueEdgesPath,
    testOpenDiamondDfsUniqueEdgesNone,
    testOpenDiamondDfsUniqueEdgesUniqueVerticesPath,
    testOpenDiamondDfsUniqueEdgesUniqueVerticesNone,
    testOpenDiamondDfsLabelVariableForwarding,
    testOpenDiamondBfsUniqueVerticesPath,
    testOpenDiamondBfsUniqueVerticesNone,
    testOpenDiamondBfsUniqueVerticesGlobal,
    testOpenDiamondBfsUniqueEdgesPath,
    testOpenDiamondBfsUniqueEdgesNone,
    testOpenDiamondBfsUniqueEdgesUniqueVerticesPath,
    testOpenDiamondBfsUniqueEdgesUniqueVerticesNone,
    testOpenDiamondBfsUniqueEdgesUniquePathVerticesGlobal,
    testOpenDiamondBfsUniqueEdgesUniqueNoneVerticesGlobal,
    testOpenDiamondBfsLabelVariableForwarding,
    testOpenDiamondWeightedUniqueVerticesPath,
    testOpenDiamondWeightedUniqueVerticesNone,
    testOpenDiamondWeightedUniqueVerticesGlobal,
    testOpenDiamondWeightedUniqueEdgesPath,
    testOpenDiamondWeightedUniqueEdgesNone,
    testOpenDiamondWeightedUniqueEdgesUniqueVerticesPath,
    testOpenDiamondWeightedUniqueEdgesUniqueVerticesNone,
    testOpenDiamondWeightedUniqueEdgesUniquePathVerticesGlobal,
    testOpenDiamondWeightedUniqueEdgesUniqueNoneVerticesGlobal,
    testOpenDiamondWeightedLabelVariableForwarding,
    testOpenDiamondShortestPath,
    testOpenDiamondShortestPathWT,
    testOpenDiamondKPaths,
    testOpenDiamondShortestPathEnabledWeightCheck,
    testOpenDiamondShortestPathEnabledWeightCheckWT,
    testOpenDiamondKShortestPathWithMultipleLimits,
    testOpenDiamondKShortestPathWithMultipleLimitsWT,
    testOpenDiamondKShortestPathEnabledWeightCheckLimit1,
    testOpenDiamondKShortestPathEnabledWeightCheckLimit1WT,
    testOpenDiamondWeightedUniqueVerticesPathEnabledWeights,
    testOpenDiamondWeightedUniqueVerticesNoneEnabledWeights,
    testOpenDiamondWeightedUniqueVerticesGlobalEnabledWeights,
    testOpenDiamondWeightedUniqueEdgesPathEnableWeights,
    testOpenDiamondWeightedUniqueEdgesNoneEnableWeights,
    testOpenDiamondWeightedUniqueEdgesUniqueVerticesPathEnableWeights,
    testOpenDiamondWeightedUniqueEdgesUniqueVerticesNoneEnableWeights,
    testOpenDiamondWeightedUniqueEdgesUniquePathVerticesGlobalEnableWeights,
    testOpenDiamondWeightedUniqueEdgesUniqueNoneVerticesGlobalEnableWeights
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
    testSmallCircleBfsUniqueVerticesUniqueEdgesNone,
    testSmallCircleBfsUniqueEdgesPathUniqueVerticesGlobal,
    testSmallCircleBfsUniqueEdgesNoneUniqueVerticesGlobal,
    testSmallCircleWeightedUniqueVerticesPath,
    testSmallCircleWeightedUniqueVerticesNone,
    testSmallCircleWeightedUniqueEdgesPath,
    testSmallCircleWeightedUniqueEdgesNone,
    testSmallCircleWeightedUniqueVerticesUniqueEdgesPath,
    testSmallCircleWeightedUniqueVerticesUniqueEdgesNone,
    testSmallCircleWeightedUniqueEdgesPathUniqueVerticesGlobal,
    testSmallCircleWeightedUniqueEdgesNoneUniqueVerticesGlobal,
    testSmallCircleShortestPath,
    testSmallCircleKPaths,
    testSmallCircleShortestPathEnabledWeightCheck,
    testSmallCircleKShortestPathWithMultipleLimits,
    testSmallCircleKShortestPathWithMultipleLimitsWT,
    testSmallCircleKShortestPathEnabledWeightCheckWithMultipleLimits,
    testSmallCircleKShortestPathEnabledWeightCheckWithMultipleLimitsWT
  },
  completeGraph: {
    testCompleteGraphDfsUniqueVerticesPathD1,
    testCompleteGraphDfsUniqueVerticesPathD2,
    testCompleteGraphDfsUniqueVerticesPathD3,
    testCompleteGraphDfsUniqueEdgesPathD1,
    testCompleteGraphDfsUniqueEdgesPathD2,
    testCompleteGraphDfsUniqueVerticesUniqueEdgesPathD2,
    testCompleteGraphDfsUniqueVerticesUniqueEdgesNoneD2,
    testCompleteGraphBfsUniqueVerticesPathD1,
    testCompleteGraphBfsUniqueVerticesPathD2,
    testCompleteGraphBfsUniqueVerticesPathD3,
    testCompleteGraphBfsUniqueVerticesNoneD1,
    testCompleteGraphBfsUniqueVerticesNoneD2,
    testCompleteGraphBfsUniqueVerticesNoneD3,
    testCompleteGraphBfsUniqueEdgesPathD1,
    testCompleteGraphBfsUniqueEdgesPathD2,
    testCompleteGraphBfsUniqueEdgesPathD3,
    testCompleteGraphBfsUniqueEdgesNoneD1,
    testCompleteGraphBfsUniqueEdgesNoneD2,
    testCompleteGraphBfsUniqueVerticesUniqueEdgesPathD3,
    testCompleteGraphBfsUniqueVerticesUniqueEdgesNoneD3,
    testCompleteGraphBfsUniqueEdgesPathUniqueVerticesGlobalD1,
    testCompleteGraphBfsUniqueEdgesPathUniqueVerticesGlobalD3,
    testCompleteGraphBfsUniqueEdgesNoneUniqueVerticesGlobalD3,
    testCompleteGraphBfsUniqueEdgesPathUniqueVerticesGlobalD10,
    testCompleteGraphBfsUniqueEdgesNoneUniqueVerticesGlobalD10,
    testCompleteGraphWeightedUniqueVerticesPathD1,
    testCompleteGraphWeightedUniqueVerticesPathD2,
    testCompleteGraphWeightedUniqueVerticesPathD3,
    testCompleteGraphWeightedUniqueVerticesNoneD1,
    testCompleteGraphWeightedUniqueVerticesNoneD2,
    testCompleteGraphWeightedUniqueVerticesNoneD3,
    testCompleteGraphWeightedUniqueEdgesPathD1,
    testCompleteGraphWeightedUniqueEdgesPathD2,
    testCompleteGraphWeightedUniqueEdgesPathD3,
    testCompleteGraphWeightedUniqueEdgesNoneD1,
    testCompleteGraphWeightedUniqueEdgesNoneD2,
    testCompleteGraphWeightedUniqueVerticesUniqueEdgesPathD3,
    testCompleteGraphWeightedUniqueVerticesUniqueEdgesNoneD3,
    testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD1,
    testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD3,
    testCompleteGraphWeightedUniqueEdgesNoneUniqueVerticesGlobalD3,
    testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD10,
    testCompleteGraphWeightedUniqueEdgesNoneUniqueVerticesGlobalD10,
    testCompleteGraphShortestPath,
    testCompleteGraphKPathsD1,
    testCompleteGraphKPathsD2,
    testCompleteGraphKPathsD3,
    testCompleteGraphShortestPathEnabledWeightCheck,
    testCompleteGraphKShortestPathLimit1,
    testCompleteGraphKShortestPathLimit1WT,
    testCompleteGraphKShortestPathLimit3,
    testCompleteGraphKShortestPathLimit3WT,
    testCompleteGraphKShortestPathEnabledWeightCheckMultiLimit,
    testCompleteGraphKShortestPathEnabledWeightCheckMultiLimitWT,
    testCompleteGraphWeightedUniqueVerticesPathD1EnabledWeights,
    testCompleteGraphWeightedUniqueVerticesPathD2EnabledWeights,
    testCompleteGraphWeightedUniqueVerticesPathD3EnabledWeights,
    testCompleteGraphWeightedUniqueVerticesNoneD1EnabledWeights,
    testCompleteGraphWeightedUniqueVerticesNoneD2EnabledWeights,
    testCompleteGraphWeightedUniqueVerticesNoneD3EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesPathD1EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesPathD2EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesPathD3EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesNoneD1EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesNoneD2EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesNoneD3EnabledWeights,
    testCompleteGraphWeightedUniqueVerticesUniqueEdgesPathD3EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD1EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD3EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesPathUniqueVerticesGlobalD10EnabledWeights,
    testCompleteGraphWeightedUniqueEdgesNoneUniqueVerticesGlobalD10EnabledWeights,
  },
  easyPath: {
    testEasyPathAllCombinations,
    testEasyPathShortestPath,
    testEasyPathKPaths,
    testEasyPathShortestPathEnabledWeightCheck,
    testEasyPathKShortestPathMultipleLimits,
    testEasyPathKShortestPathMultipleLimitsWT,
    testEasyPathKShortestPathEnabledWeightCheckMultipleLimits,
    testEasyPathKShortestPathEnabledWeightCheckMultipleLimitsWT
  },
  advancedPath: {
    testAdvancedPathDfsUniqueVerticesPath,
    testAdvancedPathDfsUniqueVerticesNone,
    testAdvancedPathDfsUniqueEdgesPath,
    testAdvancedPathDfsUniqueEdgesNone,
    testAdvancedPathDfsUniqueEdgesUniqueVerticesPath,
    testAdvancedPathDfsUniqueEdgesUniqueVerticesNone,
    testAdvancedPathBfsUniqueVerticesPath,
    testAdvancedPathBfsUniqueVerticesNone,
    testAdvancedPathBfsUniqueEdgesPath,
    testAdvancedPathBfsUniqueEdgesNone,
    testAdvancedPathBfsUniqueEdgesUniqueVerticesPath,
    testAdvancedPathBfsUniqueEdgesUniqueVerticesNone,
    testAdvancedPathBfsUniqueEdgesUniquePathVerticesGlobal,
    testAdvancedPathBfsUniqueEdgesUniqueNoneVerticesGlobal,

    testAdvancedPathWeightedUniqueVerticesPath,
    testAdvancedPathWeightedUniqueVerticesNone,
    testAdvancedPathWeightedUniqueEdgesPath,
    testAdvancedPathWeightedUniqueEdgesNone,
    testAdvancedPathWeightedUniqueEdgesUniqueVerticesPath,
    testAdvancedPathWeightedUniqueEdgesUniqueVerticesNone,
    testAdvancedPathWeightedUniqueEdgesUniquePathVerticesGlobal,
    testAdvancedPathWeightedUniqueEdgesUniqueNoneVerticesGlobal,

    testAdvancedPathWeightedUniqueVerticesPathEnabledWeights,
    testAdvancedPathWeightedUniqueVerticesNoneEnabledWeights,
    testAdvancedPathWeightedUniqueEdgesPathEnabledWeights,
    testAdvancedPathWeightedUniqueEdgesNoneEnabledWeights,
    testAdvancedPathWeightedUniqueEdgesUniqueVerticesPathEnabledWeights,
    testAdvancedPathWeightedUniqueEdgesUniqueVerticesNoneEnabledWeights,
    testAdvancedPathWeightedUniqueEdgesUniquePathVerticesGlobalEnabledWeights,
    testAdvancedPathWeightedUniqueEdgesUniqueNoneVerticesGlobalEnabledWeights,

    testAdvancedPathShortestPath,
    testAdvancedPathKPaths,
    testAdvancedPathShortestPathEnabledWeightCheck,
    testAdvancedPathKShortestPathMultiLimit,
    testAdvancedPathKShortestPathMultiLimitWT,
    testAdvancedPathKShortestPathEnabledWeightCheckMultiLimit,
    testAdvancedPathKShortestPathEnabledWeightCheckMultiLimitWT
  },
  largeBinTree: {
    testLargeBinTreeAllCombinations,
  }
};

const metaTests = {
  testMetaDfsValid,
  testMetaDfsInvalid,
  testMetaBfsValid,
  testMetaBfsInvalid,
  testMetaBfsGlobalValid,
  testMetaBfsGlobalInvalid,
  testMetaTreeToPaths,
  testMetaWsValid,
};

exports.testsByGraph = testsByGraph;
exports.metaTests = metaTests;
