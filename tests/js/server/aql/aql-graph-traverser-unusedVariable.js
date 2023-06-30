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


const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';

function unusedVariableSuite() {
  const gn = 'UnitTestGraph';

  return {

    setUpAll: function () {
      db._drop(gn + 'v');
      db._drop(gn + 'e');

      let c = db._create(gn + 'v');
      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({_key: 'test' + i});
      }
      c.insert(docs);

      c = db._createEdgeCollection(gn + 'e');
      docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({_from: gn + 'v/test' + i, _to: gn + 'v/test' + (i+1)});
      }
      c.insert(docs);
    },

    tearDownAll: function () {
      db._drop(gn + 'v');
      db._drop(gn + 'e');
    },

    testCount: function () {
      const queries = [
        [ 'WITH @@vertices,@@edges FOR v IN 1..1 OUTBOUND @start @@edges RETURN 1', 1],
        [ 'WITH @@vertices,@@edges FOR v IN 1..100 OUTBOUND @start @@edges RETURN 1', 100],
        [ 'WITH @@vertices,@@edges FOR v IN 1..1000 OUTBOUND @start @@edges RETURN 1', 1000],
        [ 'WITH @@vertices,@@edges FOR v,e IN 1..1 OUTBOUND @start @@edges RETURN 1', 1],
        [ 'WITH @@vertices,@@edges FOR v,e IN 1..100 OUTBOUND @start @@edges RETURN 1', 100],
        [ 'WITH @@vertices,@@edges FOR v,e IN 1..1000 OUTBOUND @start @@edges RETURN 1', 1000],
        [ 'WITH @@vertices,@@edges FOR v,e,p IN 1..1 OUTBOUND @start @@edges RETURN 1', 1],
        [ 'WITH @@vertices,@@edges FOR v,e,p IN 1..100 OUTBOUND @start @@edges RETURN 1', 100],
        [ 'WITH @@vertices,@@edges FOR v,e,p IN 1..1000 OUTBOUND @start @@edges RETURN 1', 1000],
        [ 'WITH @@vertices,@@edges FOR v,e,p IN 1..1 OUTBOUND @start @@edges RETURN v', 1],
        [ 'WITH @@vertices,@@edges FOR v,e,p IN 1..100 OUTBOUND @start @@edges RETURN v', 100],
        [ 'WITH @@vertices,@@edges FOR v,e,p IN 1..1000 OUTBOUND @start @@edges RETURN v', 1000],
      ];

      queries.forEach(function (query) {
        const r = db._query(query[0], {"start": gn + 'v/test0', "@vertices": gn+'v', "@edges": gn+'e'});
        const resultArray = r.toArray();
        assertEqual(query[1], resultArray.length, query);
      });

    },

    testCountSubquery: function () {
      let queries = [];
      if (isCluster) {
        // [GraphRefactor] Note: Related to #GORDO-1360
        queries = [
          [ `WITH ${gn}v RETURN COUNT(FOR v IN 1..1 OUTBOUND @start @@edges RETURN 1)`, 1],
          [ `WITH ${gn}v RETURN COUNT(FOR v IN 1..100 OUTBOUND @start @@edges RETURN 1)`, 100],
          [ `WITH ${gn}v RETURN COUNT(FOR v IN 1..1000 OUTBOUND @start @@edges RETURN 1)`, 1000],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e IN 1..1 OUTBOUND @start @@edges RETURN 1)`, 1],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e IN 1..100 OUTBOUND @start @@edges RETURN 1)`, 100],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e IN 1..1000 OUTBOUND @start @@edges RETURN 1)`, 1000],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e,p IN 1..1 OUTBOUND @start @@edges RETURN 1)`, 1],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e,p IN 1..100 OUTBOUND @start @@edges RETURN 1)`, 100],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e,p IN 1..1000 OUTBOUND @start @@edges RETURN 1)`, 1000],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e,p IN 1..1 OUTBOUND @start @@edges RETURN v)`, 1],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e,p IN 1..100 OUTBOUND @start @@edges RETURN v)`, 100],
          [ `WITH ${gn}v RETURN COUNT(FOR v,e,p IN 1..1000 OUTBOUND @start @@edges RETURN v)`, 1000],
        ];
      } else {
        queries = [
          [ 'RETURN COUNT(FOR v IN 1..1 OUTBOUND @start @@edges RETURN 1)', 1],
          [ 'RETURN COUNT(FOR v IN 1..100 OUTBOUND @start @@edges RETURN 1)', 100],
          [ 'RETURN COUNT(FOR v IN 1..1000 OUTBOUND @start @@edges RETURN 1)', 1000],
          [ 'RETURN COUNT(FOR v,e IN 1..1 OUTBOUND @start @@edges RETURN 1)', 1],
          [ 'RETURN COUNT(FOR v,e IN 1..100 OUTBOUND @start @@edges RETURN 1)', 100],
          [ 'RETURN COUNT(FOR v,e IN 1..1000 OUTBOUND @start @@edges RETURN 1)', 1000],
          [ 'RETURN COUNT(FOR v,e,p IN 1..1 OUTBOUND @start @@edges RETURN 1)', 1],
          [ 'RETURN COUNT(FOR v,e,p IN 1..100 OUTBOUND @start @@edges RETURN 1)', 100],
          [ 'RETURN COUNT(FOR v,e,p IN 1..1000 OUTBOUND @start @@edges RETURN 1)', 1000],
          [ 'RETURN COUNT(FOR v,e,p IN 1..1 OUTBOUND @start @@edges RETURN v)', 1],
          [ 'RETURN COUNT(FOR v,e,p IN 1..100 OUTBOUND @start @@edges RETURN v)', 100],
          [ 'RETURN COUNT(FOR v,e,p IN 1..1000 OUTBOUND @start @@edges RETURN v)', 1000],
        ]; 
      }

      queries.forEach(function (query) {
        const r = db._query(query[0], {"start": gn + 'v/test0', "@edges": gn+'e'});
        const resultArray = r.toArray();
        assertEqual(resultArray.length, 1);
        assertEqual(query[1], resultArray[0], query);
      });
    },
  };
}

jsunity.run(unusedVariableSuite);
return jsunity.done();
