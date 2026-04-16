/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, print, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////
"use strict";

const jsunity = require("jsunity");
const {aql, db, errors} = require("@arangodb");




function aqlMatchStatementVariableLengthTestSuite() {
  const validatePath = function(path) {
    for(let i = 0; i < path.edges.length; i++) {
      assertEqual(path.vertices[i]._id, path.edges[i]._from);
      assertEqual(path.vertices[i+1]._id, path.edges[i]._to);
    }
  };

  const pathToString = function(path) {
    return path.vertices.map((v) => `(${v._id})`).join(" -[]-> ");
  };

  const projectPath = function(path) {
    return { edges: path.edges.map((edge) => [edge._from, edge._to]),
             vertices: path.vertices.map((vertex) => vertex._id) };
  };
  const setEq = function(s1, s2) {
    return s1.size == s2.size && [...s1].every((x) => s2.has(x));
  } 

    const database = "UnitTestsAqlMatchStatementVariableLength";
    const options = { matchStatement: "experimental" };

    return {

        setUpAll: function () {
            db._createDatabase(database);
            db._useDatabase(database);

            db._create("vc1");
            for (let i = 0; i < 4; i++) {
                db.vc1.save({_key: `v${i}`});
            }
            db._create("vc2");
            for (let i = 0; i < 4; i++) {
              db.vc2.save({_key: `v${i}`});
            }
            db._create("vc3");
            for (let i = 0; i < 4; i++) {
              db.vc3.save({_key: `v${i}`});
            }

            db._createEdgeCollection("ec1");
            for (let i = 0; i < 3; i++) {
              db.ec1.save({_from: `vc1/v${i}`, _to: `vc1/v${i+1}`});
              db.ec1.save({_from: `vc2/v${i}`, _to: `vc2/v${i+1}`}); 
              db.ec1.save({_from: `vc3/v${i}`, _to: `vc3/v${i+1}`}); 
            }
            for (let i = 0; i < 4; i++) {
              db.ec1.save({_from: `vc1/v${i}`, _to: `vc2/v${i}`});
              db.ec1.save({_from: `vc2/v${i}`, _to: `vc3/v${i}`});
            }
       },

        tearDownAll: function () {
            db._useDatabase("_system");
            db._dropDatabase(database);
        },

        testMatchVariableLengthPathVariableEnd: function() {
          const query = aql`WITH vc1, vc2, vc3
                              FOR v IN ["vc1/v0"]
                                FOR w IN vc3
                                  MATCH (v) -[ e:ec1 * 1..2 ]-> (w)
                                  RETURN [v, e, w]`;

          const expected = [ 
              "(vc1/v0) -[]-> (vc2/v0) -[]-> (vc3/v0)" 
          ]; 
          expected.sort();

          const result = db._query(query, {}, options)
            .toArray()
            .map((x) => pathToString(x[1]));
          result.sort();
          assertEqual(result, expected);
        },


        testMatchVariableLengthPathCollectionEnd: function() {
          const query = aql`WITH vc1, vc2, vc3
                              FOR v IN ["vc1/v0"]
                                MATCH (v) -[ e:ec1 * 1..2 ]-> (w:vc1)
                                RETURN [v, e, w]`;
          const expected = [ 
            "(vc1/v0) -[]-> (vc1/v1)",
            "(vc1/v0) -[]-> (vc1/v1) -[]-> (vc1/v2)"
          ];
          expected.sort();

          const result = db._query(query, {}, options)
            .toArray()
            .map((x) => pathToString(x[1]));
          result.sort();
          assertEqual(result, expected);
        },

        testMatchVariableLengthPathWithPWithVariableEnd: function() {
          const query = aql`WITH vc1, vc2, vc3
                              FOR v IN ["vc1/v0"]
                                FOR w IN vc3
                                  MATCH p = (v) -[ e:ec1 * 2..3 ]-> (w)
                                  RETURN p`;

          var expected =
            [ "(vc1/v0) -[]-> (vc2/v0) -[]-> (vc3/v0)", 
              "(vc1/v0) -[]-> (vc2/v0) -[]-> (vc3/v0) -[]-> (vc3/v1)", 
              "(vc1/v0) -[]-> (vc2/v0) -[]-> (vc2/v1) -[]-> (vc3/v1)",  
              "(vc1/v0) -[]-> (vc1/v1) -[]-> (vc2/v1) -[]-> (vc3/v1)" ];
          expected.sort();


          const result = db._query(query, {}, options)
                           .toArray()
                           .map(pathToString);
          result.sort();

          assertEqual(result, expected);
        },
        testMatchVariableLengthPathWithPWithCollectionEnd: function() {
          const query = aql`WITH vc1, vc2, vc3
                              FOR v IN ["vc1/v0"]
                                MATCH p = (v) -[ e:ec1 * 2..3 ]-> (w:vc1)
                                  RETURN p`;

          const expected = [
            "(vc1/v0) -[]-> (vc1/v1) -[]-> (vc1/v2)", 
            "(vc1/v0) -[]-> (vc1/v1) -[]-> (vc1/v2) -[]-> (vc1/v3)"
          ];
          expected.sort();
 
          const result = db._query(query, {}, options)
            .toArray()
            .map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },
        testMatchVariableLengthPathWithPComposes: function() {
          const query = aql`WITH vc1, vc2, vc3
                              FOR v IN [{_id: "vc1/v0"}]
                                MATCH p = (v) -[ e1:ec1 ]-> (w:vc1)
                                              -[ e2:ec1 * 1..2 ]-> (u:vc3)
                                RETURN p`;
          const expected = [
            "(vc1/v0) -[]-> (vc1/v1) -[]-> (vc2/v1) -[]-> (vc3/v1)"
          ];
          expected.sort();

          const result = db._query(query, {}, options)
            .toArray()
            .map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },
        testMatchVariableLengthPathWithPComposesBack: function() {
          const query = aql`WITH vc1, vc2, vc3
                              FOR v IN [{_id: "vc1/v0"}]
                                MATCH p = (v) -[ e2:ec1 * 1..2 ]-> (u:vc3)
                                              -[ e3:ec1 ]-> (w:vc3)
                                  RETURN p`;
          const expected = [
            "(vc1/v0) -[]-> (vc2/v0) -[]-> (vc3/v0) -[]-> (vc3/v1)"  
          ];
          expected.sort();

          const result = db._query(query, {}, options)
            .toArray()
            .map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },
        testMatchVariableLengthInbound: function() {
          const query = aql`WITH vc1, vc2, vc3
                              FOR v IN ["vc3/v3"]
                                FOR w IN vc3
                                  MATCH p = (v) <-[ e:ec1 * 1..2 ]- (w)
                                  RETURN p`;

          const expected = [
            "(vc3/v3) -[]-> (vc3/v2) -[]-> (vc3/v1)",  
            "(vc3/v3) -[]-> (vc3/v2)"
          ]; 
          expected.sort();

          const result = db._query(query, {}, options)
            .toArray()
            .map(pathToString);
          result.sort();

          assertEqual(result, expected);
        },
        testMatchVariableLengthAny: function() {
          const query = aql`WITH vc1, vc2, vc3
                              FOR v IN ["vc3/v3"]
                                FOR w IN vc3
                                  MATCH p = (v) -[ e:ec1 * 1..2 ]- (w)
                                  RETURN p`;

          const expected = [
            "(vc3/v3) -[]-> (vc3/v2) -[]-> (vc3/v1)",
            "(vc3/v3) -[]-> (vc3/v2)"
          ]; 
          expected.sort();

          const result = db._query(query, {}, options)
            .toArray()
            .map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },

    };
}

jsunity.run(aqlMatchStatementVariableLengthTestSuite);

return jsunity.done();



