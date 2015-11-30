/*jshint esnext: true */
/*global assertEqual, fail*/

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

(function() {
  "use strict";

  const jsunity = require("jsunity");

  const internal = require("internal");
  const db = internal.db;
  const errors = require("org/arangodb").errors;
  const gm = require("org/arangodb/general-graph");
  const vn = "UnitTestVertexCollection";
  const en = "UnitTestEdgeCollection";
  let vertex = {};
  let edge = {};

  let cleanup = function () {
    db._drop(vn);
    db._drop(en);
    vertex = {};
    edge = {};
  };

  let createBaseGraph = function () {
    let vc = db._create(vn, {numberOfShards: 4});
    let ec = db._createEdgeCollection(en, {numberOfShards: 4});
    vertex.A = vc.save({_key: "A"})._id;
    vertex.B = vc.save({_key: "B"})._id;
    vertex.C = vc.save({_key: "C"})._id;
    vertex.D = vc.save({_key: "D"})._id;
    vertex.E = vc.save({_key: "E"})._id;
    vertex.F = vc.save({_key: "F"})._id;

    edge.AB = ec.save(vertex.A, vertex.B, {})._id;
    edge.BC = ec.save(vertex.B, vertex.C, {})._id;
    edge.CD = ec.save(vertex.C, vertex.D, {})._id;
    edge.CF = ec.save(vertex.C, vertex.F, {})._id;
    edge.EB = ec.save(vertex.E, vertex.B, {})._id;
    edge.FE = ec.save(vertex.F, vertex.E, {})._id;
  };

  let namedGraphSuite = function() {

    /***********************************************************************
     * Graph under test:
     *
     *  A -> B -> C -> D
     *      /|\  \|/
     *       E <- F
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     ***********************************************************************/

    let g;
    const gn = "UnitTestGraph";

    return {

      setup: function() {
        cleanup();
        createBaseGraph();
        try {
          gm._drop(gn);
        } catch (e) {
          // It is expected that this graph does not exist.
        }
        g = gm._create(gn, [gm._relation(en, vn, vn)]);
      },

      tearDown: function () {
        gm._drop(gn);
        cleanup();
      },

      firstEntryIsVertexTest: function () {
        let query = "FOR x IN OUTBOUND @startId GRAPH @graph RETURN x";
        let bindVars = {
          graph: gn,
          startId: vertex.B
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 1);
        assertEqual(result[0]._id, vertex.C);
      },

      secondEntryIsEdgeTest: function () {
        let query = "FOR x, e IN OUTBOUND @startId GRAPH @graph RETURN e";
        let bindVars = {
          graph: gn,
          startId: vertex.B
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 1);
        assertEqual(result[0]._id, edge.BC);
      },

      thirdEntryIsPathTest: function () {
        let query = "FOR x, e, p IN OUTBOUND @startId GRAPH @graph RETURN p";
        let bindVars = {
          graph: gn,
          startId: vertex.B
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 1);
        let entry = result[0];
        assertEqual(entry.vertices.length, 2);
        assertEqual(entry.vertices[0]._id, vertex.B);
        assertEqual(entry.vertices[1]._id, vertex.C);
        assertEqual(entry.edges.length, 1);
        assertEqual(entry.edges[0]._id, edge.BC);
      },

      outboundDirectionTest: function () {
        let query = "FOR x IN OUTBOUND @startId GRAPH @graph RETURN x._id";
        let bindVars = {
          graph: gn,
          startId: vertex.B
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 1);
        let entry = result[0];
        assertEqual(entry, vertex.C);
      },

      inboundDirectionTest: function () {
        let query = "FOR x IN INBOUND @startId GRAPH @graph RETURN x._id";
        let bindVars = {
          graph: gn,
          startId: vertex.C
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 1);
        let entry = result[0];
        assertEqual(entry, vertex.B);
      },

      anyDirectionTest: function () {
        let query = "FOR x IN ANY @startId GRAPH @graph SORT x._id ASC RETURN x._id";
        let bindVars = {
          graph: gn,
          startId: vertex.B
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 3);
        let entry = result[0];
        assertEqual(entry, vertex.A);
        entry = result[1];
        assertEqual(entry, vertex.C);
        entry = result[2];
        assertEqual(entry, vertex.E);
      },

      exactNumberStepsTest: function () {
        let query = "FOR x IN 2 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
        let bindVars = {
          graph: gn,
          startId: vertex.B
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 2);

        assertEqual(result[0], vertex.D);
        assertEqual(result[1], vertex.F);
      },

      rangeNumberStepsTest: function () {
        let query = "FOR x IN 2..3 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
        let bindVars = {
          graph: gn,
          startId: vertex.B
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 3);

        assertEqual(result[0], vertex.D);
        assertEqual(result[1], vertex.E);
        assertEqual(result[2], vertex.F);
      },

      computedNumberStepsTest: function () {
        let query = "FOR x IN LENGTH([1,2]) OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
        let bindVars = {
          graph: gn,
          startId: vertex.B
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 2);

        assertEqual(result[0], vertex.D);
      },

      sortTest: function () {
          let query = "FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
          let bindVars = {
            graph: gn,
            startId: vertex.C
          };

          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 2);
          assertEqual(result[0], vertex.D);
          assertEqual(result[1], vertex.F);

          // Reverse ordering
          query = "FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id DESC RETURN x._id";

          result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 2);
          assertEqual(result[0], vertex.F);
          assertEqual(result[1], vertex.D);
        }

      };
    };

    let multiCollectionGraphSuite = function () {

      /***********************************************************************
       * Graph under test:
       *
       *  A -> B -> C -> D <-E2- V2:G
       *      /|\  \|/   
       *       E <- F
       *
       *
       *
       ***********************************************************************/
 
      let g;
      const gn = "UnitTestGraph";
      const vn2 = "UnitTestVertexCollection2";
      const en2 = "UnitTestEdgeCollection2";

      // We always use the same query, the result should be identical.
      let validateResult = function (result) {
        assertEqual(result.length, 1);
        let entry = result[0];
        assertEqual(entry.vertex._id, vertex.C);
        assertEqual(entry.path.vertices.length, 2);
        assertEqual(entry.path.vertices[0]._id, vertex.B);
        assertEqual(entry.path.vertices[1]._id, vertex.C);
        assertEqual(entry.path.edges.length, 1);
        assertEqual(entry.path.edges[0]._id, edge.BC);
      };

      return {

        setup: function() {
          cleanup();
          try {
            gm._drop(gn);
          } catch (e) {
            // It is expected that this graph does not exist.
          }
          db._drop(vn2);
          db._drop(en2);
          createBaseGraph();
          g = gm._create(gn, [gm._relation(en, vn, vn), gm._relation(en2, vn2, vn)]);
          db[vn2].save({_key: "G"});
          db[en2].save(vn2 + "/G", vn + "/D", {});
        },

        tearDown: function() {
          gm._drop(gn);
          db._drop(vn2);
          db._drop(en2);
          cleanup();
        },

        noBindParameterTest: function () {
          let query = "FOR x, p IN OUTBOUND '" + vertex.B + "' " + en + " RETURN {vertex: x, path: p}";
          validateResult(db._query(query).toArray());
        },

        startBindParameterTest: function () {
          let query = "FOR x, p IN OUTBOUND @startId " + en + " RETURN {vertex: x, path: p}";
          let bindVars = {
            startId: vertex.B
          };
          validateResult(db._query(query, bindVars).toArray());
        },

        edgeCollectionBindParameterTest: function () {
          let query = "FOR x, p IN OUTBOUND '" + vertex.B + "' @@eCol RETURN {vertex: x, path: p}";
          let bindVars = {
            "@eCol": en
          };
          validateResult(db._query(query, bindVars).toArray());
        },

        stepsBindParameterTest: function () {
          let query = "FOR x, p IN @steps OUTBOUND '" + vertex.B + "' " + en + " RETURN {vertex: x, path: p}";
          let bindVars = {
            steps: 1
          };
          validateResult(db._query(query, bindVars).toArray());
        },

        stepsRangeBindParameterTest: function () {
          let query = "FOR x, p IN @lsteps..@rsteps OUTBOUND '" + vertex.B
                    + "' " + en + " RETURN {vertex: x, path: p}";
          let bindVars = {
            lsteps: 1,
            rsteps: 1
          };
          validateResult(db._query(query, bindVars).toArray());
        },

        firstEntryIsVertexTest: function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 1);
          assertEqual(result[0]._id, vertex.C);
        },

        secondEntryIsEdgeTest: function () {
          let query = "FOR x, e IN OUTBOUND @startId @@eCol RETURN e";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 1);
          assertEqual(result[0]._id, edge.BC);
        },

        thirdEntryIsPathTest: function () {
          let query = "FOR x, e, p IN OUTBOUND @startId @@eCol RETURN p";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 1);
          let entry = result[0];
          assertEqual(entry.vertices.length, 2);
          assertEqual(entry.vertices[0]._id, vertex.B);
          assertEqual(entry.vertices[1]._id, vertex.C);
          assertEqual(entry.edges.length, 1);
          assertEqual(entry.edges[0]._id, edge.BC);
        },

        outboundDirectionTest: function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 1);
          let entry = result[0];
          assertEqual(entry, vertex.C);
        },

        inboundDirectionTest: function () {
          let query = "FOR x IN INBOUND @startId @@eCol RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.C
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 1);
          let entry = result[0];
          assertEqual(entry, vertex.B);
        },

        anyDirectionTest: function () {
          let query = "FOR x IN ANY @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 3);
          let entry = result[0];
          assertEqual(entry, vertex.A);
          entry = result[1];
          assertEqual(entry, vertex.C);
          entry = result[2];
          assertEqual(entry, vertex.E);
        },

        exactNumberStepsTest: function () {
          let query = "FOR x IN 2 OUTBOUND @startId @@eCol  SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 2);

          assertEqual(result[0], vertex.D);
          assertEqual(result[1], vertex.F);
        },

        rangeNumberStepsTest: function () {
          let query = "FOR x IN 2..3 OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 3);

          assertEqual(result[0], vertex.D);
          assertEqual(result[1], vertex.E);
          assertEqual(result[2], vertex.F);
        },

        computedNumberStepsTest: function () {
          let query = "FOR x IN LENGTH([1,2]) OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 2);

          assertEqual(result[0], vertex.D);
        },

        sortTest: function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.C
          };

          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 2);
          assertEqual(result[0], vertex.D);
          assertEqual(result[1], vertex.F);

          // Reverse ordering
          query = "FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id DESC RETURN x._id";

          result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 2);
          assertEqual(result[0], vertex.F);
          assertEqual(result[1], vertex.D);
        },

        singleDocumentInputTest: function () {
          let query = "FOR y IN @@vCol FILTER y._id == @startId "
                    + "FOR x IN OUTBOUND y @@eCol RETURN x";
          let bindVars = {
            startId: vertex.B,
            "@eCol": en,
            "@vCol": vn
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 1);
          assertEqual(result[0]._id, vertex.C);
        },

        listDocumentInputTest: function () {
          let query = "FOR y IN @@vCol "
                    + "FOR x IN OUTBOUND y @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            "@vCol": vn
          };
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length).toEqual(6);
          assertEqual(result[0], vertex.B);
          assertEqual(result[1], vertex.B);
          assertEqual(result[2], vertex.C);
          assertEqual(result[3], vertex.D);
          assertEqual(result[4], vertex.E);
          assertEqual(result[5], vertex.F);
        },

      };
    };

    let potentialErrorsSuite = function () {
      let vc, ec;

      return {

        setup: function () {
          cleanup();
          vc = db._create(vn);
          ec = db._createEdgeCollection(en);
          vertex.A = vn + "/unknown";
        },

        tearDown: cleanup,

        testNonIntegerSteps: function () {
          let query = "FOR x IN 2.5 OUTBOUND @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            "startId": vertex.A
          };
          try {
            db._query(query, bindVars).toArray();
            fail();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
        },

        testNonNumberSteps: function () {
          let query = "FOR x IN 'invalid' OUTBOUND @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            "startId": vertex.A
          };
          try {
            db._query(query, bindVars).toArray();
            fail();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
        },

        testMultiDirections: function () {
          let query = "FOR x IN OUTBOUND ANY @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            "startId": vertex.A
          };
          try {
            db._query(query, bindVars).toArray();
            fail();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
        },

        testNoCollections: function () {
          let query = "FOR x IN OUTBOUND @startId RETURN x";
          let bindVars = {
            "startId": vertex.A
          };
          try {
            db._query(query, bindVars).toArray();
            fail();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
        },

        testNoStartVertex: function () {
          let query = "FOR x IN OUTBOUND @@eCol RETURN x";
          let bindVars = {
            "@eCol": en
          };
          try {
            db._query(query, bindVars).toArray();
            fail();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
        },

        testTooManyOutputParameters: function () {
          let query = "FOR x, y, z, f IN OUTBOUND @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            "startId": vertex.A
          };
          try {
            db._query(query, bindVars).toArray();
            fail();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
        },

        testTraverseVertexCollection: function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol, @@vCol RETURN x";
          let bindVars = {
            "@eCol": en,
            "@vCol": vn,
            "startId": vertex.A
          };
          try {
            db._query(query, bindVars).toArray();
            fail(query + " should not be allowed");
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code);
          }
        },

        testStartWithSubquery: function () {
          let query = "FOR x IN OUTBOUND (FOR y IN @@vCol SORT y._id LIMIT 3 RETURN y) @@eCol SORT x._id RETURN x";
          let bindVars = {
            "@eCol": en,
            "@vCol": vn
          };
          try {
            db._query(query, bindVars).toArray();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
          /*
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(4);
          expect(result[0]._id).toEqual(vertex.B);
          expect(result[1]._id).toEqual(vertex.C);
          expect(result[2]._id).toEqual(vertex.D);
          expect(result[3]._id).toEqual(vertex.F);
          */
        },

        testStepsSubquery: function() {
          let query = "FOR x IN (FOR y IN 1..1 RETURN y) OUTBOUND @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            "startId": vertex.A
          };
          try {
            db._query(query, bindVars).toArray();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
          }
          /*
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          expect(result[0]._id).toEqual(vertex.B);
          */
        }

      };
    };

    let complexInternaSuite = function () {
      let vc, ec;

      return {

        setup: function () {
          cleanup();
          createBaseGraph();
        },

        tearDown: cleanup,

        testUnknownVertexCollection: function () {
          const vn2 = "UnitTestVertexCollectionOther";
          db._drop(vn2);
          const vc2 = db._create(vn2);
          vc.save({_key: "1"});
          vc2.save({_key: "1"});
          ec.save(vn + "/1", vn2 + "/1", {});
          let query = "FOR x IN OUTBOUND @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            "startId": vn + "/1"
          };
          // NOTE: vn2 is not explicitly named in AQL
          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 1);
          assertEqual(result[0]._id, vn2 + "/1");
          db._drop(vn2);
        },

        testStepsFromLet: function () {
          let query = "LET s = 1 FOR x IN s OUTBOUND @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            "startId": vertex.A
          };

          let result = db._query(query, bindVars).toArray();
          assertEqual(result.length, 1);
          assertEqual(result[0]._id, vertex.B);
        },

        testMultipleBlocksResult: function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol RETURN x";
          let amount = 10000;
          let startId = vn + "/test";
          let bindVars = {
            "@eCol": en,
            "startId": startId
          };
          vc.save({_key: startId.split("/")[1]});
          
          // Insert amount many edges and vertices into the collections.
          for (let i = 0; i < amount; ++i) {
            let tmp = vc.save({_key: "" + i})._id;
            ec.save(startId, tmp, {});
          }

          // Check that we can get all of them out again.
          let result = db._query(query, bindVars).toArray();
          // Internally: The Query selects elements in chunks, check that nothing is lost.
          assertEqual(result.length, amount);
        },

        testSkipSome: function () {
          let query = "FOR x, e, p IN 1..2 OUTBOUND @startId @@eCol LIMIT 4, 100 RETURN p.vertices[1]._key";
          let startId = vn + "/test";
          let bindVars = {
            "@eCol": en,
            "startId": startId
          };
          vc.save({_key: startId.split("/")[1]});
          
          // Insert amount many edges and vertices into the collections.
          for (let i = 0; i < 3; ++i) {
            let tmp = vc.save({_key: "" + i})._id;
            ec.save(startId, tmp, {});
            for (let k = 0; k < 3; ++k) {
              let tmp2 = vc.save({_key: "" + i + "_" + k})._id;
              ec.save(tmp, tmp2, {});
            }
          }

          // Check that we can get all of them out again.
          let result = db._query(query, bindVars).toArray();
          // Internally: The Query selects elements in chunks, check that nothing is lost.
          assertEqual(result.length, 8);

          // Each of the 3 parts of this graph contains of 4 nodes, one connected to the start.
          // And 3 connected to the first one. As we do a depth first traversal we expect to skip
          // exactly one sub-tree. Therefor we expect exactly two sub-trees to be traversed.
          let seen = {};
          for (let r of result) {
            if (!seen.hasOwnProperty(r)) {
              seen[r] = true;
            }
          }
          assertEqual(Object.keys(seen).length, 2);
        },

        testManyResults: function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol RETURN x._key";
          let startId = vn + "/many";
          let bindVars = {
            "@eCol": en,
            "startId": startId
          };
          vc.save({_key: startId.split("/")[1]});
          let amount = 10000;
          for (let i = 0; i < amount; ++i) {
            let _id = vc.save({});
            ec.save(startId, _id, {});
          }
          let result = db._query(query, bindVars);
          let found = 0;
          // Count has to be correct
          assertEqual(result.count(), amount);
          while (result.hasNext()) {
            result.next();
            ++found;
          }
          // All elements must be enumerated
          assertEqual(found, amount);
        }

      };

  };

  let complexFilteringSuite = function() {

    /***********************************************************************
     * Graph under test:
     *
     * C <- B <- A -> D -> E
     * F <--|         |--> G
     *
     *
     *
     *
     *
     * Tri1 --> Tri2
     *  ^        |
     *  |--Tri3<-|
     *
     *
     ***********************************************************************/

    return {
      setup: function() {
        cleanup();
        let vc = db._create(vn, {numberOfShards: 4});
        let ec = db._createEdgeCollection(en, {numberOfShards: 4});
        vertex.A = vc.save({_key: "A", left: false, right: false})._id;
        vertex.B = vc.save({_key: "B", left: true, right: false})._id;
        vertex.C = vc.save({_key: "C", left: true, right: false})._id;
        vertex.D = vc.save({_key: "D", left: false, right: true})._id;
        vertex.E = vc.save({_key: "E", left: false, right: true})._id;
        vertex.F = vc.save({_key: "F", left: true, right: false})._id;
        vertex.G = vc.save({_key: "G", left: false, right: true})._id;

        edge.AB = ec.save(vertex.A, vertex.B, {left: true, right: false})._id;
        edge.BC = ec.save(vertex.B, vertex.C, {left: true, right: false})._id;
        edge.AD = ec.save(vertex.A, vertex.D, {left: false, right: true})._id;
        edge.DE = ec.save(vertex.D, vertex.E, {left: false, right: true})._id;
        edge.BF = ec.save(vertex.B, vertex.F, {left: true, right: false})._id;
        edge.DG = ec.save(vertex.D, vertex.G, {left: false, right: true})._id;

        vertex.Tri1 = vc.save({_key: "Tri1", isLoop: true})._id;
        vertex.Tri2 = vc.save({_key: "Tri2", isLoop: true})._id;
        vertex.Tri3 = vc.save({_key: "Tri3", isLoop: true})._id;

        edge.Tri12 = ec.save(vertex.Tri1, vertex.Tri2, {isLoop: true})._id;
        edge.Tri23 = ec.save(vertex.Tri2, vertex.Tri3, {isLoop: true})._id;
        edge.Tri31 = ec.save(vertex.Tri3, vertex.Tri1, {isLoop: true, lateLoop: true})._id;
      },

      tearDown: cleanup,

      testVertexEarlyPruneHighDepth: function () {
        let query = "FOR v, e, p IN 100 OUTBOUND @start @@eCol FILTER p.vertices[1]._key == 'wrong' RETURN v";
        let bindVars = {
          "@eCol": en,
          "start": vertex.A
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 0);
      },

      testStartVertexEarlyPruneHighDepth: function () {
        let query = "FOR v, e, p IN 100 OUTBOUND @start @@eCol FILTER p.vertices[0]._key == 'wrong' RETURN v";
        let bindVars = {
          "@eCol": en,
          "start": vertex.A
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 0);
      },

      testEdgesEarlyPruneHighDepth: function () {
        let query = "FOR v, e, p IN 100 OUTBOUND @start @@eCol FILTER p.edges[0]._key == 'wrong' RETURN v";
        let bindVars = {
          "@eCol": en,
          "start": vertex.A
        };
        let result = db._query(query, bindVars).toArray();
        assertEqual(result.length, 0);
      },

      testVertexLevel0: function () {
        let query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
                     FILTER p.vertices[0].left == true
                     RETURN v`;
        let bindVars = {
          "@ecol": en,
          start: vertex.A
        };
        let cursor = db._query(query, bindVars);
        assertEqual(cursor.count(), 0);
        assertEqual(cursor.getExtra().stats.scannedFull, 0);
        assertEqual(cursor.getExtra().stats.scannedIndex, 2);
        assertEqual(cursor.getExtra().stats.filtered, 1);
      }

    };

  };

  jsunity.run(namedGraphSuite);
  jsunity.run(multiCollectionGraphSuite);
  jsunity.run(potentialErrorsSuite);
  jsunity.run(complexInternaSuite);
  jsunity.run(complexFilteringSuite);

  return jsunity.done();

}());
