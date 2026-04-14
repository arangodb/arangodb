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

    const database = "UnitTestsAqlMatchStatementVariableLength";
    const options = { matchStatement: "experimental" };

    return {

        setUpAll: function () {
            db._createDatabase(database);
            db._useDatabase(database);

            db._create("vc");
          /*
            for (let i = 0; i < 100; i++) {
                db.vc.save({_key: `v${i}`, i, j: i % 5});
            }
*/
            db._createEdgeCollection("ec");
          /*
            for (let i = 0; i < 50; i++) {
                db.ec.save({_key: `e${i}`, i, j: i % 10, _from: `vc/v${2 * i}`, _to: `vc/v${2 * i + 1}`});
            }
            */

            db._createEdgeCollection("ec_loops");
          /*
            for (let i = 0; i < 10; i++) {
                db.ec_loops.save({_key: `e${i}`, i, j: i % 10, _from: `vc/v${i}`, _to: `vc/v${i}`});
                db.ec_loops.save({_key: `e${i + 10}`, i, j: i % 10, _from: `vc/v${i}`, _to: `vc/v${i + 1}`});
            }
            */

            db._createEdgeCollection("ec_paths");
          /*
            for (let i = 0; i < 20; i++) {
                for (let j = 0; j < 4; j++) {
                    db.ec_paths.save({_key: `e${i}_${j}`, i, j, _from: `vc/v${5*i + j}`, _to: `vc/v${5*i + j + 1}`});
                }
            }
            */
        },

        tearDownAll: function () {
            db._useDatabase("_system");
            db._dropDatabase(database);
        },

        testMatchVariableLengthPath: function() {
          /*
            per (current) definition the below queries are expected
            to be equivalent

          MATCH (v:vc) -[ e:ec_paths * 2..3 ]-> (w:vc)
            RETURN [v, e, w]

          FOR v IN vc
            FOR w,x,e IN 2..3 OUTBOUND v ec
              FILTER IS_SAME_COLLECTION(w._id, "vc")
              RETURN [v, e, w]
          */ 

          const match_query = aql`MATCH (v:vc) -[ e:ec_paths * 1..3 ]-> (w:vc)
                              RETURN [v, e, w]`;
          const match_result = db._query(match_query, {}, options).toArray();

          const traversal_query = aql`FOR v IN vc FOR w,x,e IN 1..3 OUTBOUND v ec_paths
                              FILTER IS_SAME_COLLECTION(w._id, "vc")
                              RETURN [v, e, w]`;
          const traversal_result = db._query(traversal_query, {}, options).toArray();

          assertEqual(match_result, traversal_result); // , JSON.stringify(result));
        },
        testMatchVariableLengthPath2: function() {
          /*
            per (current) definition the below queries are expected
            to be equivalent

            FOR w IN vc
              MATCH (v:vc) -[ e:ec_paths * 2..3 ]-> (w)
                RETURN [v, e, w]

            FOR w IN ovc
              FOR v IN vc
                FOR #3,_,e IN 2..3 OUTBOUND v ec_paths
                  FILTER #3._id == w._id
                  RETURN [v, e, w]
          */ 

          const match_query = aql`FOR w IN vc
                                    MATCH (v:vc) -[ e:ec_paths * 2..3 ]-> (w)
                                      RETURN [v, e, w]`;
          const match_result = db._query(match_query, {}, options).toArray();

          const traversal_query = aql`FOR w in vc
                                        FOR v IN vc
                                          FOR temp,x,e IN 2..3 OUTBOUND v ec_paths
                                            FILTER temp._id == w._id
                                            RETURN [v, e, w]`;
          const traversal_result = db._query(traversal_query, {}, options).toArray();
          assertEqual(match_result, traversal_result); // , JSON.stringify(result));
        },
        testMatchVariableLengthPathInbound: function() {
          /*
            FOR w IN vc
              MATCH (v:vc) <-[ e:ec_paths * 2..3 ]- (w)
                RETURN [v, e, w]

            FOR w IN ovc
              FOR v IN vc
                FOR #3,_,e IN 2..3 INBOUND v ec_paths
                  FILTER #3._id == w._id
                  RETURN [v, e, w]
          */ 

          const match_query = aql`FOR w IN vc
                                    MATCH (v:vc) <-[ e:ec_paths * 2..3 ]- (w)
                                      RETURN [v, e, w]`;
          const match_result = db._query(match_query, {}, options).toArray();

          const traversal_query = aql`FOR w in vc
                                        FOR v IN vc
                                          FOR temp,x,e IN 2..3 INBOUND v ec_paths
                                            FILTER temp._id == w._id
                                            RETURN [v, e, w]`;
          const traversal_result = db._query(traversal_query, {}, options).toArray();
          assertEqual(match_result, traversal_result); // , JSON.stringify(result));
        },
        testMatchVariableLengthPathAny: function() {
          /*
            FOR w IN vc
              MATCH (v:vc) <-[ e:ec_paths * 2..3 ]- (w)
                RETURN [v, e, w]

            FOR w IN ovc
              FOR v IN vc
                FOR #3,_,e IN 2..3 INBOUND v ec_paths
                  FILTER #3._id == w._id
                  RETURN [v, e, w]
          */ 

          const match_query = aql`FOR w IN vc
                                    MATCH (v:vc) -[ e:ec_paths * 2..3 ]- (w)
                                      RETURN [v, e, w]`;
          const match_result = db._query(match_query, {}, options).toArray();

          const traversal_query = aql`FOR w in vc
                                        FOR v IN vc
                                          FOR temp,x,e IN 2..3 ANY v ec_paths
                                            FILTER temp._id == w._id
                                            RETURN [v, e, w]`;
          const traversal_result = db._query(traversal_query, {}, options).toArray();
          assertEqual(match_result, traversal_result); // , JSON.stringify(result));
        },
    };
}

jsunity.run(aqlMatchStatementVariableLengthTestSuite);

return jsunity.done();



