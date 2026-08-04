/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print, fail */

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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
"use strict";

const jsunity = require("jsunity");
const {aql, db, errors} = require("@arangodb");

function aqlMatchStatementTestSuite() {

    const database = "UnitTestsAqlMatchStatement";
    const options = { matchStatement: "experimental" };

    return {

        setUpAll: function () {
            db._createDatabase(database);
            db._useDatabase(database);

            db._create("vc");
            for (let i = 0; i < 100; i++) {
                db.vc.save({_key: `v${i}`, i, j: i % 5});
            }

            db._createEdgeCollection("ec");
            for (let i = 0; i < 50; i++) {
                db.ec.save({_key: `e${i}`, i, j: i % 10, _from: `vc/v${2 * i}`, _to: `vc/v${2 * i + 1}`});
            }

            db._createEdgeCollection("ec_loops");
            for (let i = 0; i < 10; i++) {
                db.ec_loops.save({_key: `e${i}`, i, j: i % 10, _from: `vc/v${i}`, _to: `vc/v${i}`});
                db.ec_loops.save({_key: `e${i + 10}`, i, j: i % 10, _from: `vc/v${i}`, _to: `vc/v${i + 1}`});
            }

            db._createEdgeCollection("ec_paths");
            for (let i = 0; i < 20; i++) {
                for (let j = 0; j < 4; j++) {
                    db.ec_paths.save({_key: `e${i}_${j}`, i, j, _from: `vc/v${5*i + j}`, _to: `vc/v${5*i + j + 1}`});
                }
            }
        },

        tearDownAll: function () {
            db._useDatabase("_system");
            db._dropDatabase(database);
        },

        testDisabledByDefault: function () {
            try {
                // check that by default the match statement is disabled
                db._query('MATCH (v : vc)');
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_NOT_IMPLEMENTED.code);
            }
        },

        testInvalidOptionValue: function () {
            try {
                // check that by default the match statement is disabled
                db._query('MATCH (v : vc)', {}, {matchStatement: "invalid"});
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_BAD_PARAMETER.code);
            }
        },

        testSelectVertices: function () {
            const result = db._query("MATCH (v :vc) RETURN v", {}, options).toArray();
            assertEqual(result.length, 100);
            const ids = new Set(result.map(v => v._id));
            assertEqual(ids.size, 100);
        },

        testSelectVerticesWithProperties: function () {
            const result = db._query("MATCH (v :vc {j: 0}) RETURN v", {}, options).toArray();
            assertEqual(result.length, 20);
            const ids = new Set(result.filter(v => v.j === 0).map(v => v._id));
            assertEqual(ids.size, 20);
        },

        testSelectVerticesWithWhereClause: function () {
            const result = db._query("MATCH (v :vc WHERE v.i % 5 == 0) RETURN v", {}, options).toArray();
            assertEqual(result.length, 20);
            const ids = new Set(result.filter(v => v.j === 0).map(v => v._id));
            assertEqual(ids.size, 20);
        },

        testSelectVerticesWithPropertiesAndWhereClause: function () {
            const result = db._query("MATCH (v :vc {j: 0} WHERE v.i % 10 == 0) RETURN v", {}, options).toArray();
            assertEqual(result.length, 10);
            const ids = new Set(result.filter(v => v.j === 0 && v.i % 10 === 0).map(v => v._id));
            assertEqual(ids.size, 10);
        },

        testSelectEdges: function () {
            const result = db._query("MATCH (v :vc) -[ e :ec ]-> (w :vc) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
            }
        },

        testSelectInboundEdges: function () {
            const result = db._query("MATCH (v :vc) <-[ e :ec ]- (w :vc) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._to);
                assertEqual(w._id, e._from);
            }
        },

        testSelectAnyEdges: function () {
            const result = db._query("MATCH (v :vc) -[ e :ec ]- (w :vc) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(result.length, 100);

            for (const [v, e, w] of result) {
                assertTrue([e._from, e._to].includes(v._id));
                assertTrue([e._from, e._to].includes(w._id));
            }
        },

        testDoubleEndedEdgesError: function () {
            try {
                const result = db._query("MATCH (v :vc) <-[ e :ec ]-> (w :vc) RETURN [v, e, w]", {}, options).toArray();
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
            }
        },

        testSelectEdgesWithProperties: function () {
            const result = db._query("MATCH (v :vc) -[ e :ec {j: 0}]-> (w :vc) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(result.length, 5);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
                assertEqual(e.j, 0);
            }
        },

        testSelectEdgesWithWhereClause: function () {
            const result = db._query("MATCH (v :vc) -[ e :ec WHERE e.i % 10 == 0]-> (w :vc) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(result.length, 5);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
                assertEqual(e.j, 0);
            }
        },

        testSelectVerticesWithKeep: function () {
            const result = db._query("MATCH (v :vc RETURN i) RETURN v", {}, options).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("j"));
                assertFalse(v.hasOwnProperty("_key"));
            }
        },

        testSelectEdgesWithKeep: function () {
            const result = db._query(
                "MATCH (u :vc)-[e :ec RETURN i]->(v :vc) RETURN e",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const e of result) {
                assertTrue(e.hasOwnProperty("_id"));
                assertTrue(e.hasOwnProperty("_from"));
                assertTrue(e.hasOwnProperty("_to"));
                assertTrue(e.hasOwnProperty("i"));
                assertFalse(e.hasOwnProperty("j"));
                assertFalse(e.hasOwnProperty("_key"));
            }
        },

        testSelectEdgesWithKeepAndWhere: function () {
            // WHERE may access attributes that are not kept on the bound variable.
            const result = db._query(
                "MATCH (u :vc)-[e :ec WHERE e.j == 0 RETURN i]->(v :vc) RETURN e",
                {},
                options
            ).toArray();
            assertEqual(result.length, 5);

            for (const e of result) {
                assertTrue(e.hasOwnProperty("_id"));
                assertTrue(e.hasOwnProperty("_from"));
                assertTrue(e.hasOwnProperty("_to"));
                assertTrue(e.hasOwnProperty("i"));
                assertFalse(e.hasOwnProperty("j"));
            }
        },

        testMatchPathVariableWithEdgeKeep: function () {
            const result = db._query(
                "MATCH p = (v :vc) -[ e :ec RETURN i ]-> (w :vc) RETURN p",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const {edges, vertices} of result) {
                assertEqual(edges.length, 1);
                assertEqual(vertices.length, 2);
                assertEqual(edges[0]._from, vertices[0]._id);
                assertEqual(edges[0]._to, vertices[1]._id);
                assertTrue(edges[0].hasOwnProperty("i"));
                assertFalse(edges[0].hasOwnProperty("j"));
            }
        },

        testSelectVerticesWithAlias: function () {
            // Flatten / alias: expression is evaluated in normal query scope,
            // so the projected variable must be referenced explicitly (v.i).
            const result = db._query(
                "MATCH (v :vc RETURN j, idx = v.i) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("j"));
                assertTrue(v.hasOwnProperty("idx"));
                assertEqual(v.idx % 5, v.j);
                assertFalse(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("_key"));
            }
        },

        testSelectVerticesWithCrossVariableAlias: function () {
            // Alias expressions may reference other already-bound pattern vars.
            // Projection is on w so v and e are in scope when parsing the expr.
            const result = db._query(
                "MATCH (v :vc)-[e :ec]->(w :vc RETURN j, total = v.i + w.i) RETURN [v, w]",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const [v, w] of result) {
                // v is full document (no projection); w is projected
                assertTrue(v.hasOwnProperty("i"));
                assertTrue(w.hasOwnProperty("_id"));
                assertTrue(w.hasOwnProperty("j"));
                assertTrue(w.hasOwnProperty("total"));
                // fixture: edges connect vc/v{2k} -> vc/v{2k+1}
                assertEqual(w.total, v.i + (v.i + 1));
                assertFalse(w.hasOwnProperty("i"));
                assertFalse(w.hasOwnProperty("_key"));
            }
        },

        testSelectEdgesWithAlias: function () {
            const result = db._query(
                "MATCH (u :vc)-[e :ec RETURN j, num = e.i]->(v :vc) RETURN e",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const e of result) {
                assertTrue(e.hasOwnProperty("_id"));
                assertTrue(e.hasOwnProperty("_from"));
                assertTrue(e.hasOwnProperty("_to"));
                assertTrue(e.hasOwnProperty("j"));
                assertTrue(e.hasOwnProperty("num"));
                assertEqual(e.num % 10, e.j);
                assertFalse(e.hasOwnProperty("i"));
                assertFalse(e.hasOwnProperty("_key"));
            }
        },

        testSelectEdgesWithAliasReferencingVertex: function () {
            const result = db._query(
                "MATCH (u :vc)-[e :ec RETURN fromI = u.i, edgeI = e.i]->(v :vc) RETURN e",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const e of result) {
                assertTrue(e.hasOwnProperty("_id"));
                assertTrue(e.hasOwnProperty("_from"));
                assertTrue(e.hasOwnProperty("_to"));
                assertTrue(e.hasOwnProperty("fromI"));
                assertTrue(e.hasOwnProperty("edgeI"));
                // fixture: edge i connects vc/v{2i} -> vc/v{2i+1}
                assertEqual(e.fromI, 2 * e.edgeI);
                assertFalse(e.hasOwnProperty("i"));
                assertFalse(e.hasOwnProperty("j"));
            }
        },

        testMatchPathVariableWithAlias: function () {
            const result = db._query(
                "MATCH p = (v :vc RETURN idx = v.i) -[ e :ec RETURN num = e.i ]-> (w :vc) RETURN p",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const {edges, vertices} of result) {
                assertEqual(edges.length, 1);
                assertEqual(vertices.length, 2);
                assertEqual(edges[0]._from, vertices[0]._id);
                assertEqual(edges[0]._to, vertices[1]._id);
                assertTrue(vertices[0].hasOwnProperty("idx"));
                assertFalse(vertices[0].hasOwnProperty("i"));
                assertTrue(edges[0].hasOwnProperty("num"));
                assertFalse(edges[0].hasOwnProperty("i"));
            }
        },

        testSelectEdgeLoops: function () {
            const result = db._query("MATCH (v :vc) -[ e :ec_loops ]-> (v) RETURN [v, e]", {}, options).toArray();
            assertEqual(result.length, 10);

            for (const [v, e] of result) {
                assertEqual(v._id, e._from);
                assertEqual(v._id, e._to);
            }
        },

        testMatchPathVariable: function () {
            const result = db._query("MATCH p = (v :vc) -[ e :ec ]-> (w :vc) RETURN p", {}, options).toArray();
            assertEqual(result.length, 50);

            for (const {edges, vertices} of result) {
                assertEqual(edges.length, 1);
                assertEqual(vertices.length, 2);
                assertEqual(edges[0]._from, vertices[0]._id);
                assertEqual(edges[0]._to, vertices[1]._id);
            }
        },

        testMatchPaths: function () {
            const query = `
                MATCH p = (v_0 :vc) -[ :ec_paths ]-> (v_1 :vc) 
                                    -[ :ec_paths ]-> (v_2 :vc) 
                                    -[ :ec_paths ]-> (v_3 :vc) 
                                    -[ :ec_paths ]-> (v_4 :vc) 
                RETURN p
            `;
            const result = db._query(query, {}, options).toArray();
            assertEqual(result.length, 100 / 5);

            for (const {edges, vertices} of result) {
                assertEqual(edges.length, 4);
                assertEqual(vertices.length, 5);
                for (let i = 0; i < 4; i++) {
                    assertEqual(edges[i]._from, vertices[i]._id);
                    assertEqual(edges[i]._to, vertices[i+1]._id);
                }
            }
        },

        testMatchPathsWithVars: function () {
            const query = `
                FOR v_0 IN vc
                MATCH p = (v_0) -[ :ec_paths ]-> (v_1 :vc)
                                -[ :ec_paths ]-> (v_2 :vc)
                                -[ :ec_paths ]-> (v_3 :vc)
                                -[ :ec_paths ]-> (v_4 :vc)
                RETURN p
            `;
            const result = db._query(query, {}, options).toArray();
            assertEqual(result.length, 100 / 5);

            for (const {edges, vertices} of result) {
                assertEqual(edges.length, 4);
                assertEqual(vertices.length, 5);
                for (let i = 0; i < 4; i++) {
                    assertEqual(edges[i]._from, vertices[i]._id);
                    assertEqual(edges[i]._to, vertices[i+1]._id);
                }
            }
        },
    };
}

jsunity.run(aqlMatchStatementTestSuite);

return jsunity.done();

