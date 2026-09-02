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
  const pathToString = function(path) {
    return path.vertices.map((v) => `(${v._id})`).join(" -[]-> ");
  };

  const setEq = function(s1, s2) {
    return s1.size === s2.size && [...s1].every((x) => s2.has(x));
  };

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

            // Self-contained vertex/edge collections used by the collection
            // bind-parameter tests. Their edges never leave `vcbp`, so the
            // generated traversal never reaches an undeclared collection and the
            // queries do not require an explicit WITH (which a pure MATCH using
            // bind parameters cannot express). This keeps the tests valid on a
            // cluster, where traversals must know every reachable collection.
            db._create("vcbp");
            for (let i = 0; i < 4; i++) {
              db.vcbp.save({_key: `v${i}`});
            }
            db._createEdgeCollection("ecbp");
            for (let i = 0; i < 3; i++) {
              db.ecbp.save({_from: `vcbp/v${i}`, _to: `vcbp/v${i+1}`});
            }

            // Isolated dataset for multiple-edge-type tests.
            db._create("mvc");
            for (let i = 0; i < 4; i++) {
              db.mvc.save({_key: `v${i}`});
            }
            db._createEdgeCollection("mec1");
            db.mec1.save({_from: "mvc/v0", _to: "mvc/v1"});
            db.mec1.save({_from: "mvc/v1", _to: "mvc/v2"});
            db.mec1.save({_from: "mvc/v2", _to: "mvc/v3"});
            db._createEdgeCollection("mec2");
            db.mec2.save({_from: "mvc/v0", _to: "mvc/v2"});
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
        testMatchVariableLengthCollectionBindParameters: function() {
          const query = "MATCH (v :@@vc) -[ e : @@ec * 1..1 ]-> (w :@@vc) RETURN [v, e, w]";
          const expected = [
            "(vcbp/v0) -[]-> (vcbp/v1)",
            "(vcbp/v1) -[]-> (vcbp/v2)",
            "(vcbp/v2) -[]-> (vcbp/v3)"
          ];
          expected.sort();

          const result = db._query(query, { "@vc": "vcbp", "@ec": "ecbp" }, options)
            .toArray()
            .map((x) => pathToString(x[1]));
          result.sort();
          assertEqual(result, expected);
        },
        testMatchVariableLengthValueBindParameterRejected: function() {
          // a value bind parameter (@name) cannot denote a collection / edge type;
          // only a collection bind parameter (@@name) is accepted. parsing fails.
          try {
            db._query("MATCH (v :vc1) -[ e : @ec * 1..1 ]-> (w :vc1) RETURN [v, e, w]",
                      { ec: "ec1" }, options).toArray();
            fail();
          } catch (err) {
            assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
        },

        testMatchVariableLengthValueBindParameterMissing: function() {
          try {
            db._query("MATCH (v :@@vc1) -[ e : @@ec * 1..1 ]-> (w :vc1) RETURN [v, e, w]",
                      { "@ec": "ec1" }, options).toArray();
            fail();
          } catch (err) {
            assertEqual(err.errorNum, errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code);
          }
        },

        testCollectionBindParameterUsesNonexistentCollection : function () {
          try {
            db._query("MATCH (v :@@vc) RETURN COUNT(v)", { "@vc": "someOtherCollection" },
                      options).toArray();
            fail();
          } catch (err) {
            assertEqual(err.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
          }
        },

        testMatchVariableLengthDataSourceBindParameterCollections: function() {
          const query = "MATCH (v :@@vc) -[ e : @@ec * 1..2 ]-> (w :@@vc) RETURN [v, e, w]";
          const expected = [
            "(vcbp/v0) -[]-> (vcbp/v1)",
            "(vcbp/v0) -[]-> (vcbp/v1) -[]-> (vcbp/v2)",
            "(vcbp/v1) -[]-> (vcbp/v2)",
            "(vcbp/v1) -[]-> (vcbp/v2) -[]-> (vcbp/v3)",
            "(vcbp/v2) -[]-> (vcbp/v3)"
          ];
          expected.sort();

          const result = db._query(query, { "@vc": "vcbp", "@ec": "ecbp" }, options)
            .toArray()
            .map((x) => pathToString(x[1]));
          result.sort();
          assertEqual(result, expected);
        },

        // Multiple edge types combined with variable length
        testMatchVarLenMultiTypeOutbound: function() {
          // e is a PATH object
          const query = aql`WITH mvc
                              FOR v IN mvc
                                MATCH (v) -[ e:mec1|mec2 * 1..2 ]-> (w:mvc)
                                RETURN [v, e, w]`;
          const expected = [
            "(mvc/v0) -[]-> (mvc/v1)",
            "(mvc/v0) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v2) -[]-> (mvc/v3)",
            "(mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v1) -[]-> (mvc/v2) -[]-> (mvc/v3)",
            "(mvc/v2) -[]-> (mvc/v3)"
          ];
          expected.sort();
          const result = db._query(query, {}, options)
            .toArray().map((x) => pathToString(x[1]));
          result.sort();
          assertEqual(result, expected);
        },

        testMatchVarLenMultiTypeMixedCollectionPath: function() {
          // unique 2-hop path v0->v2->v3 must use an mec2 edge then an mec1 edge
          const query = aql`WITH mvc
                              FOR v IN ["mvc/v0"]
                                MATCH (v) -[ e:mec1|mec2 * 2..2 ]-> (w:mvc)
                                FILTER w._id == "mvc/v3"
                                RETURN e.edges[*]._id`;
          const result = db._query(query, {}, options).toArray();
          assertEqual(result.length, 1);
          const edgeIds = result[0];
          assertEqual(edgeIds.length, 2);
          assertTrue(edgeIds[0].startsWith("mec2/"), edgeIds[0]);
          assertTrue(edgeIds[1].startsWith("mec1/"), edgeIds[1]);
        },

        testMatchVarLenMultiTypeInbound: function() {
          const query = aql`WITH mvc
                              FOR v IN mvc
                                MATCH (v) <-[ e:mec1|mec2 * 1..2 ]- (w:mvc)
                                RETURN 1`;
          const result = db._query(query, {}, options).toArray();
          assertEqual(result.length, 7);
        },

        testMatchVarLenMultiTypeAny: function() {
          const query = aql`WITH mvc
                              FOR v IN mvc
                                MATCH (v) -[ e:mec1|mec2 * 1..2 ]- (w:mvc)
                                RETURN e.edges[*]._id`;
          const result = db._query(query, {}, options).toArray();
          assertTrue(result.length > 0);
          const usesMec2 = result.some(
            (edgeIds) => edgeIds.some((id) => id.startsWith("mec2/")));
          assertTrue(usesMec2, "expected at least one path to use an mec2 edge");
        },

        testMatchVarLenMultiTypeBindParams: function() {
          const query = "MATCH (v :mvc) -[ e :@@ec1 | @@ec2 * 1..2 ]-> (w :mvc) RETURN [v, e, w]";
          const expected = [
            "(mvc/v0) -[]-> (mvc/v1)",
            "(mvc/v0) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v2) -[]-> (mvc/v3)",
            "(mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v1) -[]-> (mvc/v2) -[]-> (mvc/v3)",
            "(mvc/v2) -[]-> (mvc/v3)"
          ];
          expected.sort();
          const result = db._query(query, {"@ec1": "mec1", "@ec2": "mec2"}, options)
            .toArray().map((x) => pathToString(x[1]));
          result.sort();
          assertEqual(result, expected);
        },

        testMatchVarLenMultiTypeSeamIsPath: function() {
          // explicit *1..1 still binds e to a PATH object (not an edge doc)
          const query = aql`WITH mvc
                              FOR v IN mvc
                                MATCH (v) -[ e:mec1|mec2 * 1..1 ]-> (w:mvc)
                                RETURN e`;
          const result = db._query(query, {}, options).toArray();
          assertEqual(result.length, 4); // v0->v1, v1->v2, v2->v3 (mec1) + v0->v2 (mec2)
          for (const e of result) {
            assertTrue(e.hasOwnProperty("edges") && e.hasOwnProperty("vertices"),
                       JSON.stringify(e));
          }
        },

        testMatchVarLenMultiTypePathVariable: function() {
          const query = aql`WITH mvc
                              FOR v IN mvc
                                MATCH p = (v) -[ e:mec1|mec2 * 1..2 ]-> (w:mvc)
                                RETURN p`;
          const expected = [
            "(mvc/v0) -[]-> (mvc/v1)",
            "(mvc/v0) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v2) -[]-> (mvc/v3)",
            "(mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v1) -[]-> (mvc/v2) -[]-> (mvc/v3)",
            "(mvc/v2) -[]-> (mvc/v3)"
          ];
          expected.sort();
          const result = db._query(query, {}, options).toArray().map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },

        // A variable-length segment is always lowered to a traversal, so the
        // target vertex's constraints have to be re-applied on the traversal's
        // vertex output. Unlike the one-hop case there is no alternative
        // lowering to cross-check against, so the expected paths are spelled
        // out: mec1 is v0->v1->v2->v3 and mec2 is the single shortcut v0->v2.
        testMatchVarLenTargetVertexProperties: function() {
          const query = aql`MATCH (v :mvc) -[ e:mec1 * 1..2 ]-> (w :mvc {_key: "v2"})
                              RETURN e`;
          const expected = [
            "(mvc/v0) -[]-> (mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v1) -[]-> (mvc/v2)"
          ];
          expected.sort();
          const result = db._query(query, {}, options).toArray().map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },

        testMatchVarLenTargetVertexWhereClause: function() {
          const query = aql`MATCH (v :mvc) -[ e:mec1 * 1..2 ]-> (w :mvc WHERE w._key == "v2")
                              RETURN e`;
          const expected = [
            "(mvc/v0) -[]-> (mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v1) -[]-> (mvc/v2)"
          ];
          expected.sort();
          const result = db._query(query, {}, options).toArray().map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },

        testMatchVarLenMultiTypeTargetVertexProperties: function() {
          const query = aql`MATCH (v :mvc) -[ e:mec1|mec2 * 1..2 ]-> (w :mvc {_key: "v2"})
                              RETURN e`;
          const expected = [
            "(mvc/v0) -[]-> (mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v2)",
            "(mvc/v1) -[]-> (mvc/v2)"
          ];
          expected.sort();
          const result = db._query(query, {}, options).toArray().map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },

        testMatchVarLenMultiTypeTargetVertexWhereClause: function() {
          const query = aql`MATCH (v :mvc) -[ e:mec1|mec2 * 1..2 ]-> (w :mvc WHERE w._key == "v2")
                              RETURN e`;
          const expected = [
            "(mvc/v0) -[]-> (mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v2)",
            "(mvc/v1) -[]-> (mvc/v2)"
          ];
          expected.sort();
          const result = db._query(query, {}, options).toArray().map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },

        testMatchVarLenUnsatisfiableTargetVertexFilter: function() {
          // no vertex has _key == "nope", so none of these may return a row
          const queries = [
            "MATCH (v :mvc) -[ e:mec1 * 1..2 ]-> (w :mvc {_key: \"nope\"}) RETURN e",
            "MATCH (v :mvc) -[ e:mec1 * 1..2 ]-> (w :mvc WHERE w._key == \"nope\") RETURN e",
            "MATCH (v :mvc) -[ e:mec1|mec2 * 1..2 ]-> (w :mvc {_key: \"nope\"}) RETURN e",
            "MATCH (v :mvc) -[ e:mec1|mec2 * 1..2 ]-> (w :mvc WHERE w._key == \"nope\") RETURN e"
          ];
          for (const query of queries) {
            assertEqual(db._query(query, {}, options).toArray(), [], query);
          }
        },

        testMatchVarLenBothVertexFilters: function() {
          // start-vertex constraints already worked; assert they compose with
          // the target-vertex ones rather than replacing them
          const query = aql`MATCH (v :mvc {_key: "v0"}) -[ e:mec1|mec2 * 1..2 ]-> (w :mvc {_key: "v2"})
                              RETURN e`;
          const expected = [
            "(mvc/v0) -[]-> (mvc/v1) -[]-> (mvc/v2)",
            "(mvc/v0) -[]-> (mvc/v2)"
          ];
          expected.sort();
          const result = db._query(query, {}, options).toArray().map(pathToString);
          result.sort();
          assertEqual(result, expected);
        },

    };
}

jsunity.run(aqlMatchStatementVariableLengthTestSuite);

return jsunity.done();



