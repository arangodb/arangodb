/*jshint esnext: true */
/*global assertEqual, fail, AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON */

////////////////////////////////////////////////////////////////////////////////
/// @brief Spec for the AQL FOR x IN GRAPH name statement
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

"use strict";

const jsunity = require("jsunity");

const internal = require("internal");
const db = internal.db;
const errors = require("@arangodb").errors;
const gm = require("@arangodb/general-graph");
const vn = "UnitTestVertexCollection";
const en = "UnitTestEdgeCollection";
const isCluster = require("@arangodb/cluster").isCluster();
var _ = require("lodash");
var vertex = {};
var edge = {};
var vc;
var ec;

var cleanup = function () {
  db._drop(vn);
  db._drop(en);
  vertex = {};
  edge = {};
};

function optimizeNonVertexCentricIndexesSuite () {

  let explain = function (query, params) {
    return AQL_EXPLAIN(query, params, { optimizer: { rules: [ "+all" ] } });
  };

  let vertices = {};
  let edges = {};

  return {
    setUpAll: () => {
      cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      vertices.A = vc.save({_key: "A"})._id;
      vertices.B = vc.save({_key: "B"})._id;
      vertices.C = vc.save({_key: "C"})._id;
      vertices.D = vc.save({_key: "D"})._id;
      vertices.E = vc.save({_key: "E"})._id;
      vertices.F = vc.save({_key: "F"})._id;
      vertices.G = vc.save({_key: "G"})._id;

      vertices.FOO = vc.save({_key: "FOO"})._id;
      vertices.BAR = vc.save({_key: "BAR"})._id;

      edges.AB = ec.save({_key: "AB", _from: vertices.A, _to: vertices.B, foo: "A", bar: true})._id;
      edges.BC = ec.save({_key: "BC", _from: vertices.B, _to: vertices.C, foo: "B", bar: true})._id;
      edges.BD = ec.save({_key: "BD", _from: vertices.B, _to: vertices.D, foo: "C", bar: false})._id;
      edges.AE = ec.save({_key: "AE", _from: vertices.A, _to: vertices.E, foo: "D", bar: true})._id;
      edges.EF = ec.save({_key: "EF", _from: vertices.E, _to: vertices.F, foo: "E", bar: true})._id;
      edges.EG = ec.save({_key: "EG", _from: vertices.E, _to: vertices.G, foo: "F", bar: false})._id;


      // Adding these edges to make the estimate for the edge-index extremly bad
      let badEdges = [];
      for (let j = 0; j < 1000; ++j) {
        badEdges.push({_from: vertices.FOO, _to: vertices.BAR, foo: "foo" + j, bar: j});
      }
      ec.save(badEdges);
    },

    tearDownAll: cleanup,

    tearDown: () => {
      // After each test get rid of all superflous indexes.
      var idxs = db[en].getIndexes();
      for (let i = 2; i < idxs.length; ++i) {
        db[en].dropIndex(idxs[i].id);
      }
    },


    testHashIndex : () => {
      var idx = db[en].ensureIndex({type: "hash", fields: ["foo"], unique: false, sparse: false});
      // This index is assumed to be better than edge-index, but does not contain _from/_to
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[0].foo == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.levels["0"];
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
    },

    testSkiplistIndex : () => {
      var idx = db[en].ensureIndex({type: "skiplist", fields: ["foo"], unique: false, sparse: false});
      // This index is assumed to be better than edge-index, but does not contain _from/_to
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[0].foo == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.levels["0"];
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
    },


    testAllHashIndex : () => {
      var idx = db[en].ensureIndex({type: "hash", fields: ["foo"], unique: false, sparse: false});
      // This index is assumed to be better than edge-index, but does not contain _from/_to
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[*].foo ALL == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.base;
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
    },

    testAllSkiplistIndex : () => {
      var idx = db[en].ensureIndex({type: "skiplist", fields: ["foo"], unique: false, sparse: false});
      // This index is assumed to be better than edge-index, but does not contain _from/_to
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[*].foo ALL == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.base;
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
    },

    testAllSkiplistIncompleteIndex : () => {
      var idx = db[en].ensureIndex({type: "skiplist", fields: ["foo", "unknown", "_from"], unique: false, sparse: false});
      // This index is assumed to be better than edge-index, it does contain _from, but cannot use it.
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[*].foo ALL == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.base;
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
    }

  };
};

if (!isCluster) {
  jsunity.run(optimizeNonVertexCentricIndexesSuite);
}

return jsunity.done();
