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
const db = require('internal').db;
const errors = require('@arangodb').errors;
const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';



const gh = require('@arangodb/graph/helpers');

function optionsSuite() {
  const gn = 'UnitTestGraph';
  var vc, ec;

  return {
    setUp: function () {
      gh.cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }

      gm._create(gn, [gm._relation(en, vn, vn)]);
    },

    tearDown: function () {
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }
      gh.cleanup();
    },

    testEdgeUniquenessPath: function () {
      var start = vc.save({_key: 's'})._id;
      var a = vc.save({_key: 'a'})._id;
      var b = vc.save({_key: 'b'})._id;
      var c = vc.save({_key: 'c'})._id;
      var d = vc.save({_key: 'd'})._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(b, c, {});
      ec.save(c, a, {});
      ec.save(a, d, {});
      var cursor = db._query(
        `WITH ${vn}
        FOR v IN 1..10 OUTBOUND '${start}' ${en} OPTIONS {uniqueEdges: 'path'}
        SORT v._key
        RETURN v`).toArray();
      // We expect to get s->a->b->c->a->d
      // and s->a->d
      // But not s->a->b->c->a->b->*
      // And not to continue at a again
      assertEqual(cursor.length, 6);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[1]._id, a); // We somehow return to a
      assertEqual(cursor[2]._id, b); // We once find b
      assertEqual(cursor[3]._id, c); // And once c
      assertEqual(cursor[4]._id, d); // We once find d on short path
      assertEqual(cursor[5]._id, d); // And find d on long path
    },

    testEdgeUniquenessGlobal: function () {
      var start = vc.save({_key: 's'})._id;
      try {
        db._query(
          `WITH ${vn}
          FOR v IN 1..10 OUTBOUND '${start}' ${en} OPTIONS {uniqueEdges: 'global'}
          SORT v._key
          RETURN v`).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_BAD_PARAMETER.code, 'We expect a bad parameter');
      }
    },

    testEdgeUniquenessNone: function () {
      var start = vc.save({_key: 's'})._id;
      var a = vc.save({_key: 'a'})._id;
      var b = vc.save({_key: 'b'})._id;
      var c = vc.save({_key: 'c'})._id;
      var d = vc.save({_key: 'd'})._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(b, c, {});
      ec.save(c, a, {});
      ec.save(a, d, {});
      var cursor = db._query(
        `WITH ${vn}
        FOR v IN 1..10 OUTBOUND '${start}' ${en} OPTIONS {uniqueEdges: 'none'}
        SORT v._key
        RETURN v`).toArray();
      // We expect to get s->a->d
      // We expect to get s->a->b->c->a->d
      // We expect to get s->a->b->c->a->b->c->a->d
      // We expect to get s->a->b->c->a->b->c->a->b->c->a
      assertEqual(cursor.length, 13);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[1]._id, a); // We somehow return to a
      assertEqual(cursor[2]._id, a); // We somehow return to a again
      assertEqual(cursor[3]._id, a); // We somehow return to a again
      assertEqual(cursor[4]._id, b); // We once find b
      assertEqual(cursor[5]._id, b); // We find b again
      assertEqual(cursor[6]._id, b); // And b again
      assertEqual(cursor[7]._id, c); // And once c
      assertEqual(cursor[8]._id, c); // We find c again
      assertEqual(cursor[9]._id, c); // And c again
      assertEqual(cursor[10]._id, d); // Short Path d
      assertEqual(cursor[11]._id, d); // One Loop d
      assertEqual(cursor[12]._id, d); // Second Loop d
    },

    testVertexUniquenessNone: function () {
      var start = vc.save({_key: 's'})._id;
      var a = vc.save({_key: 'a'})._id;
      var b = vc.save({_key: 'b'})._id;
      var c = vc.save({_key: 'c'})._id;
      var d = vc.save({_key: 'd'})._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(b, c, {});
      ec.save(c, a, {});
      ec.save(a, d, {});
      var cursor = db._query(
        `WITH ${vn}
        FOR v IN 1..10 OUTBOUND '${start}' ${en} OPTIONS {uniqueVertices: 'none'}
        SORT v._key
        RETURN v`).toArray();
      // We expect to get s->a->b->c->a->d
      // and s->a->d
      // But not s->a->b->c->a->b->*
      // And not to continue at a again

      // Default edge Uniqueness is path
      assertEqual(cursor.length, 6);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[1]._id, a); // We somehow return to a
      assertEqual(cursor[2]._id, b); // We once find b
      assertEqual(cursor[3]._id, c); // And once c
      assertEqual(cursor[4]._id, d); // We once find d on short path
      assertEqual(cursor[5]._id, d); // And find d on long path
    },

    testVertexUniquenessGlobalDepthFirst: function () {
      var start = vc.save({_key: 's'})._id;
      try {
        db._query(
          `WITH ${vn}
          FOR v IN 1..10 OUTBOUND '${start}' ${en} OPTIONS {uniqueVertices: 'global'}
          SORT v._key
          RETURN v`).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_BAD_PARAMETER.code, 'We expect a bad parameter');
      }
    },

    testVertexUniquenessPath: function () {
      var start = vc.save({_key: 's'})._id;
      var a = vc.save({_key: 'a'})._id;
      var b = vc.save({_key: 'b'})._id;
      var c = vc.save({_key: 'c'})._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(a, a, {});
      ec.save(b, c, {});
      ec.save(b, a, {});
      ec.save(c, a, {});
      var cursor = db._query(
        `WITH ${vn}
        FOR v IN 1..10 OUTBOUND '${start}' ${en} OPTIONS {uniqueVertices: 'path'}
        SORT v._key
        RETURN v`).toArray();
      // We expect to get s->a->b->c
      // But not s->a->a*
      // But not s->a->b->a*
      // But not s->a->b->c->a*
      assertEqual(cursor.length, 3);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[1]._id, b); // We find a->b
      assertEqual(cursor[2]._id, c); // We find a->b->c
    }
  };
}

jsunity.run(optionsSuite);
return jsunity.done();
