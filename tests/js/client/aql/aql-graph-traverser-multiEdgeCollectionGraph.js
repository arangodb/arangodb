/* jshint esnext: true */

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
const db = require('internal').db;

const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';

const executeAllJson = require("@arangodb/aql-helper").executeAllJson;
const gh = require('@arangodb/graph/helpers');

function multiEdgeCollectionGraphSuite() {
  /* *********************************************************************
   * Graph under test:
   *
   *         B<----+       <- B & C via edge collection A
   *               |
   *         D<----A----<C
   *               |
   *               +----<E <- D & E via edge colltion B
   *
   ***********************************************************************/
  const gn = 'UnitTestGraph';
  const en2 = 'UnitTestEdgeCollection2';
  var ruleName = 'optimize-traversals';
  var paramEnabled = {optimizer: {rules: ['-all', '+' + ruleName]}};
  var opts = _.clone(paramEnabled);
  var vertex = {};
  var edge = {};
  var vc, ec;

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

      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      var ec2 = db._createEdgeCollection(en2, {numberOfShards: 4});

      gm._create(gn, [gm._relation(en, vn, vn), gm._relation(en2, vn, vn)]);

      vertex.A = vc.save({_key: 'A'})._id;
      vertex.B = vc.save({_key: 'B'})._id;
      vertex.C = vc.save({_key: 'C'})._id;
      vertex.D = vc.save({_key: 'D'})._id;
      vertex.E = vc.save({_key: 'E'})._id;

      edge.AB = ec.save(vertex.A, vertex.B, {})._id;
      edge.CA = ec.save(vertex.C, vertex.A, {})._id;
      edge.AD = ec2.save(vertex.A, vertex.D, {})._id;
      edge.EA = ec2.save(vertex.E, vertex.A, {})._id;
    },

    tearDownAll: function () {
      gm._drop(gn);
      db._drop(vn);
      db._drop(en);
      db._drop(en2);
      gh.cleanup();
    },

    testTwoVertexCollectionsInOutbound: function () {
      /* this test is intended to trigger the clone functionality. */
      var expectResult = ['B', 'C', 'D', 'E'];
      var query = 'FOR x IN ANY @startId GRAPH @graph SORT x._id RETURN x._key';
      var bindVars = {
        graph: gn,
        startId: vertex.A
      };

      var result = db._query(query, bindVars).toArray();

      assertEqual(result, expectResult, query);
      var plans = db._createStatement({query: query, bindVars: bindVars, options: opts}).explain().plans;
      executeAllJson(plans, result, query);
    }

  };
}

jsunity.run(multiEdgeCollectionGraphSuite);
return jsunity.done();
