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

const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';

const gh = require('@arangodb/graph/helpers');

function optimizeQuantifierSuite() {
  /* ******************************
   * Graph under test
   * C <-+             +-> F
   *     |             |
   *     +-B<---A--->E-+
   *     |             |
   * D <-+             +-> G
   *
   *
   * Left side has foo: true, right foo: false
   * Top has bar: true, bottom bar: false.
   * A,B,E has bar: true
   * A has foo: true
   * Edges have foo and bar like their target
   *******************************/

  const gn = 'UnitTestGraph';
  let vertices = {};
  let edges = {};
  var ec, vc;

  return {
    setUpAll: function () {
      gh.cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      vertices.A = vc.save({_key: 'A', foo: true, bar: true})._id;
      vertices.B = vc.save({_key: 'B', foo: true, bar: true})._id;
      vertices.C = vc.save({_key: 'C', foo: true, bar: true})._id;
      vertices.D = vc.save({_key: 'D', foo: true, bar: false})._id;
      vertices.E = vc.save({_key: 'E', foo: false, bar: true})._id;
      vertices.F = vc.save({_key: 'F', foo: false, bar: true})._id;
      vertices.G = vc.save({_key: 'G', foo: false, bar: false})._id;

      edges.AB = ec.save({_key: 'AB', _from: vertices.A, _to: vertices.B, foo: true, bar: true})._id;
      edges.BC = ec.save({_key: 'BC', _from: vertices.B, _to: vertices.C, foo: true, bar: true})._id;
      edges.BD = ec.save({_key: 'BD', _from: vertices.B, _to: vertices.D, foo: true, bar: false})._id;
      edges.AE = ec.save({_key: 'AE', _from: vertices.A, _to: vertices.E, foo: false, bar: true})._id;
      edges.EF = ec.save({_key: 'EF', _from: vertices.E, _to: vertices.F, foo: false, bar: true})._id;
      edges.EG = ec.save({_key: 'EG', _from: vertices.E, _to: vertices.G, foo: false, bar: false})._id;

      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }

      gm._create(gn, [gm._relation(en, vn, vn)]);
    },

    tearDownAll: function () {
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }
      gh.cleanup();
    },

    testAllVerticesSingle: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.vertices[*].foo ALL == true
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 11, stats.scannedIndex);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 23);
        assertTrue(stats.scannedIndex <= 22);
        /*
        assertEqual(stats.scannedIndex, 18);
        */
      }
      assertEqual(stats.filtered, 1);

      query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.vertices[*].foo ALL == false
      SORT v._key
      RETURN v._id
      `;
      cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 0);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      assertEqual(stats.scannedIndex, 1);
      assertEqual(stats.filtered, 1);
    },

    testAllEdgesSingle: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.edges[*].foo ALL == true
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 8, stats.scannedIndex);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // TODO Check for Optimization
        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
        /*
        assertEqual(stats.scannedIndex, 13);
        */
      }
      assertTrue(stats.filtered <= 2);

      query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.edges[*].foo ALL == false
      SORT v._key
      RETURN v._id
      `;
      cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 4);
      result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.E, vertices.F, vertices.G]);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 8, stats.scannedIndex);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
        /*
        assertEqual(stats.scannedIndex, 13);
        */
      }
      assertTrue(stats.filtered <= 2);
    },

    testNoneVerticesSingle: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.vertices[*].foo NONE == false
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 11, stats.scannedIndex);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 23);
        assertTrue(stats.scannedIndex <= 22);
        /*
        assertEqual(stats.scannedIndex, 18);
        */
      }
      assertEqual(stats.filtered, 1);

      query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.vertices[*].foo NONE == true
      SORT v._key
      RETURN v._id
      `;
      cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 0);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      assertEqual(stats.scannedIndex, 1);
      assertEqual(stats.filtered, 1);
    },

    testNoneEdgesSingle: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.edges[*].foo NONE == false
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 8, stats.scannedIndex);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
        /*
        assertEqual(stats.scannedIndex, 13);
        */
      }
      assertEqual(stats.filtered, 1);

      query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.edges[*].foo NONE == true
      SORT v._key
      RETURN v._id
      `;
      cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 4);
      result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.E, vertices.F, vertices.G]);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 8);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // TODO Check for Optimization
        assertTrue(stats.scannedIndex <= 17);
        /*
        assertEqual(stats.scannedIndex, 13);
        */
      }
      assertEqual(stats.filtered, 1);
    },

    testAllVerticesMultiple: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.vertices[*].foo ALL == true
      FILTER p.vertices[*].bar ALL == true
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 11, stats.scannedIndex);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
        /*
        assertEqual(stats.scannedIndex, 14);
        */
      }
      assertEqual(stats.filtered, 2);
    },

    testAllEdgesMultiple: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.edges[*].foo ALL == true
      FILTER p.edges[*].bar ALL == true
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 7, stats.scannedIndex);
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
        /*
        assertEqual(stats.scannedIndex, 9);
        */
      }
      assertTrue(stats.filtered <= 3);
    },

    testAllNoneVerticesMultiple: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.vertices[*].foo ALL == true
      FILTER p.vertices[*].bar NONE == false
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 11, stats.scannedIndex);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
        /*
        assertEqual(stats.scannedIndex, 14);
        */
      }
      assertEqual(stats.filtered, 2);
    },

    testAllNoneEdgesMultiple: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.edges[*].foo ALL == true
      FILTER p.edges[*].bar NONE == false
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 7, stats.scannedIndex);
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
        /*
        assertEqual(stats.scannedIndex, 9);
        */
      }
      assertTrue(stats.filtered <= 3);
    },

    testAllVerticesDepth: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.vertices[*].foo ALL == true
      FILTER p.vertices[2].bar == false
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 1);
      let result = cursor.toArray();
      assertEqual(result, [vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 11, stats.scannedIndex);
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
        /*
        assertEqual(stats.scannedIndex, 14);
        */
      }
      assertTrue(stats.filtered <= 4);
    },

    testAllEdgesAndDepth: function () {
      let query = `
      FOR v, e, p IN 0..2 OUTBOUND '${vertices.A}' GRAPH '${gn}'
      FILTER p.edges[*].foo ALL == true
      FILTER p.edges[1].bar == false
      SORT v._key
      RETURN v._id
      `;
      let cursor = db._createStatement({query: query, count: true}).execute();
      assertEqual(cursor.count(), 1);
      let result = cursor.toArray();
      assertEqual(result, [vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 7, stats.scannedIndex);
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
        /*
        assertEqual(stats.scannedIndex, 9);
        */
      }
      assertTrue(stats.filtered <= 4);
    }
  };
}

jsunity.run(optimizeQuantifierSuite);
return jsunity.done();
