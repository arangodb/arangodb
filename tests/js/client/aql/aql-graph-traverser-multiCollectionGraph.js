/* jshint esnext: true */
/* global AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON, arango */

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

const internal = require('internal');
const db = require('internal').db;
const errors = require('@arangodb').errors;
const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';

function execute_json(plans, result, query) {
  plans.forEach(function (plan) {
  let command = `
    let plan = ${JSON.stringify(plan)};
    return AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}});
  `;
    var jsonResult = arango.POST("/_admin/execute", command).json;
    assertEqual(jsonResult, result, query);
  });
};

const gh = require('@arangodb/graph/helpers');
var _ = require('lodash');

function multiCollectionGraphSuite() {
  /* *********************************************************************
   * Graph under test:
   *
   *  A -> B -> C -> D <-E2- V2:G
   *      /|\  \|/
   *       E <- F
   *
   ***********************************************************************/
  const gn = 'UnitTestGraph';
  const vn2 = 'UnitTestVertexCollection2';
  const en2 = 'UnitTestEdgeCollection2';
  var ruleName = 'optimize-traversals';
  var paramEnabled = {optimizer: {rules: ['-all', '+' + ruleName]}};
  var opts = _.clone(paramEnabled);

  // We always use the same query, the result should be identical.
  var validateResult = function (result) {
    assertEqual(result.length, 1);
    var entry = result[0];
    assertEqual(entry.vertex._id, gh.vertex.C);
    assertEqual(entry.path.vertices.length, 2);
    assertEqual(entry.path.vertices[0]._id, gh.vertex.B);
    assertEqual(entry.path.vertices[1]._id, gh.vertex.C);
    assertEqual(entry.path.edges.length, 1);
    assertEqual(entry.path.edges[0]._id, gh.edge.BC);
  };

  return {

    setUpAll: function () {
      opts.allPlans = true;
      opts.verbosePlans = true;
      gh.cleanup();
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }
      db._drop(vn2);
      db._drop(en2);
      gh.createBaseGraph();
      gm._create(gn, [gm._relation(en, vn, vn), gm._relation(en2, vn2, vn)]);
      db[vn2].save({_key: 'G'});
      db[en2].save(vn2 + '/G', vn + '/D', {});
    },

    tearDownAll: function () {
      gm._drop(gn);
      db._drop(vn2);
      db._drop(en2);
      gh.cleanup();
    },

    testNoBindParameterDoubleFor: function () {
      /* this test is intended to trigger the clone functionality. */
      var query = 'FOR t IN ' + vn +
        ' FOR s IN ' + vn2 +
        ' FOR x, e, p IN OUTBOUND t ' + en + ' SORT x._key, e._key RETURN {vertex: x, path: p}';
      var result = db._query(query).toArray();
      var plans = db._createStatement({query: query, bindVars: {}, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testNoBindParameterSingleFor: function () {
      var query = 'FOR s IN ' + vn + ' FOR x, e, p IN OUTBOUND s ' + en + ' SORT x._key RETURN x';
      var result = db._query(query).toArray();
      var plans = db._createStatement({query: query, bindVars: {}, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testNoBindParameterSingleForFilter: function () {
      var query = 'FOR s IN ' + vn + ' FOR x, e, p IN OUTBOUND s ' +
        en + ' FILTER p.vertices[1]._key == s._key SORT x._key RETURN x';
      var result = db._query(query).toArray();
      assertEqual(result.length, 0);
      var plans = db._createStatement({query: query, bindVars: {}, options: opts}).explain().plans;
            execute_json(plans, result, query);
    },

    testNoBindParameterRandFunction: function () {
      var query = 'FOR s IN ' + vn + ' FOR x, e, p IN OUTBOUND s ' +
        en + ' FILTER p.vertices[1]._key == NOOPT(RAND()) SORT x._key RETURN x';
      var result = db._query(query).toArray();
      assertEqual(result.length, 0);
      var plans = db._createStatement({query: query, bindVars: {}, options: opts}).explain().plans;
            execute_json(plans, result, query);
    },

    testNoBindParameter: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN OUTBOUND '${gh.vertex.B}' ${en}
      SORT x._key
      RETURN {vertex: x, path: p}`;
      var result = db._query(query).toArray();
      validateResult(result);
      var plans = db._createStatement({query: query, bindVars: {}, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testStartBindParameter: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN OUTBOUND @startId ${en}
      SORT x._key
      RETURN {vertex: x, path: p}`;
      var bindVars = {
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testEdgeCollectionBindParameter: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN OUTBOUND '${gh.vertex.B}' @@eCol
      SORT x._key
      RETURN {vertex: x, path: p}`;
      var bindVars = {
        '@eCol': en
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },
    
    testEdgeCollectionBindParameterNoCol: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN OUTBOUND '${gh.vertex.B}' @eCol
      SORT x._key
      RETURN {vertex: x, path: p}`;
      var bindVars = {
        'eCol': en
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },
    
    testEdgeCollectionBindParameterNonExisting: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN OUTBOUND '${gh.vertex.B}' @@eCol
      SORT x._key
      RETURN {vertex: x, path: p}`;
      var bindVars = {
        '@eCol': 'FleischmannNonExisting'
      };
      try {
        db._query(query, bindVars);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
    },
    
    testEdgeCollectionBindParameterNoColNonExisting: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN OUTBOUND '${gh.vertex.B}' @eCol
      SORT x._key
      RETURN {vertex: x, path: p}`;
      var bindVars = {
        'eCol': 'FleischmannNonExisting'
      };
      try {
        db._query(query, bindVars);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
    },

    testStepsBindParameter: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN @steps OUTBOUND '${gh.vertex.B}' ${en}
      SORT x._key
      RETURN {vertex: x, path: p}`;
      var bindVars = {
        steps: 1
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testStepsRangeBindParameter: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN @lsteps..@rsteps OUTBOUND '${gh.vertex.B}' ${en}
      SORT x._key
      RETURN {vertex: x, path: p}`;
      var bindVars = {
        lsteps: 1,
        rsteps: 1
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testFirstEntryIsVertex: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND @startId @@eCol
      SORT x._key
      RETURN x`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, gh.vertex.C);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testSecondEntryIsEdge: function () {
      var query = `WITH ${vn}
      FOR x, e IN OUTBOUND @startId @@eCol
      SORT x._key
      RETURN e`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, gh.edge.BC);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testThirdEntryIsPath: function () {
      var query = `WITH ${vn}
      FOR x, e, p IN OUTBOUND @startId @@eCol
      SORT x._key
      RETURN p`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry.vertices.length, 2);
      assertEqual(entry.vertices[0]._id, gh.vertex.B);
      assertEqual(entry.vertices[1]._id, gh.vertex.C);
      assertEqual(entry.edges.length, 1);
      assertEqual(entry.edges[0]._id, gh.edge.BC);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testOutboundDirection: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND @startId @@eCol
      SORT x._key
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry, gh.vertex.C);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testInboundDirection: function () {
      var query = `WITH ${vn}
      FOR x IN INBOUND @startId @@eCol
      SORT x._key
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.C
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry, gh.vertex.B);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testAnyDirection: function () {
      var query = `WITH ${vn}
      FOR x IN ANY @startId @@eCol
      SORT x._id ASC
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 3);
      var entry = result[0];
      assertEqual(entry, gh.vertex.A);
      entry = result[1];
      assertEqual(entry, gh.vertex.C);
      entry = result[2];
      assertEqual(entry, gh.vertex.E);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testExactNumberSteps: function () {
      var query = `WITH ${vn}
      FOR x IN 2 OUTBOUND @startId @@eCol
      SORT x._id ASC
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);

      assertEqual(result[0], gh.vertex.D);
      assertEqual(result[1], gh.vertex.F);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testRangeNumberSteps: function () {
      var query = `WITH ${vn}
      FOR x IN 2..3 OUTBOUND @startId @@eCol
      SORT x._id ASC
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 3);

      assertEqual(result[0], gh.vertex.D);
      assertEqual(result[1], gh.vertex.E);
      assertEqual(result[2], gh.vertex.F);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testComputedNumberSteps: function () {
      var query = `WITH ${vn}
      FOR x IN LENGTH([1,2]) OUTBOUND @startId @@eCol
      SORT x._id ASC
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);

      assertEqual(result[0], gh.vertex.D);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testSort: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND @startId @@eCol
      SORT x._id ASC
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: gh.vertex.C
      };

      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], gh.vertex.D);
      assertEqual(result[1], gh.vertex.F);

      // Reverse ordering
      query = `WITH ${vn}
      FOR x IN OUTBOUND @startId @@eCol
      SORT x._id DESC
      RETURN x._id`;

      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], gh.vertex.F);
      assertEqual(result[1], gh.vertex.D);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testSingleDocumentInput: function () {
      var query = `FOR y IN @@vCol FILTER y._id == @startId
          FOR x IN OUTBOUND y @@eCol SORT x._key RETURN x`;
      var bindVars = {
        startId: gh.vertex.B,
        '@eCol': en,
        '@vCol': vn
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, gh.vertex.C);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testListDocumentInput: function () {
      var query = `WITH ${vn}
      FOR y IN @@vCol
      FOR x IN OUTBOUND y @@eCol
      SORT x._id ASC
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        '@vCol': vn
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 6);
      assertEqual(result[0], gh.vertex.B);
      assertEqual(result[1], gh.vertex.B);
      assertEqual(result[2], gh.vertex.C);
      assertEqual(result[3], gh.vertex.D);
      assertEqual(result[4], gh.vertex.E);
      assertEqual(result[5], gh.vertex.F);
      var plans =  db._createStatement({query, bindVars: bindVars, options: opts}).explain().plans;
      execute_json(plans, result, query);
    },

    testOtherCollectionAttributeAccessInput: function () {
      var query = `WITH ${vn}
      FOR y IN @@vCol
      FOR x IN OUTBOUND y._id @@eCol
      SORT x._id ASC
      RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        '@vCol': vn
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 6);
      assertEqual(result[0], gh.vertex.B);
      assertEqual(result[1], gh.vertex.B);
      assertEqual(result[2], gh.vertex.C);
      assertEqual(result[3], gh.vertex.D);
      assertEqual(result[4], gh.vertex.E);
      assertEqual(result[5], gh.vertex.F);
    },

    testTraversalAttributeAccessInput: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND @startId @@eCol
      FOR y IN OUTBOUND x._id @@eCol
      SORT y._id ASC
      RETURN y._id`;
      var bindVars = {
        '@eCol': en,
        'startId': gh.vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], gh.vertex.C);
    },

    testTraversalLetIdInput: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND @startId @@eCol
      LET next = x._id
      FOR y IN OUTBOUND next @@eCol
      SORT y._id ASC
      RETURN y._id`;
      var bindVars = {
        '@eCol': en,
        'startId': gh.vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], gh.vertex.C);
    },

    testTraversalLetDocInput: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND @startId @@eCol
      LET next = x
      FOR y IN OUTBOUND next @@eCol
      SORT y._id ASC
      RETURN y._id`;
      var bindVars = {
        '@eCol': en,
        'startId': gh.vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], gh.vertex.C);
    }

  };
}

jsunity.run(multiCollectionGraphSuite);
return jsunity.done();
