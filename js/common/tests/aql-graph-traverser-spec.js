/*jshint esnext: true */
/*global describe, beforeEach, it, expect, afterEach, fail*/

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

  const internal = require("internal");
  const db = internal.db;
  const errors = require("org/arangodb").errors;
  const gm = require("org/arangodb/general-graph");
  const vn = "UnitTestVertexCollection";
  const en = "UnitTestEdgeCollection";
  const vertex = {};
  const edge = {};

  let cleanup = function () {
    db._drop(vn);
    db._drop(en);
  };

  describe("The FOR x IN GRAPH statement", function () {

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

    beforeEach(function() {
      cleanup();
      let vc = db._create(vn);
      let ec = db._createEdgeCollection(en);
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
    });

    afterEach(function() {
      cleanup();
    });

    describe("with a named graph", function () {
      let g;
      const gn = "UnitTestGraph";

      beforeEach(function() {
        try {
          gm._drop(gn);
        } catch (e) {
          // It is expected that this graph does not exist.
        }
        g = gm._create(gn, [gm._relation(en, vn, vn)]);
      });

      afterEach(function() {
        gm._drop(gn);
      });

      describe("return format", function() {

        it("should return the vertex as first entry", function () {
          let query = "FOR x IN OUTBOUND @startId GRAPH @graph RETURN x";
          let bindVars = {
            graph: gn,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          expect(result[0]._id).toEqual(vertex.C);
        });

        it("should return the edge as second entry", function () {
          let query = "FOR x, e IN OUTBOUND @startId GRAPH @graph RETURN e";
          let bindVars = {
            graph: gn,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          expect(result[0]._id).toEqual(edge.BC);
        });

        it("should return the path as third entry", function () {
          let query = "FOR x, e, p IN OUTBOUND @startId GRAPH @graph RETURN p";
          let bindVars = {
            graph: gn,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          let entry = result[0];
          expect(entry.vertices.length).toEqual(2);
          expect(entry.vertices[0]._id).toEqual(vertex.B);
          expect(entry.vertices[1]._id).toEqual(vertex.C);
          expect(entry.edges.length).toEqual(1);
          expect(entry.edges[0]._id).toEqual(edge.BC);
        });

      });

      describe("direction", function() {

        it("can use outbound direction", function () {
          let query = "FOR x IN OUTBOUND @startId GRAPH @graph RETURN x._id";
          let bindVars = {
            graph: gn,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          let entry = result[0];
          expect(entry).toEqual(vertex.C);
        });

        it("can use inbound direction", function () {
          let query = "FOR x IN INBOUND @startId GRAPH @graph RETURN x._id";
          let bindVars = {
            graph: gn,
            startId: vertex.C
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          let entry = result[0];
          expect(entry).toEqual(vertex.B);
        });

        it("can use any direction", function () {
          let query = "FOR x IN ANY @startId GRAPH @graph SORT x._id ASC RETURN x._id";
          let bindVars = {
            graph: gn,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(3);

          let entry = result[0];
          expect(entry).toEqual(vertex.A);
          entry = result[1];
          expect(entry).toEqual(vertex.C);
          entry = result[2];
          expect(entry).toEqual(vertex.E);
        });

      });

      describe("steps", function () {

        it("can use an exact number of steps", function () {
          let query = "FOR x IN 2 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
          let bindVars = {
            graph: gn,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(2);

          expect(result[0]).toEqual(vertex.D);
          expect(result[1]).toEqual(vertex.F);
        });

        it("can use a range of steps", function () {
          let query = "FOR x IN 2..3 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
          let bindVars = {
            graph: gn,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(3);

          expect(result[0]).toEqual(vertex.D);
          expect(result[1]).toEqual(vertex.E);
          expect(result[2]).toEqual(vertex.F);
        });

        it("can use a computed function of steps", function () {
          let query = "FOR x IN LENGTH([1,2]) OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
          let bindVars = {
            graph: gn,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(2);

          expect(result[0]).toEqual(vertex.D);
          expect(result[1]).toEqual(vertex.F);
        });

      });

      describe("sorting", function () {
        it("should be able to sort the result", function () {
          let query = "FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
          let bindVars = {
            graph: gn,
            startId: vertex.C
          };

          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(2);
          expect(result[0]).toEqual(vertex.D);
          expect(result[1]).toEqual(vertex.F);

          // Reverse ordering
          query = "FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id DESC RETURN x._id";

          result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(2);
          expect(result[0]).toEqual(vertex.F);
          expect(result[1]).toEqual(vertex.D);
        });

      });
    });

    describe("with a multi-collection graph", function () {

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

      beforeEach(function() {
        try {
          gm._drop(gn);
        } catch (e) {
          // It is expected that this graph does not exist.
        }
        db._drop(vn2);
        db._drop(en2);
        g = gm._create(gn, [gm._relation(en, vn, vn), gm._relation(en2, vn2, vn)]);
        db[vn2].save({_key: "G"});
        db[en2].save(vn2 + "/G", vn + "/D", {});
      });

      afterEach(function() {
        gm._drop(gn);
        db._drop(vn2);
        db._drop(en2);
      });

 
    });

    describe("with a collection", function () {

      describe("bind parameter positions", function () {

        // We always use the same query, the result should be identical.
        let validateResult = function (result) {
          expect(result.length).toEqual(1);
          try {
          let entry = result[0];
          expect(entry.vertex._id).toEqual(vertex.C);
          expect(entry.path.vertices.length).toEqual(2);
          expect(entry.path.vertices[0]._id).toEqual(vertex.B);
          expect(entry.path.vertices[1]._id).toEqual(vertex.C);
          expect(entry.path.edges.length).toEqual(1);
          expect(entry.path.edges[0]._id).toEqual(edge.BC);
          } catch (e) {}
        };

        it("should be able to use no bind parameters", function () {
          let query = "FOR x, p IN OUTBOUND '" + vertex.B + "' " + en + " RETURN {vertex: x, path: p}";
          validateResult(db._query(query).toArray());
        });

        it("should be able to bind the start point", function () {
          let query = "FOR x, p IN OUTBOUND @startId " + en + " RETURN {vertex: x, path: p}";
          let bindVars = {
            startId: vertex.B
          };
          validateResult(db._query(query, bindVars).toArray());
        });

        it("should be able to bind the edge collection", function () {
          let query = "FOR x, p IN OUTBOUND '" + vertex.B + "' @@eCol RETURN {vertex: x, path: p}";
          let bindVars = {
            "@eCol": en
          };
          validateResult(db._query(query, bindVars).toArray());
        });

        it("should be able to bind the steps", function () {
          let query = "FOR x, p IN @steps OUTBOUND '" + vertex.B + "' " + en + " RETURN {vertex: x, path: p}";
          let bindVars = {
            steps: 1
          };
          validateResult(db._query(query, bindVars).toArray());
        });

        it("should be able to bind the steps as range with two values", function () {
          let query = "FOR x, p IN @lsteps..@rsteps OUTBOUND '" + vertex.B
                    + "' " + en + " RETURN {vertex: x, path: p}";
          let bindVars = {
            lsteps: 1,
            rsteps: 1
          };
          validateResult(db._query(query, bindVars).toArray());
        });

        /* TODO: Should we support this?
        it("should be able to bind the steps as range in one value", function () {
          let query = "FOR x IN TRAVERSE FROM '" + vertex.B + "' GRAPH " + en + ", "
                    + vn + " @steps STEPS RETURN x";
          let bindVars = {
            "steps": "1..1",
          };
          validateResult(db._query(query, bindVars).toArray());
        });
        */

      });

      describe("return format", function() {

        it("should return the vertex as first entry", function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol RETURN x";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          expect(result[0]._id).toEqual(vertex.C);
        });

        it("should return the edge as second entry", function () {
          let query = "FOR x, e IN OUTBOUND @startId @@eCol RETURN e";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          expect(result[0]._id).toEqual(edge.BC);
        });

        it("should return the path as third entry", function () {
          let query = "FOR x, e, p IN OUTBOUND @startId @@eCol RETURN p";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          let entry = result[0];
          expect(entry.vertices.length).toEqual(2);
          expect(entry.vertices[0]._id).toEqual(vertex.B);
          expect(entry.vertices[1]._id).toEqual(vertex.C);
          expect(entry.edges.length).toEqual(1);
          expect(entry.edges[0]._id).toEqual(edge.BC);
        });

      });

      describe("direction", function() {

        it("can use outbound direction", function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          let entry = result[0];
          expect(entry).toEqual(vertex.C);
        });

        it("can use inbound direction", function () {
          let query = "FOR x IN INBOUND @startId @@eCol RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.C
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          let entry = result[0];
          expect(entry).toEqual(vertex.B);
        });

        it("can use any direction", function () {
          let query = "FOR x IN ANY @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(3);

          let entry = result[0];
          expect(entry).toEqual(vertex.A);
          entry = result[1];
          expect(entry).toEqual(vertex.C);
          entry = result[2];
          expect(entry).toEqual(vertex.E);
        });

      });

      describe("steps", function () {

        it("can use an exact number of steps", function () {
          let query = "FOR x IN 2 OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(2);

          expect(result[0]).toEqual(vertex.D);
          expect(result[1]).toEqual(vertex.F);
        });

        it("can use a range of steps", function () {
          let query = "FOR x IN 2..3 OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(3);

          expect(result[0]).toEqual(vertex.D);
          expect(result[1]).toEqual(vertex.E);
          expect(result[2]).toEqual(vertex.F);
        });

        it("can use a computed function of steps", function () {
          let query = "FOR x IN LENGTH([1,2]) OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.B
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(2);

          expect(result[0]).toEqual(vertex.D);
          expect(result[1]).toEqual(vertex.F);
        });

      });

      describe("sorting", function () {
        it("should be able to sort the result", function () {
          let query = "FOR x IN OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            startId: vertex.C
          };

          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(2);
          expect(result[0]).toEqual(vertex.D);
          expect(result[1]).toEqual(vertex.F);

          // Reverse ordering
          query = "FOR x IN OUTBOUND @startId @@eCol SORT x._id DESC RETURN x._id";

          result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(2);
          expect(result[0]).toEqual(vertex.F);
          expect(result[1]).toEqual(vertex.D);
        });

      });

      describe("document input" , function () {

        it("should be able to use a document from a further iteration as input", function () {
          let query = "FOR y IN @@vCol FILTER y._id == @startId "
                    + "FOR x IN OUTBOUND y @@eCol RETURN x";
          let bindVars = {
            startId: vertex.B,
            "@eCol": en,
            "@vCol": vn
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(1);
          expect(result[0]._id).toEqual(vertex.C);
        });

        it("should be able to use a list of documents as input", function () {
          let query = "FOR y IN @@vCol "
                    + "FOR x IN OUTBOUND y @@eCol SORT x._id ASC RETURN x._id";
          let bindVars = {
            "@eCol": en,
            "@vCol": vn
          };
          let result = db._query(query, bindVars).toArray();
          expect(result.length).toEqual(6);
          expect(result[0]).toEqual(vertex.B);
          expect(result[1]).toEqual(vertex.B);
          expect(result[2]).toEqual(vertex.C);
          expect(result[3]).toEqual(vertex.D);
          expect(result[4]).toEqual(vertex.E);
          expect(result[5]).toEqual(vertex.F);
        });

 
      });

    });


  });

  describe("Potential errors", function () {

    let vc, ec;

    beforeEach(function () {
      cleanup();
      vc = db._create(vn);
      ec = db._createEdgeCollection(en);
    });

    afterEach(function () {
      cleanup();
    });

    describe("Malformed AQL", function () {

      it("should not allow non-integer numbers of steps", function () {
        let query = "FOR x IN 2.5 OUTBOUND @startId @@eCol RETURN x";
        let bindVars = {
          "@eCol": en,
          "startId": vertex.A
        };
        try {
          db._query(query, bindVars).toArray();
          fail(query + " should not be allowed");
        } catch (e) {
          expect(e.errorNum).toEqual(errors.ERROR_QUERY_PARSE.code);
        }
      });

      it("should not allow non numbers of steps", function () {
        let query = "FOR x IN 'invalid' OUTBOUND @startId @@eCol RETURN x";
        let bindVars = {
          "@eCol": en,
          "startId": vertex.A
        };
        try {
          db._query(query, bindVars).toArray();
          fail(query + " should not be allowed");
        } catch (e) {
          expect(e.errorNum).toEqual(errors.ERROR_QUERY_PARSE.code);
        }
      });

      it("should not allow more than one direction", function () {
        let query = "FOR x IN OUTBOUND ANY @startId @@eCol RETURN x";
        let bindVars = {
          "@eCol": en,
          "startId": vertex.A
        };
        try {
          db._query(query, bindVars).toArray();
          fail(query + " should not be allowed");
        } catch (e) {
          expect(e.errorNum).toEqual(errors.ERROR_QUERY_PARSE.code);
        }
      });

      it("should not allow an empty collection list", function () {
        let query = "FOR x IN OUTBOUND @startId RETURN x";
        let bindVars = {
          "startId": vertex.A
        };
        try {
          db._query(query, bindVars).toArray();
          fail(query + " should not be allowed");
        } catch (e) {
          expect(e.errorNum).toEqual(errors.ERROR_QUERY_PARSE.code);
        }
      });

      it("should not allow a query without a start vertex", function () {
        let query = "FOR x IN OUTBOUND @@eCol RETURN x";
        let bindVars = {
          "@eCol": en
        };
        try {
          db._query(query, bindVars).toArray();
          fail(query + " should not be allowed");
        } catch (e) {
          expect(e.errorNum).toEqual(errors.ERROR_QUERY_PARSE.code);
        }
      });

      it("should not allow more than three output parameters", function () {
        let query = "FOR x, y, z, f IN OUTBOUND @startId @@eCol RETURN x";
        let bindVars = {
          "@eCol": en,
          "startId": vertex.A
        };
        try {
          db._query(query, bindVars).toArray();
          fail(query + " should not be allowed");
        } catch (e) {
          expect(e.errorNum).toEqual(errors.ERROR_QUERY_PARSE.code);
        }
      });

      it("should not allow to use a vertex collection for traversing", function () {
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
          expect(e.errorNum).toEqual(errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code);
        }
      });

      it("should not allow starting with a subquery", function () {
        let query = "FOR x IN OUTBOUND (FOR y IN @@vCol SORT y._id LIMIT 3 RETURN y) @@eCol SORT x._id RETURN x";
        let bindVars = {
          "@eCol": en,
          "@vCol": vn
        };
        try {
          db._query(query, bindVars).toArray();
        } catch (e) {
          expect(e.errorNum).toEqual(errors.ERROR_QUERY_PARSE.code);
        }
        /*
        let result = db._query(query, bindVars).toArray();
        expect(result.length).toEqual(4);
        expect(result[0]._id).toEqual(vertex.B);
        expect(result[1]._id).toEqual(vertex.C);
        expect(result[2]._id).toEqual(vertex.D);
        expect(result[3]._id).toEqual(vertex.F);
        */
      });

      it("should not allow to determine the steps with a subquery", function () {
        let query = "FOR x IN (FOR y IN 1..1 RETURN y) OUTBOUND @startId @@eCol RETURN x";
        let bindVars = {
          "@eCol": en,
          "startId": vertex.A
        };
        try {
          db._query(query, bindVars).toArray();
        } catch (e) {
          expect(e.errorNum).toEqual(errors.ERROR_QUERY_PARSE.code);
        }
        /*
        let result = db._query(query, bindVars).toArray();
        expect(result.length).toEqual(1);
        expect(result[0]._id).toEqual(vertex.B);
        */
      });


    });

    describe("queries with complex interna", function () {
      let vc, ec;

      beforeEach(function() {
        cleanup();
        vc = db._create(vn);
        ec = db._createEdgeCollection(en);
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
      });

      afterEach(function() {
        cleanup();
      });

      it("should return documents from unknown vertex collections", function () {
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
        expect(result.length).toEqual(1);
        expect(result[0]._id).toEqual(vn2 + "/1");
        db._drop(vn2);
      });

      it("should use the steps from let", function () {
        let query = "LET s = 1 FOR x IN s OUTBOUND @startId @@eCol RETURN x";
        let bindVars = {
          "@eCol": en,
          "startId": vertex.A
        };

        let result = db._query(query, bindVars).toArray();
        expect(result.length).toEqual(1);
        expect(result[0]._id).toEqual(vertex.B);
      });

      it("should be able to return a vast amount of results", function () {
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
        expect(result.length).toEqual(amount);
      });

      it("should be able to skip some results", function () {
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
        expect(result.length).toEqual(8);

        // Each of the 3 parts of this graph contains of 4 nodes, one connected to the start.
        // And 3 connected to the first one. As we do a depth first traversal we expect to skip
        // exactly one sub-tree. Therefor we expect exactly two sub-trees to be traversed.
        let seen = {};
        for (let r of result) {
          if (!seen.hasOwnProperty(r)) {
            seen[r] = true;
          }
        }
        expect(Object.keys(seen).length).toEqual(2);
      });

      it("should be able to handle many results per step", function () {
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
        expect(result.count()).toEqual(amount);
        while (result.hasNext()) {
          result.next();
          ++found;
        }
        // All elements must be enumerated
        expect(found).toEqual(amount);
      });

    });

  });

}());
