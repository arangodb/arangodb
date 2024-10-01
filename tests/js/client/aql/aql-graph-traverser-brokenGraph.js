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
const errors = require('@arangodb').errors;
const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';



const gh = require('@arangodb/graph/helpers');

function brokenGraphSuite() {
  var paramDisabled = {optimizer: {rules: ['-all']}};
  var vc;
  var ec;
  var vertex = {};
  var edge = {};
  return {

    setUpAll: function () {
      gh.cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});

      vertex.A = vc.save({_key: 'A'})._id;
      vertex.B = vc.save({_key: 'B'})._id;

      ec.save(vertex.A, vn + '/missing', {});
      ec.save(vn + '/missing', vertex.B, {});
    },

    tearDownAll: gh.cleanup,

    testRequestMissingVertex: function () {
      var query = `WITH ${vn} FOR x IN OUTBOUND @startId @@eCol RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result, [null]);
    },

    testStartAtMissingVertex: function () {
      var query = `WITH ${vn} FOR x IN OUTBOUND @startId @@eCol RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: vn + '/missing'
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result, [vertex.B]);
    },

    testHopOverMissingVertex: function () {
      var query = `WITH ${vn} FOR x IN 2 OUTBOUND @startId @@eCol RETURN x._id`;
      var bindVars = {
        '@eCol': en,
        startId: vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result, [vertex.B]);
    },

    testFilterOnMissingVertexNotTrue: function () {
      var filter = [
        'FILTER p.vertices[1].attribute == "missing"',
        'FILTER p.vertices[1].attribute > 12',
        'FILTER p.vertices[1] != null'
      ];
      for (var i = 0; i < filter.length; ++i) {
        var query = `WITH ${vn} FOR x, e, p IN 2 OUTBOUND @startId @@eCol ${filter[i]} RETURN x._id`;
        var bindVars = {
          '@eCol': en,
          startId: vertex.A
        };
        var result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 0, 'With opt: ', query);
        result = db._query(query, bindVars, paramDisabled).toArray();
        assertEqual(result.length, 0, 'Without opt: ', query);
      }
    },

    testFilterOnMissingVertexTrue: function () {
      var filter = [
        'FILTER p.vertices[1].attribute != "missing"',
        'FILTER p.vertices[1].attribute < 12',
        'FILTER p.vertices[1] == null'
      ];
      for (var i = 0; i < filter.length; ++i) {
        var query = `WITH ${vn} FOR x, e, p IN 2 OUTBOUND @startId @@eCol ${filter[i]} RETURN x._id`;
        var bindVars = {
          '@eCol': en,
          startId: vertex.A
        };
        var result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 1, 'With opt: ', query);
        assertEqual(result, [vertex.B], 'With opt: ', query);
        result = db._query(query, bindVars, paramDisabled).toArray();
        assertEqual(result.length, 1, 'Without opt: ', query);
        assertEqual(result, [vertex.B], 'Without opt: ', query);
      }
    },

    testQueryWithEmptyGraph: function () {
      var query = 'FOR x IN OUTBOUND "start/123" GRAPH @graph RETURN x';
      var emptyGN = 'UnitTestEmptyGraph';
      try {
        gm._drop(emptyGN);
      } catch (e) {
      }
      gm._create(emptyGN);
      var bindVars = {
        graph: emptyGN
      };
      try {
        db._query(query, bindVars);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_GRAPH_EMPTY.code);
      }
      gm._drop(emptyGN);
    }
  };
}

jsunity.run(brokenGraphSuite);
return jsunity.done();
