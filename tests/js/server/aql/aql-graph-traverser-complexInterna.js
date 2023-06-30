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
const gh = require("@arangodb/graph/helpers");

function complexInternaSuite() {
  var ruleName = 'optimize-traversals';
  var paramEnabled = {optimizer: {rules: ['-all', '+' + ruleName]}};
  var opts = _.clone(paramEnabled);

  return {

    setUp: function () {
      opts.allPlans = true;
      opts.verbosePlans = true;
      gh.cleanup();
      gh.createBaseGraph();
    },

    tearDown: gh.cleanup,

    testUnknownVertexCollection: function () {
      const vn2 = 'UnitTestVertexCollectionOther';
      db._drop(vn2);
      const vc2 = db._create(vn2);
      gh.vc.save({_key: '1'});
      vc2.save({_key: '1'});
      gh.ec.save(gh.vn + '/1', vn2 + '/1', {});

      // [GraphRefactor] Note: Eventually related to #GORDO-1361
      // Currently we always load the start vertex, even if it might not be required.
      // Therefore, "WITH ${gh.vn}" have been added here. This can be improved, after we
      // are capable of handling the issue: #GORDO-1360.
      var query = `WITH ${gh.vn}, ${vn2}
      FOR x IN OUTBOUND @startId @@eCol
      RETURN x`;
      var bindVars = {
        '@eCol': gh.en,
        'startId': gh.vn + '/1'
      };
      // NOTE: vn2 is not explicitly named in AQL
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, vn2 + '/1');
      db._drop(vn2);
    },

    testStepsFromLet: function () {
      var query = `WITH ${gh.vn}
      LET s = 1
      FOR x IN s OUTBOUND @startId @@eCol
      RETURN x`;
      var bindVars = {
        '@eCol': gh.en,
        'startId': gh.vertex.A
      };

      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, gh.vertex.B);
    },

    testMultipleBlocksResult: function () {
      const query = `WITH ${gh.vn}
        FOR x IN OUTBOUND @startId @@eCol
        SORT x._key RETURN x`;
        const amount = 10000;
        const startId = gh.vn + '/test';
        const bindVars = {
        '@eCol': gh.en,
        'startId': startId
      };
      gh.vc.save({_key: startId.split('/')[1]});

      // Insert amount many edges and vertices into the collections.
      const docs = [];
      for (let i = 0; i < amount; ++i) {
        docs.push({_key: i.toString()});
      }
      const vs = gh.vc.insert(docs);
      gh.ec.insert(vs.map(v => ({_from: startId, _to: v._id})));

      // Check that we can get all of them out again.
      var result = db._query(query, bindVars).toArray();
      // Internally: The Query selects elements in chunks, check that nothing is lost.
      assertEqual(result.length, amount);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testSkipSome: function () {
      const query = `WITH ${gh.vn}
      FOR x, e, p IN 1..2 OUTBOUND @startId @@eCol
      LIMIT 4, 100
      RETURN p.vertices[1]._key`;
      const startId = gh.vn + '/start';
      const bindVars = {
        '@eCol': gh.en,
        'startId': startId
      };
      gh.vc.save({_key: startId.split('/')[1]});

      // Insert amount many edges and vertices into the collections.
      for (let i = 0; i < 3; ++i) {
        const tmp = gh.vc.save({_key: '' + i})._id;
        gh.ec.save(startId, tmp, {});
        for (let k = 0; k < 3; ++k) {
          const tmp2 = gh.vc.save({_key: '' + i + '_' + k})._id;
          gh.ec.save(tmp, tmp2, {});
        }
      }

      /*
                   /-> 0_0
               -> 0 -> 0_1
             /     \-> 0_2
            |
            |      /-> 1_0
         start -> 1 -> 1_1
            |      \-> 1_2
            |
             \     /-> 2_0
               -> 2 -> 2_1
                   \-> 2_2
       */

      const isValidResult = result => {
        return true
          // all results must be depth 1 vertices
          && result.every(v => -1 !== ['0', '1', '2'].indexOf(v))
          // we expect exactly 8 results
          && result.length === 8
          // but only two different vertices (because we skipped one subtree)
          && _.uniq(result).length === 2
          // first one (any) of the subtrees must be returned, then the other
          && _.uniq(result.slice(0, 4)).length === 1
          && _.uniq(result.slice(4, 8)).length === 1;
      };

      // Check that we can get all of them out again.
      const result = db._query(query, bindVars).toArray();
      assertTrue(isValidResult(result), result);

      // Each of the 3 parts of this graph contains of 4 nodes, one connected to the start.
      // And 3 connected to the first one. As we do a depth first traversal we expect to skip
      // exactly one sub-tree. Therefor we expect exactly two sub-trees to be traversed.

      const plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        const jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertTrue(isValidResult(jsonResult), JSON.stringify({jsonResult, plan}));
      });
    },

    testManyResults: function () {
      const query = `WITH ${gh.vn}
        FOR x IN OUTBOUND @startId @@eCol
        RETURN x._key`;
      const startId = gh.vn + '/many';
      const bindVars = {
        '@eCol': gh.en,
        'startId': startId
      };
      gh.vc.save({_key: startId.split('/')[1]});
      const amount = 10000;
      const docs = [];
      for (let i = 0; i < amount; ++i) {
        docs.push({});
      }
      const vs = gh.vc.insert(docs);
      gh.ec.insert(vs.map(v => ({_from: startId, _to: v._id})));

      var result = db._query(query, bindVars);
      var found = 0;
      // Count has to be correct
      assertEqual(result.count(), amount);
      while (result.hasNext()) {
        result.next();
        ++found;
      }
      // All elements must be enumerated
      assertEqual(found, amount);
    },

    testTailRecursion: function () {
      // This test is to make sure their is no
      // inifinite callstack in getSome() API
      let query = `
      WITH ${gh.vn}
      FOR id IN 0..100000
      FOR v IN OUTBOUND CONCAT("${gh.vn}/foobar", id) ${gh.en}
      RETURN v
      `;

      let res = db._query(query);
      assertEqual(res.count(), 0);
      // With inifinit callstack in getSome this
      // test will segfault
    },

    testEdgeOptimizeAboveMinDepth: function () {
      // The query should return depth 1
      // Because gh.edge.[1] == null => null != gh.edge.BC => ok
      // And not depth 2/3 because gh.edge.[1]._id == gh.edge.BC => not okay
      let query = `
      WITH ${gh.vn}
      FOR v, e, p IN 1..3 OUTBOUND "${gh.vn}/A" ${gh.en}
      FILTER p.edges[1]._id != "${gh.edge.BC}"
      RETURN v._id`;

      let res = db._query(query);
      assertEqual(res.count(), 1);
      assertEqual(res.toArray(), [gh.vertex.B]);
    },

    testVertexOptimizeAboveMinDepth: function () {
      // The query should return depth 1
      // Because vertices[2] == null => null != gh.vertex.C => ok
      // And not depth 2/3 because vertices[3]._id == gh.vertex.C => not okay
      let query = `
      WITH ${gh.vn}
      FOR v, e, p IN 1..3 OUTBOUND "${gh.vn}/A" ${gh.en}
      FILTER p.vertices[2]._id != "${gh.vertex.C}"
      RETURN v._id`;

      let res = db._query(query);
      assertEqual(res.count(), 1);
      assertEqual(res.toArray(), [gh.vertex.B]);
    },

    testPathOptimizeAboveMinDepth: function () {
      // The query should return depth 1
      // Because vertices[2] == null => null != gh.vertex.C => ok
      // And not depth 2/3 because vertices[3]._id == gh.vertex.C => not okay
      let query = `
      WITH ${gh.vn}
      FOR v, e, p IN 1..3 OUTBOUND "${gh.vn}/A" ${gh.en}
      FILTER p.edges[1]._id != "${gh.edge.BC}"
      FILTER p.vertices[2]._id != "${gh.vertex.C}"
      RETURN v._id`;

      let res = db._query(query);
      assertEqual(res.count(), 1);
      assertEqual(res.toArray(), [gh.vertex.B]);
    },

    testLargeMaxDepth: function () {
      let query = `
      WITH ${gh.vn}
      FOR v, e, p IN 1..4294967295 OUTBOUND "${gh.vn}/A" ${gh.en}
      FILTER p.edges[1]._id != "${gh.edge.BC}"
      FILTER p.vertices[2]._id != "${gh.vertex.C}"
      RETURN v._id`;

      let res = db._query(query);
      assertEqual(res.count(), 1);
      assertEqual(res.toArray(), [gh.vertex.B]);
    },

    testInt64MaxMaxDepth: function () {
      let query = `
      WITH ${gh.vn}
      FOR v, e, p IN 1..9223372036854775807 OUTBOUND "${gh.vn}/A" ${gh.en}
      FILTER p.edges[1]._id != "${gh.edge.BC}"
      FILTER p.vertices[2]._id != "${gh.vertex.C}"
      RETURN v._id`;

      let res = db._query(query);
      assertEqual(res.count(), 1);
      assertEqual(res.toArray(), [gh.vertex.B]);
    },


    testEvenLargerMaxDepth: function () {
      let query = `
      WITH ${gh.vn}
      FOR v, e, p IN 1..18446744073709551615 OUTBOUND "${gh.vn}/A" ${gh.en}
      FILTER p.edges[1]._id != "${gh.edge.BC}"
      FILTER p.vertices[2]._id != "${gh.vertex.C}"
      RETURN v._id`;

      try {
        db._query(query);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testNegativeMinDepth: function () {
      let query = `
      WITH ${gh.vn}
      FOR v, e, p IN -1..3 OUTBOUND "${gh.vn}/A" ${gh.en}
      FILTER p.edges[1]._id != "${gh.edge.BC}"
      FILTER p.vertices[2]._id != "${gh.vertex.C}"
      RETURN v._id`;
      try {
        db._query(query);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testNegativeMaxDepth: function () {
      let query = `
      WITH ${gh.vn}
      FOR v, e, p IN 1..-3 OUTBOUND "${gh.vn}/A" ${gh.en}
      FILTER p.edges[1]._id != "${gh.edge.BC}"
      FILTER p.vertices[2]._id != "${gh.vertex.C}"
      RETURN v._id`;

      try {
        db._query(query);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

  };
}

jsunity.run(complexInternaSuite);
return jsunity.done();
