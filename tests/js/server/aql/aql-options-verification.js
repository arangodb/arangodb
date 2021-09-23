/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertMatch, fail, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for invalid OPTIONS attributes
///
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const db = internal.db;
const jsunity = require("jsunity");
const errors = internal.errors;

function aqlOptionsVerificationSuite () {
  const cn = 'UnitTestsCollection';

  let checkQueries = (operation, queries) => {
    queries.forEach((query) => {
      let result = AQL_EXPLAIN(query[0]);
      assertTrue(Array.isArray(result.warnings));
      if (!query[1]) {
        assertEqual(0, result.warnings.length, query);
      } else {
        assertEqual(1, result.warnings.length, query);
        assertEqual(errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code, result.warnings[0].code, query);
        assertMatch(RegExp(operation), result.warnings[0].message, query);
        assertMatch(RegExp(query[1]), result.warnings[0].message, query);
      }
    });
  };

  return {

    setUpAll : function () {
      db._drop(cn);
      db._create(cn);
      
      db._drop(cn + "Edge");
      db._createEdgeCollection(cn + "Edge");

      db._dropView(cn + "View");
      db._createView(cn + "View", "arangosearch", { links: { [cn] : { includeAllFields: true } } });
    },

    tearDownAll : function () {
      db._dropView(cn + "View");
      db._drop(cn + "Edge");
      db._drop(cn);
    },
    
    testForCollection : function () {
      const prefix = "FOR doc IN " + cn + " OPTIONS ";
      const queries = [
        [ prefix + "{ indexHint: 'primary' } RETURN 1" ],
        [ prefix + "{ indexHint: ['primary'] } RETURN 1" ],
        [ prefix + "{ indexHint: ['primary'], forceIndexHint: false } RETURN 1" ],
        [ prefix + "{ forceIndexHint: false } RETURN 1" ],
        [ prefix + "{ forceIndexHint: 'meow' } RETURN 1", "forceIndexHint" ],
        [ prefix + "{ indexHint: false } RETURN 1", "indexHint" ],
        [ prefix + "{ indexHint: [] } RETURN 1", "indexHint" ],
        [ prefix + "{ waitForSync: true } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: false } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: +1 } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: -1 } RETURN 1", "waitForSync" ],
        [ prefix + "{ method: 'hash' } RETURN 1", "method" ],
        [ prefix + "{ tititi: 'piff' } RETURN 1", "tititi" ],
      ];

      checkQueries("FOR", queries);
    },
    
    testForView : function () {
      const prefix = "FOR doc IN " + cn + "View SEARCH doc.testi == 123 OPTIONS ";
      const queries = [
        [ prefix + "{ collections: ['" + cn + "'] } RETURN 1" ],
        [ prefix + "{ waitForSync: true } RETURN 1" ],
        [ prefix + "{ waitForSync: false } RETURN 1" ],
        [ prefix + "{ noMaterialization: true } RETURN 1" ],
        [ prefix + "{ noMaterialization: false } RETURN 1" ],
        [ prefix + "{ countApproximate: 'exact' } RETURN 1" ],
        [ prefix + "{ countApproximate: 'cost' } RETURN 1" ],
        [ prefix + "{ conditionOptimization: 'auto' } RETURN 1" ],
        [ prefix + "{ conditionOptimization: 'nodnf' } RETURN 1" ],
        [ prefix + "{ conditionOptimization: 'noneg' } RETURN 1" ],
        [ prefix + "{ conditionOptimization: 'none' } RETURN 1" ],

        [ prefix + "{ method: 'hash' } RETURN 1", "method" ],
        [ prefix + "{ tititi: 'piff' } RETURN 1", "tititi" ],
      ];

      // arangosearch only likes boolean attributes for its waitForSync value
      try {
        AQL_EXPLAIN(prefix + "{ waitForSync: +1 } RETURN 1");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
      try {
        AQL_EXPLAIN(prefix + "{ waitForSync: -1 } RETURN 1");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testTraversal : function () {
      const prefix = "FOR v, e, p IN 1..1 OUTBOUND '" + cn + "/test' " + cn + "Edge OPTIONS ";
      const queries = [
        [ prefix + "{ bfs: false } RETURN 1" ],
        [ prefix + "{ bfs: true } RETURN 1" ],
        [ prefix + "{ order: 'bfs' } RETURN 1" ],
        [ prefix + "{ order: 'dfs' } RETURN 1" ],
        [ prefix + "{ order: 'weighted' } RETURN 1" ],
        [ prefix + "{ defaultWeight: 17 } RETURN 1" ],
        [ prefix + "{ weightAttribute: 'testi' } RETURN 1" ],
        [ prefix + "{ uniqueVertices: 'path' } RETURN 1" ],
        [ prefix + "{ uniqueVertices: 'none' } RETURN 1" ],
        [ prefix + "{ parallelism: 4 } RETURN 1" ],
        [ prefix + "{ indexHint: 'primary' } RETURN 1", "indexHint" ],
        [ prefix + "{ defaultWeight: true } RETURN 1", "defaultWeight" ],
        [ prefix + "{ weightAttribute: ['testi'] } RETURN 1", "weightAttribute" ],
        [ prefix + "{ uniqueVertices: true } RETURN 1", "uniqueVertices" ],
        [ prefix + "{ uniqueEdges: true } RETURN 1", "uniqueEdges" ],
        [ prefix + "{ bfs: true, order: 'bfs' } RETURN 1", "order" ],
        [ prefix + "{ waitForSync: true } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: false } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: +1 } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: -1 } RETURN 1", "waitForSync" ],
        [ prefix + "{ method: 'hash' } RETURN 1", "method" ],
        [ prefix + "{ tititi: 'piff' } RETURN 1", "tititi" ],
      ];
      
      checkQueries("TRAVERSAL", queries);
    },
    
    testShortestPath : function () {
      const prefix = "FOR p IN OUTBOUND SHORTEST_PATH '" + cn + "/test0' TO '" + cn + "/test1' " + cn + "Edge OPTIONS ";
      const queries = [
        [ prefix + "{ weightAttribute: 'testi' } RETURN 1" ],
        [ prefix + "{ defaultWeight: 42.5 } RETURN 1" ],
        [ prefix + "{ weightAttribute: false } RETURN 1", "weightAttribute" ],
        [ prefix + "{ defaultWeight: false } RETURN 1", "defaultWeight" ],
        [ prefix + "{ waitForSync: false } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: true } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: +1 } RETURN 1", "waitForSync" ],
        [ prefix + "{ waitForSync: -1 } RETURN 1", "waitForSync" ],
        [ prefix + "{ method: 'hash' } RETURN 1", "method" ],
        [ prefix + "{ tititi: 'piff' } RETURN 1", "tititi" ],
      ];
      
      checkQueries("SHORTEST_PATH", queries);
    },

    testCollect : function () {
      const prefix = "FOR doc IN " + cn + " COLLECT x = doc._key OPTIONS ";
      const queries = [
        [ prefix + "{ method: 'sorted' } RETURN x" ],
        [ prefix + "{ method: 'hash' } RETURN x" ],
        [ prefix + "{ method: 'foxx' } RETURN x", "method" ],
        [ prefix + "{ waitForSync: false } RETURN x", "waitForSync" ],
        [ prefix + "{ waitForSync: true } RETURN x", "waitForSync" ],
        [ prefix + "{ waitForSync: +1 } RETURN x", "waitForSync" ],
        [ prefix + "{ waitForSync: -1 } RETURN x", "waitForSync" ],
        [ prefix + "{ tititi: 'piff' } RETURN x", "tititi" ],
      ];

      checkQueries("COLLECT", queries);
    },
    
    testInsert : function () {
      const prefix = "FOR doc IN " + cn + " INSERT {} INTO " + cn + " OPTIONS ";
      const queries = [
        [ prefix + "{ waitForSync: false }" ],
        [ prefix + "{ waitForSync: true }" ],
        [ prefix + "{ waitForSync: +1 }" ],
        [ prefix + "{ waitForSync: -1 }" ],
        [ prefix + "{ skipDocumentValidation: true }" ],
        [ prefix + "{ keepNull: true }" ],
        [ prefix + "{ mergeObjects: true }" ],
        [ prefix + "{ overwrite: true }" ],
        [ prefix + "{ overwriteMode: 'ignore' }" ],
        [ prefix + "{ ignoreRevs: true }" ],
        [ prefix + "{ exclusive: true }" ],
        [ prefix + "{ ignoreErrors: true }" ],
        [ prefix + "{ overwriteMode: true }", "overwriteMode" ],
        [ prefix + "{ method: 'hash' }", "method" ],
        [ prefix + "{ tititi: 'piff' }", "tititi" ],
      ];

      checkQueries("INSERT", queries);
    },
    
    testUpdate : function () {
      const prefix = "FOR doc IN " + cn + " UPDATE doc WITH {} IN " + cn + " OPTIONS ";
      const queries = [
        [ prefix + "{ waitForSync: false }" ],
        [ prefix + "{ waitForSync: true }" ],
        [ prefix + "{ waitForSync: +1 }" ],
        [ prefix + "{ waitForSync: -1 }" ],
        [ prefix + "{ skipDocumentValidation: true }" ],
        [ prefix + "{ keepNull: true }" ],
        [ prefix + "{ mergeObjects: true }" ],
        [ prefix + "{ overwrite: true }" ],
        [ prefix + "{ overwriteMode: 'ignore' }" ],
        [ prefix + "{ ignoreRevs: true }" ],
        [ prefix + "{ exclusive: true }" ],
        [ prefix + "{ ignoreErrors: true }" ],
        [ prefix + "{ overwriteMode: true }", "overwriteMode" ],
        [ prefix + "{ method: 'hash' }", "method" ],
        [ prefix + "{ tititi: 'piff' }", "tititi" ],
      ];
      
      checkQueries("UPDATE", queries);
    },
    
    testReplace : function () {
      const prefix = "FOR doc IN " + cn + " REPLACE doc WITH {} IN " + cn + " OPTIONS ";
      const queries = [
        [ prefix + "{ waitForSync: false }" ],
        [ prefix + "{ waitForSync: true }" ],
        [ prefix + "{ waitForSync: +1 }" ],
        [ prefix + "{ waitForSync: -1 }" ],
        [ prefix + "{ skipDocumentValidation: true }" ],
        [ prefix + "{ keepNull: true }" ],
        [ prefix + "{ mergeObjects: true }" ],
        [ prefix + "{ overwrite: true }" ],
        [ prefix + "{ overwriteMode: 'ignore' }" ],
        [ prefix + "{ ignoreRevs: true }" ],
        [ prefix + "{ exclusive: true }" ],
        [ prefix + "{ ignoreErrors: true }" ],
        [ prefix + "{ overwriteMode: true }", "overwriteMode" ],
        [ prefix + "{ method: 'hash' }", "method" ],
        [ prefix + "{ tititi: 'piff' }", "tititi" ],
      ];
      
      checkQueries("REPLACE", queries);
    },
    
    testRemove : function () {
      const prefix = "FOR doc IN " + cn + " REMOVE doc IN " + cn + " OPTIONS ";
      const queries = [
        [ prefix + "{ waitForSync: false }" ],
        [ prefix + "{ waitForSync: true }" ],
        [ prefix + "{ waitForSync: +1 }" ],
        [ prefix + "{ waitForSync: -1 }" ],
        [ prefix + "{ skipDocumentValidation: true }" ],
        [ prefix + "{ keepNull: true }" ],
        [ prefix + "{ mergeObjects: true }" ],
        [ prefix + "{ overwrite: true }" ],
        [ prefix + "{ overwriteMode: 'ignore' }" ],
        [ prefix + "{ ignoreRevs: true }" ],
        [ prefix + "{ exclusive: true }" ],
        [ prefix + "{ ignoreErrors: true }" ],
        [ prefix + "{ overwriteMode: true }", "overwriteMode" ],
        [ prefix + "{ method: 'hash' }", "method" ],
        [ prefix + "{ tititi: 'piff' }", "tititi" ],
      ];
      
      checkQueries("REMOVE", queries);
    },
    
    testUpsert : function () {

      const prefix = "FOR doc IN " + cn + " UPSERT { testi: 1234 } INSERT { testi: 1234 } UPDATE { testi: OLD.testi + 1 } IN " + cn + " OPTIONS ";
      const queries = [
        [ prefix + "{ waitForSync: false }" ],
        [ prefix + "{ waitForSync: true }" ],
        [ prefix + "{ waitForSync: +1 }" ],
        [ prefix + "{ waitForSync: -1 }" ],
        [ prefix + "{ skipDocumentValidation: true }" ],
        [ prefix + "{ keepNull: true }" ],
        [ prefix + "{ mergeObjects: true }" ],
        [ prefix + "{ overwrite: true }" ],
        [ prefix + "{ overwriteMode: 'ignore' }" ],
        [ prefix + "{ ignoreRevs: true }" ],
        [ prefix + "{ exclusive: true }" ],
        [ prefix + "{ ignoreErrors: true }" ],
        [ prefix + "{ overwriteMode: true }", "overwriteMode" ],
        [ prefix + "{ method: 'hash' }", "method" ],
        [ prefix + "{ tititi: 'piff' }", "tititi" ],
      ];
      
      checkQueries("UPSERT", queries);
    },

    testUpsertWithIndexHint: function () {
      print(db.cn.any());
      db.cn.ensureIndex({type: 'persistent', fields: ['value', '1234'], name: 'index1'});
      db.cn.ensureIndex({type: 'persistent', fields: ['value'], name: 'index2'});
      db.cn.ensureIndex({type: 'persistent', fields: ['value', 5678], name: 'index3'});
      const prefix = "FOR doc IN " + cn + " UPSERT { testi: 1234 } INSERT { testi: 1234 } UPDATE { testi: OLD.testi + 1 } IN " + cn + " OPTIONS ";
      const queries = [
        [ prefix + "{ waitForSync: false }" ],
        [ prefix + "{ waitForSync: true }" ],
        [ prefix + "{ waitForSync: +1 }" ],
        [ prefix + "{ waitForSync: -1 }" ],
        [ prefix + "{ indexHint: 'index1' }" ],
        [ prefix + "{ indexHint: 'index1', exclusive: true }" ],
        [ prefix + "{ indexHint: 'index1', waitForSync: true}" ],
        [ prefix + "{ indexHint: 'index2' }" ],
        [ prefix + "{ indexHint: 'index2', exclusive: true }" ],
        [ prefix + "{ indexHint: 'index2', waitForSync: true}" ],
        [ prefix + "{ indexHint: 'index3' }" ],
        [ prefix + "{ indexHint: 'index3', exclusive: true }" ],
        [ prefix + "{ indexHint: 'index3', waitForSync: true}" ]
      ];

      checkQueries("UPSERT", queries);
    }
    
  };
}

jsunity.run(aqlOptionsVerificationSuite);
return jsunity.done();
