/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global describe, before, after, it, AQL_EXPLAIN, AQL_EXECUTE, AQL_EXECUTEJSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for optimizer rules
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
// / @author Wilfried Goesgens, Michael Hackstein
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////
//
const expect = require('chai').expect;

const helper = require('@arangodb/aql-helper');
const isEqual = helper.isEqual;
const graphModule = require('@arangodb/general-graph');
const graphName = 'myUnittestGraph';
const db = require('internal').db;
let graph;
let edgeKey;

const ruleName = 'optimize-traversals';
// various choices to control the optimizer:
const paramEnabled = { optimizer: { rules: [ '-all', '+' + ruleName ] } };
const paramDisabled = { optimizer: { rules: [ '+all', '-' + ruleName ] } };

const cleanup = () => {
  try {
    graphModule._drop(graphName, true);
  } catch (x) {
  }
};

const assertRuleIsUsed = (query) => {
  // assert the rule is not used when it's disabled
  const planDisabled = AQL_EXPLAIN(query, { }, paramDisabled);
  expect(planDisabled.plan.rules.indexOf(ruleName)).to.equal(-1, query);

  // assert the rule is used when it's enabled
  const planEnabled = AQL_EXPLAIN(query, { }, paramEnabled);
  expect(planEnabled.plan.rules.indexOf(ruleName)).to.not.equal(-1, query);
};

const assertResultsAreUnchanged = (query) => {
  // assert the rule is not used when it's disabled.
  const planDisabled = AQL_EXPLAIN(query, { }, paramDisabled);
  expect(planDisabled.plan.rules.indexOf(ruleName)).to.equal(-1, query);

  const resultDisabled = AQL_EXECUTE(query, { }, paramDisabled).json;
  const resultEnabled = AQL_EXECUTE(query, { }, paramEnabled).json;

  expect(isEqual(resultDisabled, resultEnabled)).to.equal(true, query);

  const opts = { allPlans: true, verbosePlans: true, optimizer: { rules: [ '-all', '+' + ruleName ] } };

  const plans = AQL_EXPLAIN(query, {}, opts).plans;
  plans.forEach(function (plan) {
    const jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ '-all' ] } }).json;
    expect(jsonResult).to.deep.equal(resultDisabled, query);
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

describe('Rule optimize-traversals', () => {
  // / @brief set up
  before(() => {
    cleanup();
    graph = graphModule._create(graphName, [
      graphModule._relation('edges', 'circles', 'circles')]);

    // Add circle circles
    graph.circles.save({'_key': 'A', 'label': '1'});
    graph.circles.save({'_key': 'B', 'label': '2'});
    graph.circles.save({'_key': 'C', 'label': '3'});
    graph.circles.save({'_key': 'D', 'label': '4'});
    graph.circles.save({'_key': 'E', 'label': '5'});
    graph.circles.save({'_key': 'F', 'label': '6'});
    graph.circles.save({'_key': 'G', 'label': '7'});

    // Add relevant edges
    edgeKey = graph.edges.save('circles/A', 'circles/C', {theFalse: false, theTruth: true, 'label': 'foo'})._key;
    graph.edges.save('circles/A', 'circles/B', {theFalse: false, theTruth: true, 'label': 'bar'});
    graph.edges.save('circles/B', 'circles/D', {theFalse: false, theTruth: true, 'label': 'blarg'});
    graph.edges.save('circles/B', 'circles/E', {theFalse: false, theTruth: true, 'label': 'blub'});
    graph.edges.save('circles/C', 'circles/F', {theFalse: false, theTruth: true, 'label': 'schubi'});
    graph.edges.save('circles/C', 'circles/G', {theFalse: false, theTruth: true, 'label': 'doo'});
  });

  // / @brief tear down
  after(cleanup);

  it('should remove unused variables', () => {
    const queries = [
      [ `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN 1`, true, [ false, false, false ] ],
      [ `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [v, e]`, true, [ true, true, false ] ],
      [ `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [v, p]`, true, [ true, false, true ] ],
      [ `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [e, p]`, false, [ true, true, true ] ],
      [ `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [v]`, true, [ true, false, false ] ],
      [ `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [e]`, true, [ false, true, false ] ],
      [ `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [p]`, true, [ true, false, true ] ],
      [ `FOR v, e IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [v, e]`, false, [ true, true, false ] ],
      [ `FOR v, e IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [v]`, true, [ true, false, false ] ],
      [ `FOR v IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' RETURN [v]`, false, [ true, false, false ] ]
    ];

    queries.forEach((query) => {
      const result = AQL_EXPLAIN(query[0], { }, paramEnabled);
      expect(result.plan.rules.indexOf(ruleName) !== -1).to.equal(query[1], query);

      // check that variables were correctly optimized away
      let found = false;
      result.plan.nodes.forEach(function (thisNode) {
        if (thisNode.type === 'TraversalNode') {
          expect(thisNode.hasOwnProperty('vertexOutVariable')).to.equal(query[2][0]);
          expect(thisNode.hasOwnProperty('edgeOutVariable')).to.equal(query[2][1]);
          expect(thisNode.hasOwnProperty('pathOutVariable')).to.equal(query[2][2]);
          found = true;
        }
      });
      expect(found).to.be.true;
    });
  });

  it('should not take effect in these queries', () => {
    const queries = [
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      LET localScopeVar = NOOPT(true) FILTER p.edges[0].theTruth == localScopeVar
      RETURN {v:v,e:e,p:p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[-1].theTruth == true
      RETURN {v:v,e:e,p:p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[*].theTruth == true or p.edges[1].label == 'bar'
      RETURN {v:v,e:e,p:p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[RAND()].theFalse == false
      RETURN {v:v,e:e,p:p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[p.edges.length - 1].theFalse == false
      RETURN {v:v,e:e,p:p}`,
      `FOR v, e, p IN 2 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER CONCAT(p.edges[0]._key, '') == " + edgeKey + " SORT v._key
      RETURN {v:v,e:e,p:p}`,
      `FOR v, e, p IN 2 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER NOOPT(CONCAT(p.edges[0]._key, '')) == " + edgeKey + " SORT v._key
      RETURN {v:v,e:e,p:p}`,
      `FOR snippet IN ['a', 'b']
      FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[1].label == CONCAT(snippet, 'ar')
      RETURN {v,e,p}`
    ];

    queries.forEach(function (query) {
      const result = AQL_EXPLAIN(query, { }, paramEnabled);
      expect(result.plan.rules.indexOf(ruleName)).to.equal(-1, query);
    });
  });

  it('should take effect in these queries', () => {
    const queries = [
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[0].theTruth == true
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[1].theTruth == true
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[2].theTruth == true
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[2].theTruth == true AND    p.edges[1].label == 'bar'
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[0].theTruth == true FILTER p.edges[1].label == 'bar'
      RETURN {v,e,p}`,
      `FOR snippet IN ['a', 'b']
      LET test = CONCAT(snippet, 'ar')
      FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[1].label == test 
      RETURN {v,e,p}`
    ];
    queries.forEach(function (query) {
      const result = AQL_EXPLAIN(query, { }, paramEnabled);
      expect(result.plan.rules.indexOf(ruleName)).to.not.equal(-1, query);
      result.plan.nodes.forEach((thisNode) => {
        if (thisNode.type === 'TraversalNode') {
          expect(thisNode.hasOwnProperty('condition')).to.equal(true, query);
        }
      });
    });
  });

  it('should not modify the result', () => {
    var queries = [
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[0].label == 'foo'
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[1].label == 'foo'
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[0].theTruth == true FILTER p.edges[0].label == 'foo'
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[0].theTruth == true FILTER p.edges[1].label == 'foo'
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[0].theTruth == true FILTER p.edges[0].label == 'foo'
      RETURN {v,e,p}`,
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}'
      FILTER p.edges[1].theTruth == true FILTER p.edges[1].label == 'foo'
      RETURN {v,e,p}`
    ];

    queries.forEach(query => assertRuleIsUsed(query));
    queries.forEach(query => assertResultsAreUnchanged(query));
  });

  it('should prune when using functions', () => {
    let queries = [
      `WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol
      FILTER p.edges[0]._key == CONCAT(@edgeKey, '')
      SORT v._key RETURN v._key`,
      `WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol
      FILTER p.edges[0]._key == @edgeKey
      SORT v._key RETURN v._key`,
      `WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol
      FILTER CONCAT(p.edges[0]._key, '') == @edgeKey
      SORT v._key RETURN v._key`,
      `WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol
      FILTER NOOPT(CONCAT(p.edges[0]._key, '')) == @edgeKey
      SORT v._key RETURN v._key`
    ];
    const bindVars = {
      start: 'circles/A',
      '@ecol': 'edges',
      edgeKey: edgeKey
    };
    queries.forEach(function (q) {
      let res = AQL_EXECUTE(q, bindVars, paramDisabled).json;
      expect(res.length).to.equal(2, 'query: ' + q);
      expect(res[0]).to.equal('F', 'query: ' + q);
      expect(res[1]).to.equal('G', 'query: ' + q);

      res = AQL_EXECUTE(q, bindVars, paramEnabled).json;

      expect(res.length).to.equal(2, 'query (enabled): ' + q);
      expect(res[0]).to.equal('F', 'query (enabled): ' + q);
      expect(res[1]).to.equal('G', 'query (enabled): ' + q);
    });
  });

  it('should short-circuit conditions that cannot be fulfilled', () => {
    let queries = [
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' FILTER p.edges[7].label == 'foo' RETURN {v,e,p}`,
      // indexed access starts with 0 - this is also forbidden since it will look for the 6th!
      `FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '${graphName}' FILTER p.edges[5].label == 'foo' RETURN {v,e,p}`
      // "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH 'myGraph' FILTER p.edges[1].label == 'foo' AND p.edges[1].label == 'bar' return {v:v,e:e,p:p}"
    ];

    queries.forEach(function (query) {
      let result = AQL_EXPLAIN(query, { }, paramEnabled);
      let simplePlan = helper.getCompactPlan(result);
      expect(result.plan.rules.indexOf(ruleName)).to.not.equal(-1, query);
      expect(simplePlan[2].type).to.equal('NoResultsNode');
    });
  });

  describe('using condition vars', () => {
    it('taking invalid start form subquery', () => {
      const query = `
        LET data = (FOR i IN 1..1 RETURN i)
        FOR v, e, p IN 1..10 OUTBOUND data GRAPH '${graphName}'
        FILTER p.vertices[0]._id == '123'
        FILTER p.vertices[1]._id != null
        FILTER p.edges[0]._id IN data[*].foo.bar
        RETURN 1`;

      const result = AQL_EXPLAIN(query);
      expect(result.plan.rules.indexOf(ruleName)).to.not.equal(-1, query);
      expect(AQL_EXECUTE(query).json.length).to.equal(0);
    });

    it('filtering on a subquery', () => {
      const query = `
        LET data = (FOR i IN 1..1 RETURN i)
        FOR v, e, p IN 1..10 OUTBOUND 'circles/A' GRAPH '${graphName}'
        FILTER p.vertices[0]._id == '123'
        FILTER p.vertices[1]._id != null
        FILTER p.edges[0]._id IN data[*].foo.bar
        RETURN 1`;

      const result = AQL_EXPLAIN(query);
      expect(result.plan.rules.indexOf(ruleName)).to.not.equal(-1, query);
      expect(AQL_EXECUTE(query).json.length).to.equal(0);
    });

    it('filtering on a subquery 2', () => {
      const query = `
        LET data = (FOR i IN 1..1 RETURN i)
        FOR v, e, p IN 1..10 OUTBOUND 'circles/A' GRAPH '${graphName}'
        FILTER p.vertices[0]._id == '123'
        FILTER p.vertices[1]._id != null
        FILTER p.edges[0]._id IN data[*].foo.bar
        FILTER p.edges[1]._key IN data[*].bar.baz._id
        RETURN 1`;

      const result = AQL_EXPLAIN(query);
      expect(result.plan.rules.indexOf(ruleName)).to.not.equal(-1, query);
      expect(AQL_EXECUTE(query).json.length).to.equal(0);
    });
  });

  describe('filtering on own output', () => {
    it('should filter with v on path', () => {
      let query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER v.bar == p.edges[0].foo
        RETURN {v,e,p}
      `;
      const bindVars = {
        start: 'circles/A',
        graph: graphName
      };

      let plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      let result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);

      query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER p.edges[0].foo == v.bar 
        RETURN {v,e,p}
      `;

      plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);
    });

    it('should filter with e on path', () => {
      let query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER e.bar == p.edges[0].foo
        RETURN {v,e,p}
      `;
      const bindVars = {
        start: 'circles/A',
        graph: graphName
      };
      let plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      let result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);

      query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER p.edges[0].foo == e.bar 
        RETURN {v,e,p}
      `;

      plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);
    });

    it('should filter with path.edges on path.edges', () => {
      let query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER p.edges[1].foo == p.edges[0].foo
        RETURN {v,e,p}
      `;
      const bindVars = {
        start: 'circles/A',
        graph: graphName
      };

      let plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      let result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);

      query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER p.edges[0].foo == p.edges[1].foo
        RETURN {v,e,p}
      `;

      result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);
    });

    it('should filter with path.vertices on path.vertices', () => {
      let query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER p.vertices[1].foo == p.vertices[0].foo
        RETURN {v,e,p}
      `;
      const bindVars = {
        start: 'circles/A',
        graph: graphName
      };
      let plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      let result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);

      query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER p.vertices[0].foo == p.vertices[1].foo
        RETURN {v,e,p}
      `;

      plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);
    });

    it('should filter with path.vertices on path.edges', () => {
      let query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER p.vertices[1].foo == p.edges[0].foo
        RETURN {v,e,p}
      `;
      const bindVars = {
        start: 'circles/A',
        graph: graphName
      };

      let plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      let result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);

      query = `
        FOR v,e,p IN 2 OUTBOUND @start GRAPH @graph
        FILTER p.edges[0].foo == p.vertices[1].foo
        RETURN {v,e,p}
      `;

      plan = AQL_EXPLAIN(query, bindVars, paramEnabled);
      expect(plan.plan.rules.indexOf(ruleName)).to.equal(-1, query);

      result = AQL_EXECUTE(query, bindVars).json;
      expect(result.length).to.equal(4);
    });
  });

  describe('various filter optimizations', () => {
    const multiplyArray = (left, right) => {
      // Only works if both are arrays
      let res = [];
      for (let l of left) {
        for (let r of right) {
          res.push(l + r);
        }
      }
      return res;
    };

    const multiplyArrays = (left, middle, right) => {
      // Only works if both are arrays
      let res = [];
      for (let l of left) {
        for (let m of middle) {
          for (let r of right) {
            res.push(l + m + r);
          }
        }
      }
      return res;
    };

    const symetricOperators = [
      ` == `, ` < `, ` <= `, ` != `
    ];
    const asymetricOperators = [
      ` IN `, ` NOT IN `
    ];

    const modifiers = [
      ` ALL`, ` NONE`
    ];
    const arrayCmpStart = [
      `p.edges[*]`,
      `p.vertices[*]`
    ];
    const singleCompStart = [
      `p.edges[0]`,
      `p.vertices[1]`
    ];

    const outputStart = [
      `v`, `e`, `p`
    ];

    const valuePostFixes = [
      ``, `.foo`, `.foo[*]`, `.foo.bar`, `.foo[0]`
    ];

    const constValues = [
      `1`,
      `'test'`,
      `null`,
      `2 + 1`,
      `['foo', 'bar']`,
      `{foo: 'bar'}`
    ];

    const functValues = [
      `NOOPT(V8(3+2))`,
      `APPEND(['foo'], ['bar'], false)`
    ];
    const queryStart = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH '${graphName}' FILTER `;
    const queryEnd = ` RETURN {v,e,p}`;

    const symOperatorsWithModifiers = multiplyArray(modifiers, symetricOperators);
    const noOptSymOperators = multiplyArray([` ANY`], symetricOperators);

    const checkDoesOptimize = (conditions, shouldOptimize) => {
      for (let c of conditions) {
        const q = queryStart + c + queryEnd;
        try {
          const expl = AQL_EXPLAIN(q, { });
          if (shouldOptimize) {
            expect(expl.plan.rules.indexOf(ruleName)).to.not.equal(-1, c);
          } else {
            expect(expl.plan.rules.indexOf(ruleName)).to.equal(-1, c);
          }
          const result = db._query(q);
          // This is just to validate that the server does not crash!
          expect(result.count()).to.be.at.least(0, c);
        } catch (e) {
          // if sth throws an error we should see it
          expect(e.errorMessage).to.equal(undefined, c);
          expect(e.errorNum).to.equal(undefined, c);
        }
      }
    };

    const arrayStarts = multiplyArray(arrayCmpStart, valuePostFixes);
    const singleStarts = multiplyArray(singleCompStart, valuePostFixes);

    describe('should optimize', () => {
      it('symetric operators for point access vs constants', () => {
        const conditions = multiplyArrays(singleStarts, symetricOperators, constValues)
          .concat(multiplyArrays(constValues, symetricOperators, singleStarts));
        checkDoesOptimize(conditions, true);
      });

      it('array modifiers with path array access left', () => {
        const conditions = multiplyArrays(arrayStarts, symOperatorsWithModifiers, constValues);
        checkDoesOptimize(conditions, true);
      });

      it('array modifiers with point access left', () => {
        const conditions = multiplyArrays(constValues, noOptSymOperators, singleStarts);
        checkDoesOptimize(conditions, true);
      });

      it('asymetric operators with path array access left (NOT ANY)', () => {
        const ops = multiplyArray(modifiers.concat([``]), asymetricOperators);
        const conditions = multiplyArrays(arrayStarts, ops, constValues);
        checkDoesOptimize(conditions, true);
      });
    });

    describe('should not optimize', () => {
      it('ANY array modifiers with path array access left', () => {
        const conditions = multiplyArrays(arrayStarts, noOptSymOperators, constValues);
        checkDoesOptimize(conditions, false);
      });

      it('array modifiers with path array access right', () => {
        const conditions = multiplyArrays(constValues, noOptSymOperators, arrayStarts);
        checkDoesOptimize(conditions, false);
      });

      it('asymetric operators with path array access right', () => {
        const ops = multiplyArray(([``, ` ANY`].concat(modifiers)), asymetricOperators);
        const conditions = multiplyArrays(constValues, ops, arrayStarts);
        checkDoesOptimize(conditions, false);
      });

      it('asymetric operators with ANY and path array access left', () => {
        const ops = multiplyArray([` ANY`], asymetricOperators);
        const conditions = multiplyArrays(arrayStarts, ops, constValues);
        checkDoesOptimize(conditions, false);
      });

      it('functions symetric operators with array access', () => {
        let ops = multiplyArray([``, ` ANY`].concat(modifiers), symetricOperators);
        let conditions = multiplyArrays(arrayStarts, ops, functValues);
        checkDoesOptimize(conditions, false);
        conditions = multiplyArrays(functValues, ops, arrayStarts);
        checkDoesOptimize(conditions, false);
      });

      it('functions asymetric operators with array access', () => {
        let ops = multiplyArray([``, ` ANY`].concat(modifiers), asymetricOperators);
        let conditions = multiplyArrays(arrayStarts, ops, functValues);
        checkDoesOptimize(conditions, false);
        conditions = multiplyArrays(functValues, ops, arrayStarts);
        checkDoesOptimize(conditions, false);
      });

      it('functions symetric operators with point access', () => {
        let ops = multiplyArray([``, ` ANY`].concat(modifiers), symetricOperators);
        let conditions = multiplyArrays(singleStarts, ops, functValues);
        checkDoesOptimize(conditions, false);
        conditions = multiplyArrays(functValues, ops, singleStarts);
        checkDoesOptimize(conditions, false);
      });

      it('functions asymetric operators with point access', () => {
        let ops = multiplyArray([``, ` ANY`].concat(modifiers), asymetricOperators);
        let conditions = multiplyArrays(singleStarts, ops, functValues);
        checkDoesOptimize(conditions, false);
        conditions = multiplyArrays(functValues, ops, singleStarts);
        checkDoesOptimize(conditions, false);
      });

      it('other access referencing traversal output', () => {
        // Build all combinations of v,e,p
        const starters = multiplyArray(outputStart.concat(arrayCmpStart).concat(singleCompStart), valuePostFixes);
        const conditions = multiplyArrays(starters, symetricOperators, starters);
        checkDoesOptimize(conditions, false);
      });
    });
  });

  describe('regression tests', () => {
    before(cleanup);
    after(cleanup);

    // https://github.com/arangodb/release-3.4/issues/91
    // Regression test: With 'optimize-traversals' enabled, the filter
    //   p.vertices[* FILTER CURRENT._id != 'V/1'].label ALL == true
    // was effectively changed to
    //   p.vertices[*].label ALL == true
    // . That is, inline expressions were ignored.
    it('inline expressions should not be changed', () => {
      // create data
      graph = graphModule._create(graphName, [
        graphModule._relation('E', 'V', 'V')]);

      graph.V.save({_key: '1', label: false});
      graph.V.save({_key: '2', label: true});
      graph.V.save({_key: '3', label: false});

      graph.E.save('V/1', 'V/2', {});
      graph.E.save('V/1', 'V/3', {});

      const query = `
        FOR v, e, p IN 1..10 OUTBOUND 'V/1' GRAPH '${graphName}'
        FILTER p.vertices[* FILTER CURRENT._id != 'V/1'].label ALL == true
        RETURN p.vertices
      `;

      assertResultsAreUnchanged(query);
    });
  });
});
