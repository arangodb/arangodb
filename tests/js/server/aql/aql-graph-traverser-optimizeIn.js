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


const internal = require('internal');
const db = internal.db;
const isCluster = require('@arangodb/cluster').isCluster();
const errors = require('@arangodb').errors;

const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';

const removeCost = require('@arangodb/aql-helper').removeCost;

const gh = require('@arangodb/graph/helpers');

function optimizeInSuite() {
  var ruleName = 'optimize-traversals';
  var startId = vn + '/optIn';
  var vc;
  var ec;

  return {

    setUpAll: function () {
      gh.cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      vc.save({_key: startId.split('/')[1]});

      for (let i = 0; i < 100; ++i) {
        const tmp = vc.insert({_key: 'tmp' + i, value: i});
        ec.insert(startId, tmp._id, {_key: 'tmp' + i, value: i});
        let innerVs = [];
        for (let j = 0; j < 100; ++j) {
          innerVs.push({_key: 'innertmp' + i + '_' + j});
        }
        innerVs = vc.insert(innerVs);
        ec.insert(innerVs.map(v => { return {_from: tmp._id, _to: v._id }; }));
      }
    },

    tearDownAll: gh.cleanup,

    testSingleOptimize: function () {
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      var vertexQuery = `WITH ${vn}
      FOR v, e, p IN 2 OUTBOUND @startId @@eCol
      FILTER p.vertices[1]._key IN @keys
      RETURN v._key`;
      var edgeQuery = `WITH ${vn}
      FOR v, e, p IN 2 OUTBOUND @startId @@eCol
      FILTER p.edges[0]._key IN @keys
      RETURN v._key`;
      var bindVars = {
        '@eCol': en,
        'startId': startId,
        'keys': ['tmp0', 'tmp1', 'tmp2', 'tmp3', 'tmp4', 'tmp5', 'tmp6', 'tmp7', 'tmp8', 'tmp9']
      };

      var result = db._query(vertexQuery, bindVars);
      var extra = result.getExtra();

      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars);
      extra = result.getExtra();
      if (isCluster) {
        // The cluster uses a different index no filtering on _key
        // this unfortunately depends on the storage engine used
        assertTrue(extra.stats.filtered === 90 || extra.stats.filtered === 0);
      } else {
        // We have only 10 valid elements in the array.
        assertEqual(extra.stats.filtered, 0);
      }
      assertEqual(result.count(), 1000);

      // if the rule is disabled we expect to do way more filtering
      var noOpt = {optimizer: {rules: ['-all']}};
      result = db._query(vertexQuery, bindVars, {}, noOpt);

      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);
    },

    testCombinedAndOptimize: function () {
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      var vertexQuery = `WITH ${vn}
      FOR v, e, p IN 2 OUTBOUND @startId @@eCol
      FILTER p.vertices[1]._key IN @keys
      AND p.vertices[1].value IN @values
      RETURN v._key`;
      var edgeQuery = `WITH ${vn}
      FOR v, e, p IN 2 OUTBOUND @startId @@eCol
      FILTER p.edges[0]._key IN @keys
      AND p.edges[0].value IN @values
      RETURN v._key`;
      var mixedQuery1 = `WITH ${vn}
      FOR v, e, p IN 2 OUTBOUND @startId @@eCol
      FILTER p.edges[0]._key IN @keys
      AND p.vertices[1].value IN @values
      RETURN v._key`;
      var mixedQuery2 = `WITH ${vn}
      FOR v, e, p IN 2 OUTBOUND @startId @@eCol
      FILTER p.vertices[1]._key IN @keys
      AND p.edges[0].value IN @values
      RETURN v._key`;
      var bindVars = {
        '@eCol': en,
        'startId': startId,
        'keys': ['tmp0', 'tmp1', 'tmp2', 'tmp3', 'tmp4', 'tmp5', 'tmp6', 'tmp7', 'tmp8', 'tmp9'],
        'values': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
      };

      var result = db._query(vertexQuery, bindVars);
      var extra = result.getExtra();

      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars);
      extra = result.getExtra();

      if (isCluster) {
        // The cluster uses a different index no filtering on _key
        // this unfortunately depends on the storage engine used
        assertTrue(extra.stats.filtered === 90 || extra.stats.filtered === 0);
      } else {
        // We have only 10 valid elements in the array.
        assertEqual(extra.stats.filtered, 0);
      }
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery1, bindVars);
      extra = result.getExtra();

      if (isCluster) {
        // The cluster uses a different index no filtering on _key
        // this unfortunately depends on the storage engine used
        assertTrue(extra.stats.filtered === 90 || extra.stats.filtered === 0);
      } else {
        // We have only 10 valid elements in the array.
        assertEqual(extra.stats.filtered, 0);
      }
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery2, bindVars);
      extra = result.getExtra();
      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      // if the rule is disabled we expect to do way more filtering
      var noOpt = {optimizer: {rules: ['-all']}};
      result = db._query(vertexQuery, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery1, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery2, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);
    },

    testCombinedNoOptimize: function () {
      var vertexQuery = `WITH ${vn}
      FOR v, e, p IN 2 OUTBOUND @startId @@eCol
      FILTER @obj IN p.vertices
      RETURN [v, e, p]`;
      var edgeQuery = `WITH ${vn}
      FOR v, e, p IN 2 OUTBOUND @startId @@eCol
      FILTER @obj IN p.edges
      RETURN [v, e, p]`;
      var bindVars = {
        '@eCol': en,
        'startId': startId,
        'obj': {'_key': 'tmp0', 'value': 0}
      };

      var noOpt = {optimizer: {rules: ['-all']}};
      var opt = {optimizer: {rules: ['-all', '+' + ruleName]}};

      var optPlans = AQL_EXPLAIN(vertexQuery, bindVars, opt).plan;
      var noOptPlans = AQL_EXPLAIN(vertexQuery, bindVars, noOpt).plan;
      assertEqual(optPlans.rules, []);
      // This query cannot be optimized by traversal rule
      // we do not want to test estimatedCost or selectivityEstimate here
      // 1.) subject to rounding errors and other fluctuations
      // 2.) absolute numbers for estimatedCost and selectivityEstimate are an implementation detail and meaningless for this test.
      assertEqual(removeCost(optPlans), removeCost(noOptPlans));

      optPlans = AQL_EXPLAIN(edgeQuery, bindVars, opt).plan;
      noOptPlans = AQL_EXPLAIN(edgeQuery, bindVars, noOpt).plan;
      assertEqual(optPlans.rules, []);
      // This query cannot be optimized by traversal rule
      // we do not want to test estimatedCost or selectivityEstimate here
      // 1.) subject to rounding errors and other fluctuations
      // 2.) absolute numbers for estimatedCost and selectivityEstimate are an implementation detail and meaningless for this test.
      assertEqual(removeCost(optPlans), removeCost(noOptPlans));
    }
  };
}

jsunity.run(optimizeInSuite);
return jsunity.done();
