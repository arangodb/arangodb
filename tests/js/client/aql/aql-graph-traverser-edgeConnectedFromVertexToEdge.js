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
const isCluster = require("internal").isCluster();
const errors = require('@arangodb').errors;
const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';

const gh = require('@arangodb/graph/helpers');

/*
 *       A
 *     ↙ ↲ ↘  // Edge from A to the edge connecting "A->B"
 *   B       C
 */
function edgeConnectedFromVertexToEdge() {
  const gn = 'UnitTestGraph';
  const gn2 = 'UnitTestGraph2';
  const en2 = 'UnitTestEdgeCollection2';
  var vertex = {};
  var edge = {};
  var vc, ec;

  return {
    setUpAll: function () {
      gh.cleanup();

      vc = db._createDocumentCollection(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      let ec2 = db._createEdgeCollection(en2, {numberOfShards: 4});

      vertex.A = vc.save({_key: 'A'})._id;
      vertex.B = vc.save({_key: 'B'})._id;
      vertex.C = vc.save({_key: 'C'})._id;

      // collections and directions
      edge.AB = ec.save(vertex.A, vertex.B, {_key: 'AB'});
      edge.AC = ec.save(vertex.A, vertex.C, {_key: 'AC'});
      edge.AAB = ec.save(vertex.A, edge.AB, {_key: 'AAB'}); // Edge from A to the edge connecting "A->B"

      edge.AB = ec2.save(vertex.A, vertex.B, {_key: 'AB'});
      edge.AC = ec2.save(vertex.A, vertex.C, {_key: 'AC'});
      edge.AAB = ec2.save(vertex.A, edge.AB, {_key: 'AAB'}); // Edge from A to the edge connecting "A->B"

      // also create a named graph for testing as well
      try {
        gm._drop(gn, true);
        if (isCluster) {
          gm._drop(gn2, true);
        }
      } catch (e) {
        // It is expected that those graphs are not existing.
      }
      gm._create(gn, [gm._relation(en, vn, [vn, en])]); // complete definition
      if (isCluster) {
        gm._create(gn2, [gm._relation(en2, vn, vn)]); // en collection is missing
      }
    },

    tearDownAll: function () {
      gm._drop(gn, true);
      if (isCluster) {
        gm._drop(gn2, true);
      }
      gh.cleanup();
      db._drop(en2);
    },

    testConnectedEdgeToAnotherEdge: function () {
      let queries = [
        `WITH ${vn}, ${en} FOR x,y,z IN 1..10 OUTBOUND @start @@ec return x`,
        `FOR x,y,z IN 1..10 OUTBOUND @start GRAPH "${gn}" return x`,
        `FOR x,y,z IN 1..10 OUTBOUND @start GRAPH "${gn2}" return x`
      ];

      let bindVarsAnonymous = {
        '@ec': en,
        start: vertex.A
      };

      let bindVarsNamed = {
        start: vertex.A
      };

      let checkFoundVertices = function(result) {
        // sort returned array
        result = _.orderBy(result, ['_key'],['asc']);
        assertEqual(result[0]._key, 'AB');
        assertEqual(result[1]._key, 'B');
        assertEqual(result[2]._key, 'C');
        assertEqual(result.length, 3);
      };

      // must work
      checkFoundVertices(db._query(queries[0], bindVarsAnonymous).toArray()); // anonymous
      checkFoundVertices(db._query(queries[1], bindVarsNamed).toArray()); // named, all collections well defined

      if (isCluster) {
        try {
          // must fail
          db._query(queries[2], bindVarsNamed).toArray(); // en collection is missing in "to"
          fail();
        } catch (e) {
          // TODO: In the future create a better error message. Named graphs cannot work with the
          // "WITH" statement. But currently it is reported to the user as a solution within th error
          // message. This is false and needs to be fixed.
          assertEqual(e.errorNum, errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code);
        }
      }
    }
  };
}
jsunity.run(edgeConnectedFromVertexToEdge);
return jsunity.done();
