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
const errors = require('@arangodb').errors;

const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';



const gh = require('@arangodb/graph/helpers');

function multiEdgeDirectionSuite() {
  const en2 = 'UnitTestEdgeCollection2';
  var ec2;
  var vertex = {};
  var edges = {};
  var ec, vc;

  return {

    setUpAll: function () {
      gh.cleanup();
      db._drop(en2);

      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      ec2 = db._createEdgeCollection(en2, {numberOfShards: 4});

      vertex.A = vc.save({_key: 'A'})._id;
      vertex.B = vc.save({_key: 'B'})._id;
      vertex.C = vc.save({_key: 'C'})._id;
      vertex.D = vc.save({_key: 'D'})._id;
      vertex.E = vc.save({_key: 'E'})._id;
      vertex.F = vc.save({_key: 'F'})._id;

      // F is always 2 hops away and only reachable with alternating
      // collections and directions

      ec.save(vertex.A, vertex.B, {});
      ec.save(vertex.C, vertex.A, {});
      ec2.save(vertex.A, vertex.D, {});
      ec2.save(vertex.E, vertex.A, {});

      ec2.save(vertex.F, vertex.B, {});
      ec2.save(vertex.C, vertex.F, {});

      ec.save(vertex.F, vertex.D, {});
      ec.save(vertex.E, vertex.F, {});
    },

    tearDownAll: function () {
      gh.cleanup();
      db._drop(en2);
    },

    testOverrideOneDirection: function () {
      var queries = [
        {
          q1: `WITH ${vn} FOR x IN ANY @start @@ec1, INBOUND @@ec2 SORT x._key RETURN x._id`,
          q2: `WITH ${vn} FOR x IN ANY @start ${en}, INBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.C, vertex.E]
        },
        {
          q1: `WITH ${vn} FOR x IN ANY @start @@ec1, OUTBOUND @@ec2 SORT x._key RETURN x._id`,
          q2: `WITH ${vn} FOR x IN ANY @start ${en}, OUTBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.C, vertex.D]
        },
        {
          q1: `WITH ${vn} FOR x IN ANY @start INBOUND @@ec1, @@ec2 SORT x._key RETURN x._id`,
          q2: `WITH ${vn} FOR x IN ANY @start INBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D, vertex.E]
        },
        {
          q1: `WITH ${vn} FOR x IN ANY @start OUTBOUND @@ec1, @@ec2 SORT x._key RETURN x._id`,
          q2: `WITH ${vn} FOR x IN ANY @start OUTBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.D, vertex.E]
        },
        {
          q1: `WITH ${vn} FOR x IN OUTBOUND @start INBOUND @@ec1, @@ec2 SORT x._key RETURN x._id`,
          q2: `WITH ${vn} FOR x IN OUTBOUND @start INBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D]
        },
        {
          q1: `WITH ${vn} FOR x IN OUTBOUND @start @@ec1, INBOUND @@ec2 SORT x._key RETURN x._id`,
          q2: `WITH ${vn} FOR x IN OUTBOUND @start ${en}, INBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.E]
        },
        {
          q1: `WITH ${vn} FOR x IN INBOUND @start @@ec1, OUTBOUND @@ec2 SORT x._key RETURN x._id`,
          q2: `WITH ${vn} FOR x IN INBOUND @start ${en}, OUTBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D]
        },
        {
          q1: `WITH ${vn} FOR x IN INBOUND @start OUTBOUND @@ec1, @@ec2 SORT x._key RETURN x._id`,
          q2: `WITH ${vn} FOR x IN INBOUND @start OUTBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.E]
        }
      ];

      var bindVars = {
        '@ec1': en,
        '@ec2': en2,
        start: vertex.A
      };
      var bindVars2 = {
        start: vertex.A
      };
      queries.forEach(function (item) {
        var result = db._query(item.q1, bindVars).toArray();
        assertEqual(result, item.res);
        result = db._query(item.q2, bindVars2).toArray();
        assertEqual(result, item.res);
      });
    },

    testDuplicationCollections: function () {
      var queries = [
        [`WITH ${vn} FOR x IN ANY @start @@ec, INBOUND @@ec RETURN x`, false],
        [`WITH ${vn} FOR x IN ANY @start @@ec, OUTBOUND @@ec RETURN x`, false],
        [`WITH ${vn} FOR x IN ANY @start @@ec, ANY @@ec RETURN x`, true],
        [`WITH ${vn} FOR x IN INBOUND @start @@ec, INBOUND @@ec RETURN x`, true],
        [`WITH ${vn} FOR x IN INBOUND @start @@ec, OUTBOUND @@ec RETURN x`, false],
        [`WITH ${vn} FOR x IN INBOUND @start @@ec, ANY @@ec RETURN x`, false],
        [`WITH ${vn} FOR x IN OUTBOUND @start @@ec, INBOUND @@ec RETURN x`, false],
        [`WITH ${vn} FOR x IN OUTBOUND @start @@ec, OUTBOUND @@ec RETURN x`, true],
        [`WITH ${vn} FOR x IN OUTBOUND @start @@ec, ANY @@ec RETURN x`, false]
      ];

      var bindVars = {
        '@ec': en,
        start: vertex.A
      };
      queries.forEach(function (query) {
        if (query[1]) {
          // should work
          db._query(query[0], bindVars).toArray();
        } else {
          // should fail
          try {
            db._query(query[0], bindVars).toArray();
            fail();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code);
          }
        }
      });
    }
  };
}

jsunity.run(multiEdgeDirectionSuite);
return jsunity.done();
