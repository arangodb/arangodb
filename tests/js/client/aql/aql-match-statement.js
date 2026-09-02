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

    // Run a one-hop `RETURN [v, e, w]` MATCH, assert every row's endpoints are
    // the edge's own _from/_to, and return the matched edge ids sorted. Checking
    // the bindings matters as much as the edge set: a lowering that filtered
    // correctly but bound `w` to the wrong document would pass an edges-only
    // assertion.
    const edgeIds = function (query) {
        const rows = db._query(query, {}, options).toArray();
        for (const [v, e, w] of rows) {
            assertEqual(v._id, e._from, e._id);
            assertEqual(w._id, e._to, e._id);
        }
        return rows.map((x) => x[1]._id).sort();
    };

    return {

        setUpAll: function () {
            db._createDatabase(database);
            db._useDatabase(database);

            db._create("vc");
            for (let i = 0; i < 100; i++) {
                db.vc.save({
                    _key: `v${i}`,
                    i,
                    j: i % 5,
                    profile: {name: `user${i}`, age: i % 40},
                    a: {b: {c: i, d: i + 100}},
                    "profile.name": `literal${i}`
                });
            }

            db._createEdgeCollection("ec");
            for (let i = 0; i < 50; i++) {
                db.ec.save({
                    _key: `e${i}`,
                    i,
                    j: i % 10,
                    meta: {since: i},
                    _from: `vc/v${2 * i}`,
                    _to: `vc/v${2 * i + 1}`
                });
            }

            db._createEdgeCollection("ec2");
            for (let i = 50; i < 60; i++) {
                db.ec2.save({_key: `e2${i}`, _from: `vc/v${i}`, _to: `vc/v${i + 1}`});
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

            // A second vertex collection reachable from vc, so a target label can
            // be shown to *exclude* a vertex: ec_cross leaves vc, ec_loops stays
            // inside it.
            db._create("vc_other");
            for (let i = 0; i < 10; i++) {
                db.vc_other.save({_key: `o${i}`, i});
            }
            db._createEdgeCollection("ec_cross");
            for (let i = 0; i < 10; i++) {
                db.ec_cross.save({_key: `x${i}`, _from: `vc/v${i}`, _to: `vc_other/o${i}`});
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

        testSelectVerticesWithProjection: function () {
            const result = db._query("MATCH (v :vc RETURN i) RETURN v", {}, options).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("j"));
                assertFalse(v.hasOwnProperty("_key"));
                assertFalse(v.hasOwnProperty("_rev"));
            }
        },

        testSelectVerticesWithMultipleProjections: function () {
            const result = db._query("MATCH (v :vc RETURN i, j) RETURN v", {}, options).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("i"));
                assertTrue(v.hasOwnProperty("j"));
                assertEqual(v.j, v.i % 5);
                assertFalse(v.hasOwnProperty("_key"));
            }
        },

        testSelectVerticesWithQuotedProjection: function () {
            const result = db._query('MATCH (v :vc RETURN "i") RETURN v', {}, options).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("j"));
            }
        },

        testSelectVerticesWithMissingProjectionAttribute: function () {
            const result = db._query("MATCH (v :vc RETURN missingAttr) RETURN v", {}, options).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("missingAttr"));
                assertEqual(v.missingAttr, null);
                assertFalse(v.hasOwnProperty("i"));
            }
        },

        testSelectVerticesWithSystemAttributeProjection: function () {
            const result = db._query("MATCH (v :vc RETURN _key, i) RETURN v", {}, options).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("_key"));
                assertTrue(v.hasOwnProperty("i"));
                assertEqual(v._id, `vc/${v._key}`);
                assertFalse(v.hasOwnProperty("j"));
            }
        },

        testSelectVerticesWithProjectionAndProperties: function () {
            const result = db._query("MATCH (v :vc {j: 0} RETURN i) RETURN v", {}, options).toArray();
            assertEqual(result.length, 20);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("i"));
                assertEqual(v.i % 5, 0);
                assertFalse(v.hasOwnProperty("j"));
            }
        },

        testSelectVerticesWithProjectionAndWhereClause: function () {
            // WHERE may access attributes that are not projected on the bound variable.
            const result = db._query("MATCH (v :vc WHERE v.j == 0 RETURN i) RETURN v", {}, options).toArray();
            assertEqual(result.length, 20);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("i"));
                assertEqual(v.i % 5, 0);
                assertFalse(v.hasOwnProperty("j"));
            }
        },

        testSelectVerticesWithProjectionPropertiesAndWhereClause: function () {
            const result = db._query(
                "MATCH (v :vc {j: 0} WHERE v.i % 10 == 0 RETURN i, j) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 10);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertEqual(v.j, 0);
                assertEqual(v.i % 10, 0);
                assertFalse(v.hasOwnProperty("_key"));
            }
        },

        testSelectEdges: function () {
            const result = db._query("MATCH (v :vc) -[ e :ec ]-> (w :vc) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
            }
        },

        testSelectEdgesWithMultipleEdgeTypes: function () {
            const result = db._query("MATCH (v :vc) -[ e :ec | ec2 ]-> (w :vc) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(result.length, 60);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
                assertTrue(e._id.startsWith("ec/") || e._id.startsWith("ec2/"));
            }
        },

        // Several edge collections force the segment to be lowered to a
        // traversal instead of a collection enumeration; the target vertex's
        // constraints must survive that lowering. Each case is cross-checked
        // against the single-collection spelling, which uses the other lowering.
        testSelectEdgesWithMultipleEdgeTypesAndTargetVertexProperties: function () {
            // single edge collection => join lowering, used here as the oracle
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec ]-> (w :vc {i: 51}) RETURN [v, e, w]"), ["ec/e25"]);
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec2 ]-> (w :vc {i: 51}) RETURN [v, e, w]"), ["ec2/e250"]);

            // several edge collections => traversal lowering
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc {i: 51}) RETURN [v, e, w]"),
                        ["ec/e25", "ec2/e250"]);

            // the target binding is the constrained vertex itself, not merely
            // the right edge set
            const rows = db._query("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc {i: 51}) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(rows.length, 2);
            for (const [v, e, w] of rows) {
                assertEqual(w._id, "vc/v51");
                assertEqual(w.i, 51);
            }
        },

        testSelectEdgesWithMultipleEdgeTypesAndTargetVertexWhereClause: function () {
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc WHERE w.i == 51) RETURN [v, e, w]"),
                        ["ec/e25", "ec2/e250"]);

            const rows = db._query("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc WHERE w.i == 51) RETURN [v, e, w]", {}, options).toArray();
            assertEqual(rows.length, 2);
            for (const [v, e, w] of rows) {
                assertEqual(w._id, "vc/v51");
            }
        },

        testSelectEdgesWithMultipleEdgeTypesAndTargetVertexPropertiesAndWhereClause: function () {
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc {i: 51} WHERE w.j == 1) RETURN [v, e, w]"),
                        ["ec/e25", "ec2/e250"]);

            // same vertex, contradictory WHERE
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc {i: 51} WHERE w.j == 2) RETURN [v, e, w]"), []);
        },

        testSelectEdgesWithMultipleEdgeTypesAndUnsatisfiableTargetVertexFilter: function () {
            // no vertex has i == 999, so neither spelling may return a row
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc {i: 999}) RETURN [v, e, w]"), []);
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc WHERE w.i == 999) RETURN [v, e, w]"), []);
        },

        testSelectEdgesWithMultipleEdgeTypesAndBothVertexFilters: function () {
            // start-vertex constraints already worked; assert they compose with
            // the target-vertex ones rather than replacing them. ec_loops holds
            // both a self-loop v3->v3 and the step v3->v4, so only the target
            // constraint can separate them -- a start-only filter would keep both.
            assertEqual(edgeIds("MATCH (v :vc {i: 3}) -[ e :ec_loops|ec2 ]-> (w :vc {i: 4}) RETURN [v, e, w]"),
                        ["ec_loops/e13"]);
            assertEqual(edgeIds("MATCH (v :vc {i: 3}) -[ e :ec_loops|ec2 ]-> (w :vc {i: 3}) RETURN [v, e, w]"),
                        ["ec_loops/e3"]);

            // target unconstrained beyond its collection: v3 has exactly these two
            assertEqual(edgeIds("MATCH (v :vc {i: 3}) -[ e :ec_loops|ec2 ]-> (w :vc) RETURN [v, e, w]"),
                        ["ec_loops/e13", "ec_loops/e3"]);
        },

        testSelectEdgesWithMultipleEdgeTypesAndTargetVertexCollection: function () {
            // ec_loops stays inside vc, ec_cross leaves it for vc_other. Multiple
            // edge collections force the traversal lowering, where the target
            // label is the only thing keeping each half out of the other result.
            const inside = edgeIds("MATCH (v :vc) -[ e :ec_loops|ec_cross ]-> (w :vc) RETURN [v, e, w]");
            assertEqual(inside.length, 20);
            assertTrue(inside.every((id) => id.startsWith("ec_loops/")), JSON.stringify(inside));

            const crossing = edgeIds("MATCH (v :vc) -[ e :ec_loops|ec_cross ]-> (w :vc_other) RETURN [v, e, w]");
            assertEqual(crossing.length, 10);
            assertTrue(crossing.every((id) => id.startsWith("ec_cross/")), JSON.stringify(crossing));

            // target label and property constraint compose
            assertEqual(edgeIds("MATCH (v :vc) -[ e :ec_loops|ec_cross ]-> (w :vc_other {i: 3}) RETURN [v, e, w]"),
                        ["ec_cross/x3"]);
        },

        testSelectEdgesWithCollectionBindParameterMultipleEdgeTypes: function () {
            const result = db._query("MATCH (v :vc) -[ e :@@ec1 | @@ec2 ]-> (w :vc) RETURN [v, e, w]",
                { "@ec1": "ec", "@ec2": "ec2" }, options).toArray();
            assertEqual(result.length, 60);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
            }
        },

        testSelectEdgesWithThreeEdgeTypes: function () {
            // union over 3 edge collections == sum of the individual counts (disjoint)
            const q3 = "MATCH (v :vc) -[ e :ec|ec2|ec_loops ]-> (w :vc) RETURN e._id";
            const three = db._query(q3, {}, options).toArray();
            const nEc = db._query("MATCH (v :vc) -[ e :ec ]-> (w :vc) RETURN e", {}, options).toArray().length;
            const nEc2 = db._query("MATCH (v :vc) -[ e :ec2 ]-> (w :vc) RETURN e", {}, options).toArray().length;
            const nLoops = db._query("MATCH (v :vc) -[ e :ec_loops ]-> (w :vc) RETURN e", {}, options).toArray().length;
            assertEqual(three.length, nEc + nEc2 + nLoops);
            const prefixes = new Set(three.map((id) => id.split("/")[0]));
            assertTrue(prefixes.has("ec") && prefixes.has("ec2") && prefixes.has("ec_loops"),
                       JSON.stringify([...prefixes]));
        },

        testSelectAnyEdgesWithMultipleEdgeTypes: function () {
            // any-direction multi-type union == sum of the individual any-direction counts
            const union = db._query("MATCH (v :vc) -[ e :ec|ec2 ]- (w :vc) RETURN e", {}, options).toArray().length;
            const nEc = db._query("MATCH (v :vc) -[ e :ec ]- (w :vc) RETURN e", {}, options).toArray().length;
            const nEc2 = db._query("MATCH (v :vc) -[ e :ec2 ]- (w :vc) RETURN e", {}, options).toArray().length;
            assertEqual(union, nEc + nEc2);
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

        testSelectEdgesWithCollectionBindParameterEdgeType: function () {
            const result = db._query("MATCH (v :vc) -[ e : @@ec ]-> (w :vc) RETURN [v, e, w]", {"@ec": "ec"}, options).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
            }
        },

        testSelectVerticesWithCollectionBindParameterLabel: function () {
            const result = db._query("MATCH (v : @@vc) RETURN v", {"@vc": "vc"}, options).toArray();
            assertEqual(result.length, 100);
            const ids = new Set(result.map(v => v._id));
            assertEqual(ids.size, 100);
        },

        testSelectWithCollectionBindParametersForLabelsAndEdgeType: function () {
            const result = db._query("MATCH (v :@@vc) -[ e : @@ec ]-> (w :@@vc) RETURN [v, e, w]", {"@vc": "vc", "@ec": "ec"}, options).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
            }
        },

        testSelectEdgesWithMissingCollectionBindParameterEdgeType: function () {
            try {
                db._query("MATCH (v :vc) -[ e : @@ec ]-> (w :vc) RETURN [v, e, w]", {}, options).toArray();
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code);
            }
        },

        testValueBindParameterRejectedAsEdgeType: function () {
            // a value bind parameter (@name) is not accepted as an edge type;
            // only a collection bind parameter (@@name) is. parsing fails.
            try {
                db._query("MATCH (v :vc) -[ e : @ec ]-> (w :vc) RETURN [v, e, w]", {ec: "ec"}, options).toArray();
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
            }
        },

        testValueBindParameterRejectedAsVertexLabel: function () {
            try {
                db._query("MATCH (v : @vc) RETURN v", {vc: "vc"}, options).toArray();
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

        testSelectEdgesWithProjection: function () {
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

        testSelectEdgesWithProjectionAndWhere: function () {
            // WHERE may access attributes that are not projected on the bound variable.
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

        testMatchPathVariableWithEdgeProjection: function () {
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

        testSelectEdgesWithVertexProjections: function () {
            const result = db._query(
                "MATCH (v :vc RETURN i) -[ e :ec ]-> (w :vc RETURN i, j) RETURN [v, e, w]",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);

                assertTrue(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("j"));
                assertFalse(v.hasOwnProperty("_key"));

                assertTrue(e.hasOwnProperty("_key"));
                assertTrue(e.hasOwnProperty("j"));

                assertTrue(w.hasOwnProperty("i"));
                assertTrue(w.hasOwnProperty("j"));
                assertFalse(w.hasOwnProperty("_key"));
            }
        },

        testSelectEdgesWithLeftVertexProjectionOnly: function () {
            const result = db._query(
                "MATCH (v :vc RETURN i) -[ e :ec ]-> (w :vc) RETURN [v, e, w]",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
                assertTrue(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("j"));
                assertTrue(w.hasOwnProperty("j"));
                assertTrue(w.hasOwnProperty("_key"));
            }
        },

        testSelectEdgesWithRightVertexProjectionOnly: function () {
            const result = db._query(
                "MATCH (v :vc) -[ e :ec ]-> (w :vc RETURN i) RETURN [v, e, w]",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._from);
                assertEqual(w._id, e._to);
                assertTrue(v.hasOwnProperty("j"));
                assertTrue(v.hasOwnProperty("_key"));
                assertTrue(w.hasOwnProperty("i"));
                assertFalse(w.hasOwnProperty("j"));
                assertFalse(w.hasOwnProperty("_key"));
            }
        },

        testSelectInboundEdgesWithVertexProjections: function () {
            const result = db._query(
                "MATCH (v :vc RETURN i) <-[ e :ec ]- (w :vc RETURN j) RETURN [v, e, w]",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const [v, e, w] of result) {
                assertEqual(v._id, e._to);
                assertEqual(w._id, e._from);
                assertTrue(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("j"));
                assertTrue(w.hasOwnProperty("j"));
                assertFalse(w.hasOwnProperty("i"));
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

        testSelectEdgeLoopsWithVertexProjection: function () {
            const result = db._query(
                "MATCH (v :vc RETURN i) -[ e :ec_loops ]-> (v) RETURN [v, e]",
                {},
                options
            ).toArray();
            assertEqual(result.length, 10);

            for (const [v, e] of result) {
                assertEqual(v._id, e._from);
                assertEqual(v._id, e._to);
                assertTrue(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("j"));
                assertTrue(e.hasOwnProperty("j"));
            }
        },

        testEmptyVertexProjectionParseError: function () {
            try {
                db._query("MATCH (v :vc RETURN) RETURN v", {}, options).toArray();
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
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

        testMatchPathVariableWithVertexProjections: function () {
            const result = db._query(
                "MATCH p = (v :vc RETURN i) -[ e :ec ]-> (w :vc RETURN j) RETURN p",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const {edges, vertices} of result) {
                assertEqual(edges.length, 1);
                assertEqual(vertices.length, 2);
                assertEqual(edges[0]._from, vertices[0]._id);
                assertEqual(edges[0]._to, vertices[1]._id);

                assertTrue(vertices[0].hasOwnProperty("i"));
                assertFalse(vertices[0].hasOwnProperty("j"));
                assertTrue(vertices[1].hasOwnProperty("j"));
                assertFalse(vertices[1].hasOwnProperty("i"));
                assertTrue(edges[0].hasOwnProperty("j"));
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

        testSelectVerticesWithNestedProjection: function () {
            const result = db._query(
                "MATCH (v :vc RETURN profile.name) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("profile"));
                assertTrue(v.profile.hasOwnProperty("name"));
                assertTrue(v.profile.name.startsWith("user"));
                assertFalse(v.profile.hasOwnProperty("age"));
                assertFalse(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("j"));
                assertFalse(v.hasOwnProperty("profile.name"));
            }
        },

        testSelectVerticesWithDeepNestedProjection: function () {
            const result = db._query(
                "MATCH (v :vc RETURN a.b.c) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertEqual(typeof v.a.b.c, "number");
                assertFalse(v.hasOwnProperty("i"));
            }
        },

        testSelectVerticesWithSiblingNestedProjections: function () {
            const result = db._query(
                "MATCH (v :vc RETURN profile.name, profile.age) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("profile"));
                assertTrue(v.profile.hasOwnProperty("name"));
                assertTrue(v.profile.hasOwnProperty("age"));
                assertEqual(typeof v.profile.age, "number");
                assertFalse(v.hasOwnProperty("i"));
            }
        },

        testSelectVerticesWithSharedNestedParent: function () {
            // RETURN a.b.c, a.b.d → one shared a.b object with both leaves.
            const result = db._query(
                "MATCH (v :vc RETURN a.b.c, a.b.d) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("a"));
                assertTrue(v.a.hasOwnProperty("b"));
                assertEqual(typeof v.a.b.c, "number");
                assertEqual(v.a.b.d, v.a.b.c + 100);
                assertFalse(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("profile"));
            }
        },

        testSelectVerticesWithFlatAndNestedProjection: function () {
            const result = db._query(
                "MATCH (v :vc RETURN i, profile.name) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("i"));
                assertEqual(typeof v.i, "number");
                assertEqual(v.profile.name, `user${v.i}`);
                assertFalse(v.hasOwnProperty("j"));
                assertFalse(v.profile.hasOwnProperty("age"));
            }
        },

        testSelectEdgesWithNestedProjection: function () {
            const result = db._query(
                "MATCH (u :vc)-[e :ec RETURN meta.since]->(v :vc) RETURN e",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const e of result) {
                assertTrue(e.hasOwnProperty("_id"));
                assertTrue(e.hasOwnProperty("_from"));
                assertTrue(e.hasOwnProperty("_to"));
                assertTrue(e.hasOwnProperty("meta"));
                assertEqual(typeof e.meta.since, "number");
                assertFalse(e.hasOwnProperty("i"));
                assertFalse(e.hasOwnProperty("j"));
            }
        },

        testSelectVerticesWithAliasUnchangedByNested: function () {
            // Aliasing stays flat even when the RHS walks a nested path.
            const result = db._query(
                "MATCH (v :vc RETURN name = v.profile.name) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("name"));
                assertTrue(v.name.startsWith("user"));
                assertFalse(v.hasOwnProperty("profile"));
            }
        },

        testSelectVerticesWithNestedAndAliasComposition: function () {
            const result = db._query(
                "MATCH (v :vc RETURN profile.name, label = v.i) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertEqual(v.profile.name, `user${v.label}`);
                assertFalse(v.hasOwnProperty("i"));
            }
        },

        testSelectVerticesWithQuotedDottedProjection: function () {
            // Quoted "profile.name" is a single literal key, not nested hierarchy.
            const result = db._query(
                'MATCH (v :vc RETURN "profile.name") RETURN v',
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("profile.name"));
                assertTrue(v["profile.name"].startsWith("literal"));
                assertFalse(v.hasOwnProperty("profile"));
            }
        },

        testSelectVerticesWithMissingNestedProjection: function () {
            const result = db._query(
                "MATCH (v :vc RETURN profile.missingAttr) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("profile"));
                assertEqual(v.profile.missingAttr, null);
                assertFalse(v.hasOwnProperty("i"));
            }
        },

        testSelectVerticesWithPrefixOverlapKeepsShorter: function () {
            // profile subsumes profile.name — emit the whole profile object.
            const result = db._query(
                "MATCH (v :vc RETURN profile, profile.name) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("profile"));
                assertTrue(v.profile.hasOwnProperty("name"));
                assertTrue(v.profile.hasOwnProperty("age"));
                assertFalse(v.hasOwnProperty("i"));
            }
        },

        testSelectVerticesWithPrefixOverlapKeepsShorterDeep: function () {
            // RETURN a.b, a.b.c.d → keep only a.b (shorter prefix wins).
            const result = db._query(
                "MATCH (v :vc RETURN a.b, a.b.c.d) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("a"));
                assertTrue(v.a.hasOwnProperty("b"));
                // Whole a.b object kept (both c and d leaves), not only a.b.c.d.
                assertEqual(typeof v.a.b.c, "number");
                assertEqual(v.a.b.d, v.a.b.c + 100);
                assertFalse(v.hasOwnProperty("i"));
                assertFalse(v.hasOwnProperty("profile"));
            }
        },

        testSelectVerticesWithPrefixOverlapNestedLeaf: function () {
            // RETURN profile.name, profile.name.first → just profile.name.
            const result = db._query(
                "MATCH (v :vc RETURN profile.name, profile.name.first) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(v.hasOwnProperty("_id"));
                assertTrue(v.hasOwnProperty("profile"));
                assertTrue(typeof v.profile.name === "string");
                assertTrue(v.profile.name.startsWith("user"));
                assertFalse(v.profile.hasOwnProperty("age"));
                assertFalse(v.hasOwnProperty("i"));
            }
        },

        testProjectionIgnoresNestedSystemAttributePath: function () {
            // Nested keeps under system attrs must not overwrite scalar _id.
            const result = db._query(
                "MATCH (v :vc RETURN _id.foo, i) RETURN v",
                {},
                options
            ).toArray();
            assertEqual(result.length, 100);

            for (const v of result) {
                assertTrue(typeof v._id === "string");
                assertTrue(v._id.startsWith("vc/"));
                assertEqual(typeof v.i, "number");
                assertFalse(v.hasOwnProperty("foo"));
            }
        },

        testEdgeProjectionIgnoresNestedSystemAttributePath: function () {
            const result = db._query(
                "MATCH (u :vc)-[e :ec RETURN _from.x, i]->(v :vc) RETURN e",
                {},
                options
            ).toArray();
            assertEqual(result.length, 50);

            for (const e of result) {
                assertTrue(typeof e._id === "string");
                assertTrue(typeof e._from === "string");
                assertTrue(typeof e._to === "string");
                assertTrue(e._from.startsWith("vc/"));
                assertEqual(typeof e.i, "number");
                assertFalse(e.hasOwnProperty("x"));
            }
        },

        testProjectionAliasCollidesWithBareKeep: function () {
            try {
                db._query(
                    "MATCH (v :vc RETURN i, i = v.j) RETURN v",
                    {},
                    options
                );
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
            }
        },

        testProjectionAliasCollidesWithNestedKeepRoot: function () {
            // Nested keep claims top-level key "profile"; alias must not reuse it.
            try {
                db._query(
                    "MATCH (v :vc RETURN profile.name, profile = v.i) RETURN v",
                    {},
                    options
                );
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
            }
        },

        testProjectionDuplicateAliasNames: function () {
            try {
                db._query(
                    "MATCH (v :vc RETURN a = v.i, a = v.j) RETURN v",
                    {},
                    options
                );
                fail();
            } catch (err) {
                assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
            }
        },

    };
}

jsunity.run(aqlMatchStatementTestSuite);

return jsunity.done();

