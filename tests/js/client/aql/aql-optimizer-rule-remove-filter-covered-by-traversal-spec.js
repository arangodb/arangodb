/* global describe, it, before, after */
'use strict';

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

const expect = require('chai').expect;
const db = require('internal').db;

const OptimizeTraversalRule = "optimize-traversals";
const FilterRemoveRule = "remove-filter-covered-by-traversal";
const deactivateOptimizer = { optimizer: { rules: [ "-all" ] } };
const activateOptimizer = { optimizer: { rules: [ "+all" ] } };
const helper = require("@arangodb/aql-helper");
const findExecutionNodes = helper.findExecutionNodes;

describe('Single Traversal Optimizer', function () {
  const vertexCollection = 'UnitTestsOptimizerVertices';
  const edgeCollection = 'UnitTestsOptimizerEdges';
  const startId = `${vertexCollection}/start`;

  const bindVars = {
    "@vertices": vertexCollection,
    "@edges": edgeCollection,
    start: startId
  };

  const dropCollections = () => {
    db._drop(vertexCollection);
    db._drop(edgeCollection);
  };

  const hasNoFilterNode = (plan) => {
    expect(findExecutionNodes(plan, "FilterNode").length).to.equal(0, "Plan should have no filter node");
  };

  const hasFilterNode = (plan) => {
    expect(findExecutionNodes(plan, "FilterNode").length).to.equal(1, "Plan should have a single filter node");
  };

  const validateResult = (query, bindVars) => {
    let resultOpt = db._query(query, bindVars, activateOptimizer).toArray().sort();
    let resultNoOpt = db._query(query, bindVars, deactivateOptimizer).toArray().sort();
    expect(resultOpt).to.deep.equal(resultNoOpt);
  };

  before(function () {
    dropCollections();
    let v = db._create(vertexCollection);
    let e = db._createEdgeCollection(edgeCollection);
    let vertices = [
      {_key: "start"}
    ];
    let edges = [
    ];

    for (let i = 0; i < 10; ++i) {
      vertices.push({_key: `a${i}`, foo: i});
      edges.push({_from: startId, _to: `${vertexCollection}/a${i}`, foo: i});
      for (let j = 0; j < 10; ++j) {
        vertices.push({_key: `a${i}${j}`, foo: j});
        edges.push({_from: `${vertexCollection}/a${i}`, _to: `${vertexCollection}/a${i}${j}`, foo: j, bar: [1, 2, 3, 4, 5]});
      }
    }

    v.save(vertices);
    e.save(edges);
    
  });

  after(dropCollections);

  describe('should remove a single', () => {

    describe('equality filter', () => {
      it('on p.vertices[1].foo', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 2 OUTBOUND @start @@edges
                       FILTER p.vertices[1].foo == 3
                       RETURN v`;

        let plan = db._createStatement({query: query, bindVars:  bindVars, options:  activateOptimizer}).explain();
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on p.vertices[2]', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 2 OUTBOUND @start @@edges
                       FILTER p.vertices[2].foo == 3
                       RETURN v`;
        let plan = db._createStatement({query: query, bindVars:  bindVars, options:  activateOptimizer}).explain();
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on p.vertices[*] ALL', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 2 OUTBOUND @start @@edges
                       FILTER p.vertices[*].foo ALL == 3
                       RETURN v`;
        let plan = db._createStatement({query: query, bindVars:  bindVars, options:  activateOptimizer}).explain();
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on p.edges[*] ALL', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 2 ANY @start @@edges
                       FILTER p.edges[*].foo ALL == 3
                       RETURN v`;
        let plan = db._createStatement({query: query, bindVars:  bindVars, options:  activateOptimizer}).explain();
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on p.edges[*] ALL AND p.vertices[*] NONE', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 0..2 ANY @start @@edges
                       FILTER p.edges[*].foo ALL <= 5
                       FILTER p.vertices[*].foo NONE > 6
                       RETURN v`;
        let plan = db._createStatement({query: query, bindVars:  bindVars, options:  activateOptimizer}).explain();
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      // BTS-2182
      it('on v.foo != 3 AND v.foo < 10', () => {
        let query = `WITH @@vertices
                       FOR v, e IN 0..2 ANY @start @@edges
                       FILTER v.foo < 10
                       FILTER v.foo != 3
                       RETURN v`;
        let plan = db._createStatement({query: query, bindVars:  bindVars, options:  activateOptimizer}).explain();
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on e.bar != 4 AND v.foo < 10 AND v.foo != 3', () => {
        let query = `WITH @@vertices
                       FOR v, e IN 0..2 ANY @start @@edges
                       FILTER e.bar != 4
                       FILTER v.foo < 10 AND v.foo != 3
                       RETURN v`;
        let plan = db._createStatement({query: query, bindVars:  bindVars, options:  activateOptimizer}).explain();
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      // This check that in array will not be handler by Traversal Node
      it('on p.edges[*].foo ALL > 3 AND FILTER v.bar[*] ALL < 10', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 0..2 ANY @start @@edges
                       FILTER p.edges[*].foo ALL > 3
                       FILTER v.bar[*] ALL < 10
                       RETURN v`;
        let plan = db._createStatement({query: query, bindVars:  bindVars, options:  activateOptimizer}).explain();

        hasFilterNode(plan);
        validateResult(query, bindVars);
      });

      // replace-any-eq-with-in rewrites `e.bar[*] ANY == 2` into `2 IN e.bar[*]`,
      // which optimize-traversals can then push into the TraversalNode, so both
      // filters are covered and no FilterNode remains.
      it('on p.edges[*].foo ALL > 3 AND e.bar[*] ANY == 2', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN OUTBOUND @start @@edges
                       FILTER p.edges[*].foo ALL > 3
                       FILTER e.bar[*] ANY == 2
                       RETURN v`;

        // With all rules on: our rewrite fires, optimize-traversals pushes both
        // conditions into the TraversalNode, no FilterNode remains.
        let planAllRules = db._createStatement({query: query, bindVars: bindVars, options: activateOptimizer}).explain();
        hasNoFilterNode(planAllRules);
        validateResult(query, bindVars);

        // Gate test: disable only replace-any-eq-with-in. The BINARY_ARRAY_EQ ANY ==
        // form is not pushed by optimize-traversals, so the FilterNode stays.
        const disableOurRule = { optimizer: { rules: [ "+all", "-replace-any-eq-with-in" ] } };
        let planWithoutOurRule = db._createStatement({query: query, bindVars: bindVars, options: disableOurRule}).explain();
        hasFilterNode(planWithoutOurRule);
      });
    });

    // An expansion with an inline FILTER restricts the quantifier to a subset
    // of the path, which per edge/vertex is an implication:
    //   p.edges[* FILTER B(CURRENT)].attr ALL op y
    //     <=>  for every edge:  !B(edge) || edge.attr op y
    // optimize-traversals pushes that implication into the TraversalNode, so no
    // FilterNode is left. Note the data set only has `bar` on the second level
    // of edges, and no `foo` on the start vertex, so the inline filters below
    // do exclude elements.
    describe('expansion with an inline FILTER', () => {
      const pushedDown = (query, bindVars) => {
        let plan = db._createStatement({query: query, bindVars: bindVars, options: activateOptimizer}).explain();
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      };

      it('on p.edges[*] ALL', () => {
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER CURRENT.bar != null].foo ALL > 3
                      RETURN v._key`, bindVars);
      });

      it('on p.edges[*] NONE', () => {
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER CURRENT.bar != null].foo NONE <= 3
                      RETURN v._key`, bindVars);
      });

      it('on p.vertices[*] ALL, excluding the start vertex', () => {
        // the start vertex has no `foo`, so a plain ALL would reject every path
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.vertices[* FILTER CURRENT.foo != null].foo ALL >= 5
                      RETURN v._key`, bindVars);
      });

      it('with a conjunction in the inline FILTER', () => {
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER CURRENT.foo > 3 && CURRENT.foo < 8].foo ALL != 5
                      RETURN v._key`, bindVars);
      });

      it('with a function call in the inline FILTER', () => {
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER HAS(CURRENT, 'bar')].foo ALL > 3
                      RETURN v._key`, bindVars);
      });

      it('with a nested expansion in the inline FILTER', () => {
        // the inner expansion must not be mistaken for the path access itself
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER CURRENT.bar[*] ANY == 2].foo ALL > 3
                      RETURN v._key`, bindVars);
      });

      it('with CURRENT bound again by a nested expansion', () => {
        // the inner CURRENT is a `bar` element, the outer one an edge. Only
        // references to the outer one may be rewritten to the edge: `edge < 3`
        // would compare an object to a number and select nothing, which turns
        // the implication into a tautology and lets every path through.
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER LENGTH(CURRENT.bar[* FILTER CURRENT < 3]) > 0].foo ALL > 3
                      RETURN v._key`, bindVars);
      });

      it('with a constant true inline FILTER', () => {
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER true].foo ALL > 3
                      RETURN v._key`, bindVars);
      });

      it('with a constant false inline FILTER', () => {
        // no element is left for the quantifier, so ALL is trivially true
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER false].foo ALL > 3
                      RETURN v._key`, bindVars);
      });

      it('with an outer variable in the inline FILTER', () => {
        pushedDown(`WITH @@vertices
                      LET threshold = NOOPT(3)
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER CURRENT.foo > threshold].foo ALL < 9
                      RETURN v._key`, bindVars);
      });

      it('next to a second path condition', () => {
        pushedDown(`WITH @@vertices
                      FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                      FILTER p.edges[* FILTER CURRENT.bar != null].foo ALL > 3
                      FILTER p.vertices[*].foo NONE == 42
                      RETURN v._key`, bindVars);
      });
    });
  });

  describe('should not remove an inline expansion it cannot push', () => {
    const notPushedDown = (query, bindVars) => {
      let plan = db._createStatement({query: query, bindVars: bindVars, options: activateOptimizer}).explain();
      hasFilterNode(plan);
      validateResult(query, bindVars);
    };

    it('on an expansion with an inline LIMIT', () => {
      // a LIMIT needs the whole array, it cannot be evaluated per edge
      notPushedDown(`WITH @@vertices
                       FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                       FILTER p.edges[* FILTER CURRENT.bar != null LIMIT 1].foo ALL > 3
                       RETURN v._key`, bindVars);
    });

    it('on an expansion with an inline RETURN', () => {
      notPushedDown(`WITH @@vertices
                       FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                       FILTER p.edges[* FILTER CURRENT.bar != null RETURN CURRENT.foo] ALL > 3
                       RETURN v._key`, bindVars);
    });

    it('on an inline FILTER referencing the path variable', () => {
      // the condition is evaluated per edge, where the path is not available
      notPushedDown(`WITH @@vertices
                       FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                       FILTER p.edges[* FILTER CURRENT.foo != LENGTH(p.vertices)].foo ALL > 3
                       RETURN v._key`, bindVars);
    });
  });

  describe('should not remove a filter when quantifiers differ', () => {
    // Regression tests for COR-546.
    // NONE > 5 and ALL > 3 together constrain foo to (3, 5] on every edge.
    // Both conditions must be absorbed into the TraversalNode; neither may be
    // dropped. validateResult catches the regression (wrong results if one is
    // silently discarded).
    it('on p.edges[*].foo NONE > 5 AND p.edges[*].foo ALL > 3', () => {
      let query = `WITH @@vertices
                     FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                     FILTER p.edges[*].foo NONE > 5
                     FILTER p.edges[*].foo ALL > 3
                     RETURN v._key`;

      let plan = db._createStatement({query: query, bindVars: bindVars, options: activateOptimizer}).explain();
      hasNoFilterNode(plan);
      validateResult(query, bindVars);
    });

    // AT LEAST (n) cannot be turned into a per-edge condition, as that would
    // apply it to every edge, i.e. it would be treated like ALL. The condition
    // has to stay a post-filter.
    it('on p.edges[*].foo AT LEAST (1) > 3', () => {
      let query = `WITH @@vertices
                     FOR v, e, p IN 1..2 OUTBOUND @start @@edges
                     FILTER p.edges[*].foo AT LEAST (1) > 3
                     RETURN v._key`;

      let plan = db._createStatement({query: query, bindVars: bindVars, options: activateOptimizer}).explain();
      hasFilterNode(plan);
      validateResult(query, bindVars);
    });
  });

});

// An inline FILTER has to be evaluated during the traversal, not on the paths
// it produces, or it breaks vertex uniqueness: a vertex reached over an edge
// the filter rejects still counts as visited, and with uniqueVertices:
// "global" it is then unreachable over the path that the filter keeps.
//
// Graph, with `versionTo` marking the version at which an edge expired
// (null / absent means still valid):
//
//   start -e1-> b -e2(50)-> x -e5-> y      x is reachable through an expired edge
//   start -e3-> c -e4-----> x              ... and through a valid path
//                 c -e9(150)-> w           still valid at version 100
//   start -e6(50)-> d -e7-> z              expired already at depth 1
//
// The filter keeps a path if every edge that has a `versionTo` expired after
// @version. Pushed into the traversal it stops e2 from being followed at all,
// so x is first reached over the valid start->c->x, and both uniqueness modes
// return the same paths.
describe('Traversal vertex uniqueness with an inline FILTER', function () {
  const vertexCollection = 'UnitTestsInlineFilterVertices';
  const edgeCollection = 'UnitTestsInlineFilterEdges';
  const startId = `${vertexCollection}/start`;

  const bindVars = {
    "@vertices": vertexCollection,
    "@edges": edgeCollection,
    start: startId,
    version: 100
  };

  const dropCollections = () => {
    db._drop(vertexCollection);
    db._drop(edgeCollection);
  };

  const query = (uniqueVertices) => `WITH @@vertices
        FOR v, e, p IN 1..4 OUTBOUND @start @@edges
        OPTIONS {uniqueVertices: "${uniqueVertices}", uniqueEdges: "path", order: "bfs"}
        FILTER p.edges[* FILTER CURRENT.versionTo != null].versionTo ALL > @version
        RETURN CONCAT_SEPARATOR(">", p.edges[*]._key)`;

  const run = (uniqueVertices, options) =>
    db._query(query(uniqueVertices), bindVars, options).toArray().sort();

  before(function () {
    dropCollections();
    let v = db._create(vertexCollection);
    let e = db._createEdgeCollection(edgeCollection);
    v.save([{_key: "start"}, {_key: "b"}, {_key: "c"}, {_key: "d"},
            {_key: "w"}, {_key: "x"}, {_key: "y"}, {_key: "z"}]);
    e.save([
      {_key: "e1", _from: `${vertexCollection}/start`, _to: `${vertexCollection}/b`, versionTo: null},
      {_key: "e2", _from: `${vertexCollection}/b`, _to: `${vertexCollection}/x`, versionTo: 50},
      {_key: "e3", _from: `${vertexCollection}/start`, _to: `${vertexCollection}/c`, versionTo: null},
      {_key: "e4", _from: `${vertexCollection}/c`, _to: `${vertexCollection}/x`, versionTo: null},
      {_key: "e5", _from: `${vertexCollection}/x`, _to: `${vertexCollection}/y`, versionTo: null},
      {_key: "e6", _from: `${vertexCollection}/start`, _to: `${vertexCollection}/d`, versionTo: 50},
      {_key: "e7", _from: `${vertexCollection}/d`, _to: `${vertexCollection}/z`, versionTo: null},
      {_key: "e9", _from: `${vertexCollection}/c`, _to: `${vertexCollection}/w`, versionTo: 150}
    ]);
  });

  after(dropCollections);

  it('pushes the condition into the traversal', () => {
    let plan = db._createStatement({query: query("global"), bindVars: bindVars, options: activateOptimizer}).explain();
    expect(findExecutionNodes(plan, "FilterNode").length).to.equal(0, "Plan should have no filter node");
  });

  it('returns the expected paths', () => {
    expect(run("global", activateOptimizer)).to.deep.equal(["e1", "e3", "e3>e4", "e3>e4>e5", "e3>e9"]);
  });

  it('still uses an index for the edge lookup', () => {
    // the pushed down condition is a disjunction (!B || cmp) and ends up in the
    // edge index lookup condition, so make sure it does not degrade into a
    // collection scan
    let cursor = db._query(query("global"), bindVars, activateOptimizer);
    cursor.toArray();
    expect(cursor.getExtra().stats.scannedFull).to.equal(0);
  });

  it('is not treated as covered by a sparse index', () => {
    // the pushed down condition is a disjunction (!B || cmp), which
    // collectOverlappingMembersForTraversal must never consider covered by an
    // index: a sparse index does not contain the edges without a versionTo,
    // which are exactly the ones the inline FILTER lets through
    const edges = db._collection(edgeCollection);
    const index = edges.ensureIndex({type: 'persistent', fields: ['_from', 'versionTo'], sparse: true});
    try {
      expect(run("global", activateOptimizer)).to.deep.equal(["e1", "e3", "e3>e4", "e3>e4>e5", "e3>e9"]);
    } finally {
      edges.dropIndex(index);
    }
  });

  it('returns the same paths for global as for path uniqueness', () => {
    expect(run("global", activateOptimizer)).to.deep.equal(run("path", activateOptimizer));
  });

  it('agrees with the unoptimized plan', () => {
    // uniqueVertices: "path" is the reference: it cannot lose a path to a
    // vertex that was visited through a rejected path
    const reference = run("path", { optimizer: { rules: [ "-optimize-traversals" ] } });
    expect(run("path", activateOptimizer)).to.deep.equal(reference);
    expect(run("global", activateOptimizer)).to.deep.equal(reference);
  });
});
