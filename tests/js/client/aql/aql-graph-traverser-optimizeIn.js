/* jshint esnext: true */

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
// / @author Michael Hackstein
// / @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, fail} = jsunity.jsUnity.assertions;
const internal = require('internal');
const db = require('internal').db;
const isCluster = require("internal").isCluster();
const errors = require('@arangodb').errors;
const removeCost = require('@arangodb/aql-helper').removeCost;
const gh = require('@arangodb/graph/helpers');
const waitForEstimatorSync = require('@arangodb/test-helper').waitForEstimatorSync;

const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';

function optimizeInSuite() {
  const ruleName = 'optimize-traversals';
  const startId = vn + '/optIn';
  let vc;
  let ec;

  return {
    setUpAll: function () {
      gh.cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});

      vc.insert({_key: startId.split('/')[1]});

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({_key: 'tmp' + i, value: i});
      }
      let ids = vc.insert(docs).map((doc) => doc._id);

      docs = [];
      ids.forEach((id, i) => { 
        docs.push({ _from: startId, _to: id, _key: 'tmp' + i, value: i });
      });
      ec.insert(docs);

      ids.forEach((id, i) => {
        let docs = [];
        for (let j = 0; j < 100; ++j) {
          docs.push({_key: 'innertmp' + i + '_' + j});
        }
        let innerIds = vc.insert(docs).map((doc) => doc._id);

        docs = [];
        for (let j = 0; j < 100; ++j) {
          docs.push({ _from: id, _to: innerIds[j] });
        }
        ec.insert(docs);
      });
    },

    tearDownAll: gh.cleanup,

    testSingleOptimize: function () {
      waitForEstimatorSync();
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

      var result = db._query(vertexQuery, bindVars, {count: true});
      var extra = result.getExtra();

      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars, {count: true});
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
      result = db._query(vertexQuery, bindVars, {count: true}, noOpt);

      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars, {count: true}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);
    },

    testCombinedAndOptimize: function () {
      waitForEstimatorSync();
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

      var result = db._query(vertexQuery, bindVars, {count: true});
      var extra = result.getExtra();

      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars, {count: true});
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

      result = db._query(mixedQuery1, bindVars, {count: true});
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

      result = db._query(mixedQuery2, bindVars, {count: true});
      extra = result.getExtra();
      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      // if the rule is disabled we expect to do way more filtering
      var noOpt = {optimizer: {rules: ['-all']}};
      result = db._query(vertexQuery, bindVars, {count: true}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars, {count: true}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery1, bindVars, {count: true}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery2, bindVars, {count: true}, noOpt);
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

      var optPlans = db._createStatement({query: vertexQuery, bindVars: bindVars, options: opt}).explain().plan;
      var noOptPlans = db._createStatement({query: vertexQuery, bindVars: bindVars, options: noOpt}).explain().plan;
      assertEqual(optPlans.rules, []);
      // This query cannot be optimized by traversal rule
      // we do not want to test estimatedCost or selectivityEstimate here
      // 1.) subject to rounding errors and other fluctuations
      // 2.) absolute numbers for estimatedCost and selectivityEstimate are an implementation detail and meaningless for this test.
      assertEqual(removeCost(optPlans), removeCost(noOptPlans));

      optPlans = db._createStatement({query:edgeQuery, bindVars: bindVars, options: opt}).explain().plan;
      noOptPlans = db._createStatement({query:edgeQuery, bindVars: bindVars, options: noOpt}).explain().plan;
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
