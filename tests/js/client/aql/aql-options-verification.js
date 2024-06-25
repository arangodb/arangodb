/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertMatch, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const db = require('internal').db;
const jsunity = require("jsunity");
const errors = internal.errors;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const isEnterprise = require("internal").isEnterprise();

function aqlOptionsVerificationSuite(isSearchAlias) {
  const cn = 'UnitTestsCollection';

  let checkQueries = (operation, queries) => {
    queries.forEach((query) => {
      let result = db._createStatement(query[0]).explain();
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

    setUpAll: function () {
      db._drop(cn);
      let c = db._create(cn);

      db._drop(cn + "Edge");
      db._createEdgeCollection(cn + "Edge");

      db._dropView(cn + "View");
      if (isSearchAlias) {
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields: [{"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}]};
        } else {
          indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields: [{"name": "value[*]"}]};
        }
        let i1 = c.ensureIndex(indexMeta);
        db._createView(cn + "View", "search-alias", {indexes: [{collection: c.name(), index: i1.name}]});
      } else {
        let viewMeta = {};
        if (isEnterprise) {
          viewMeta = {links: {[cn]: {includeAllFields: true, "fields": { "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        } else {
          viewMeta = {links: {[cn]: {includeAllFields: true, "fields": {}}}};
        }
        db._createView(cn + "View", "arangosearch", viewMeta);
      }
    },

    tearDownAll: function () {
      db._dropView(cn + "View");
      db._drop(cn + "Edge");
      db._drop(cn);
    },

    testForCollection: function () {
      const prefix = "FOR doc IN " + cn + " OPTIONS ";
      const queries = [
        [prefix + "{ disableIndex: true } RETURN 1"],
        [prefix + "{ disableIndex: false } RETURN 1"],
        [prefix + "{ maxProjections: 123 } RETURN 1"],
        [prefix + "{ useCache: true } RETURN 1"],
        [prefix + "{ useCache: false } RETURN 1"],
        [prefix + "{ lookahead: 0 } RETURN 1"],
        [prefix + "{ lookahead: 1 } RETURN 1"],
        [prefix + "{ indexHint: 'primary' } RETURN 1"],
        [prefix + "{ indexHint: ['primary'] } RETURN 1"],
        [prefix + "{ indexHint: ['primary'], forceIndexHint: false } RETURN 1"],
        [prefix + "{ forceIndexHint: false } RETURN 1"],
        [prefix + "{ indexHint: 'inverted'} RETURN 1"],

        [prefix + "{ forceIndexHint: 'meow' } RETURN 1", "forceIndexHint"],
        [prefix + "{ indexHint: false } RETURN 1", "indexHint"],
        [prefix + "{ indexHint: [] } RETURN 1", "indexHint"],
        [prefix + "{ maxProjections: 'piff' } RETURN 1", "maxProjections"],
        [prefix + "{ useCache: 'piff' } RETURN 1", "useCache"],
        [prefix + "{ lookahead: 'meow' } RETURN 1", "lookahead"],
        [prefix + "{ lookahead: false } RETURN 1", "lookahead"],
        [prefix + "{ waitForSync: +1 } RETURN 1", "waitForSync"],
        [prefix + "{ waitForSync: -1 } RETURN 1", "waitForSync"],
        [prefix + "{ method: 'hash' } RETURN 1", "method"],
        [prefix + "{ tititi: 'piff' } RETURN 1", "tititi"],

        // valid waitForSync option
        [prefix + "{ waitForSync: true } RETURN 1"],
        [prefix + "{ waitForSync: false } RETURN 1"],

        // valid combinations of indexHint and disableIndex
        [prefix + "{ indexHint: 'primary', disableIndex: false } RETURN 1"],
        [prefix + "{ indexHint: ['primary'], disableIndex: false } RETURN 1"],
        [prefix + "{ disableIndex: false, indexHint: 'primary' } RETURN 1"],

        // invalid combinations of indexHint and disableIndex
        [prefix + "{ indexHint: 'primary', disableIndex: true } RETURN 1", "disableIndex"],
        [prefix + "{ indexHint: ['primary'], disableIndex: true } RETURN 1", "disableIndex"],
        [prefix + "{ disableIndex: true, indexHint: 'primary' } RETURN 1", "indexHint"],
      ];

      checkQueries("FOR", queries);
    },

    testForView: function () {
      const prefix = "FOR doc IN " + cn + "View SEARCH doc.testi == 123 OPTIONS ";
      const queries = [
        [prefix + "{ collections: ['" + cn + "'] } RETURN 1"],
        [prefix + "{ waitForSync: true } RETURN 1"],
        [prefix + "{ waitForSync: false } RETURN 1"],
        [prefix + "{ noMaterialization: true } RETURN 1"],
        [prefix + "{ noMaterialization: false } RETURN 1"],
        [prefix + "{ countApproximate: 'exact' } RETURN 1"],
        [prefix + "{ countApproximate: 'cost' } RETURN 1"],
        [prefix + "{ conditionOptimization: 'auto' } RETURN 1"],
        [prefix + "{ conditionOptimization: 'nodnf' } RETURN 1"],
        [prefix + "{ conditionOptimization: 'noneg' } RETURN 1"],
        [prefix + "{ conditionOptimization: 'none' } RETURN 1"],

        [prefix + "{ method: 'hash' } RETURN 1", "method"],
        [prefix + "{ tititi: 'piff' } RETURN 1", "tititi"],
        [prefix + "{ indexHint: false } RETURN 1", "indexHint"],
        [prefix + "{ forceIndexHint: true } RETURN 1", "forceIndexHint"],
        [prefix + "{ disableIndex: true } RETURN 1", "disableIndex"],
        [prefix + "{ maxProjections: 123 } RETURN 1", "maxProjections"],
        [prefix + "{ useCache: true } RETURN 1", "useCache"],
        [prefix + "{ lookahead: 0 } RETURN 1", "lookahead"],
      ];

      // arangosearch only likes boolean attributes for its waitForSync value
      try {
        db._createStatement(prefix + "{ waitForSync: +1 } RETURN 1").explain();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
      try {
        db._createStatement(prefix + "{ waitForSync: -1 } RETURN 1").explain();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testTraversal: function () {
      const prefix = "FOR v, e, p IN 1..1 OUTBOUND '" + cn + "/test' " + cn + "Edge OPTIONS ";
      const queries = [
        [prefix + "{ bfs: false } RETURN 1"],
        [prefix + "{ bfs: true } RETURN 1"],
        [prefix + "{ order: 'bfs' } RETURN 1"],
        [prefix + "{ order: 'dfs' } RETURN 1"],
        [prefix + "{ order: 'weighted' } RETURN 1"],
        [prefix + "{ defaultWeight: 17 } RETURN 1"],
        [prefix + "{ weightAttribute: 'testi' } RETURN 1"],
        [prefix + "{ uniqueVertices: 'path' } RETURN 1"],
        [prefix + "{ uniqueVertices: 'none' } RETURN 1"],
        [prefix + "{ parallelism: 4 } RETURN 1"],
        [prefix + "{ maxProjections: 123 } RETURN 1"],

        [prefix + "{ disableIndex: true } RETURN 1", "disableIndex"],
        [prefix + "{ lookahead: 0 } RETURN 1", "lookahead"],
        [prefix + "{ defaultWeight: true } RETURN 1", "defaultWeight"],
        [prefix + "{ weightAttribute: ['testi'] } RETURN 1", "weightAttribute"],
        [prefix + "{ uniqueVertices: true } RETURN 1", "uniqueVertices"],
        [prefix + "{ uniqueEdges: true } RETURN 1", "uniqueEdges"],
        [prefix + "{ bfs: true, order: 'bfs' } RETURN 1", "order"],
        [prefix + "{ waitForSync: true } RETURN 1", "waitForSync"],
        [prefix + "{ waitForSync: false } RETURN 1", "waitForSync"],
        [prefix + "{ waitForSync: +1 } RETURN 1", "waitForSync"],
        [prefix + "{ waitForSync: -1 } RETURN 1", "waitForSync"],
        [prefix + "{ method: 'hash' } RETURN 1", "method"],
        [prefix + "{ tititi: 'piff' } RETURN 1", "tititi"],
        
        // index hint has a wrong structure
        [prefix + "{ indexHint: 'primary' } RETURN 1", "indexHint"],
        [prefix + "{ indexHint: {} } RETURN 1", "indexHint"],
        [prefix + "{ indexHint: { foo: {} } } RETURN 1", "indexHint"],
        [prefix + "{ indexHint: { foo: { milch: {} } } } RETURN 1", "indexHint"],
        [prefix + "{ indexHint: { foo: { inbound: {} } } } RETURN 1", "indexHint"],

        // forceIndexHint not (yet) support for traversal index hints
        [prefix + "{ forceIndexHint: true } RETURN 1", "forceIndexHint"],
      ];

      checkQueries("TRAVERSAL", queries);
    },

    testShortestPath: function () {
      const prefix = "FOR p IN OUTBOUND SHORTEST_PATH '" + cn + "/test0' TO '" + cn + "/test1' " + cn + "Edge OPTIONS ";
      const queries = [
        [prefix + "{ weightAttribute: 'testi' } RETURN 1"],
        [prefix + "{ defaultWeight: 42.5 } RETURN 1"],

        [prefix + "{ weightAttribute: false } RETURN 1", "weightAttribute"],
        [prefix + "{ defaultWeight: false } RETURN 1", "defaultWeight"],
        [prefix + "{ waitForSync: false } RETURN 1", "waitForSync"],
        [prefix + "{ waitForSync: true } RETURN 1", "waitForSync"],
        [prefix + "{ waitForSync: +1 } RETURN 1", "waitForSync"],
        [prefix + "{ waitForSync: -1 } RETURN 1", "waitForSync"],
        [prefix + "{ method: 'hash' } RETURN 1", "method"],
        [prefix + "{ tititi: 'piff' } RETURN 1", "tititi"],
        [prefix + "{ disableIndex: true } RETURN 1", "disableIndex"],
        [prefix + "{ maxProjections: 123 } RETURN 1", "maxProjections"],
        [prefix + "{ lookahead: 0 } RETURN 1", "lookahead"],
        
        // indexHints not (yet) support for path queries
        [prefix + "{ indexHint: 'primary' } RETURN 1", "indexHint"],
        // forceIndexHint not (yet) support for path queries
        [prefix + "{ forceIndexHint: true } RETURN 1", "forceIndexHint"],
      ];

      checkQueries("SHORTEST_PATH", queries);
    },
    
    testEnumeratePaths: function () {
      ["K_PATHS", "K_SHORTEST_PATHS"].forEach((type) => {
        const prefix = "FOR p IN OUTBOUND " + type + " '" + cn + "/test0' TO '" + cn + "/test1' " + cn + "Edge OPTIONS ";
        const queries = [
          [prefix + "{ weightAttribute: 'testi' } RETURN 1"],
          [prefix + "{ defaultWeight: 42.5 } RETURN 1"],

          [prefix + "{ weightAttribute: false } RETURN 1", "weightAttribute"],
          [prefix + "{ defaultWeight: false } RETURN 1", "defaultWeight"],
          [prefix + "{ waitForSync: false } RETURN 1", "waitForSync"],
          [prefix + "{ waitForSync: true } RETURN 1", "waitForSync"],
          [prefix + "{ waitForSync: +1 } RETURN 1", "waitForSync"],
          [prefix + "{ waitForSync: -1 } RETURN 1", "waitForSync"],
          [prefix + "{ method: 'hash' } RETURN 1", "method"],
          [prefix + "{ tititi: 'piff' } RETURN 1", "tititi"],
          [prefix + "{ disableIndex: true } RETURN 1", "disableIndex"],
          [prefix + "{ maxProjections: 123 } RETURN 1", "maxProjections"],
          [prefix + "{ lookahead: 0 } RETURN 1", "lookahead"],
        
          // indexHints not (yet) support for path queries
          [prefix + "{ indexHint: 'primary' } RETURN 1", "indexHint"],
          // forceIndexHint not (yet) support for path queries
          [prefix + "{ forceIndexHint: true } RETURN 1", "forceIndexHint"],
        ];

        checkQueries(type, queries);
      });
    },

    testCollect: function () {
      const prefix = "FOR doc IN " + cn + " COLLECT x = doc._key OPTIONS ";
      const queries = [
        [prefix + "{ method: 'sorted' } RETURN x"],
        [prefix + "{ method: 'hash' } RETURN x"],

        [prefix + "{ method: 'foxx' } RETURN x", "method"],
        [prefix + "{ waitForSync: false } RETURN x", "waitForSync"],
        [prefix + "{ waitForSync: true } RETURN x", "waitForSync"],
        [prefix + "{ waitForSync: +1 } RETURN x", "waitForSync"],
        [prefix + "{ waitForSync: -1 } RETURN x", "waitForSync"],
        [prefix + "{ tititi: 'piff' } RETURN x", "tititi"],
        [prefix + "{ indexHint: 'primary' } RETURN x", "indexHint"],
        [prefix + "{ forceIndexHint: true } RETURN x", "forceIndexHint"],
        [prefix + "{ disableIndex: true } RETURN x", "disableIndex"],
        [prefix + "{ maxProjections: 123 } RETURN x", "maxProjections"],
        [prefix + "{ useCache: true } RETURN x", "useCache"],
        [prefix + "{ lookahead: 0 } RETURN x", "lookahead"],
      ];

      checkQueries("COLLECT", queries);
    },

    testInsert: function () {
      const prefix = "FOR doc IN " + cn + " INSERT {'value_nested': [{'nested_1': [{'nested_2': 'foo'}]}]} INTO " + cn + " OPTIONS ";
      const queries = [
        [prefix + "{ waitForSync: false }"],
        [prefix + "{ waitForSync: true }"],
        [prefix + "{ waitForSync: +1 }"],
        [prefix + "{ waitForSync: -1 }"],
        [prefix + "{ skipDocumentValidation: true }"],
        [prefix + "{ keepNull: true }"],
        [prefix + "{ mergeObjects: true }"],
        [prefix + "{ overwrite: true }"],
        [prefix + "{ overwriteMode: 'ignore' }"],
        [prefix + "{ ignoreRevs: true }"],
        [prefix + "{ exclusive: true }"],
        [prefix + "{ ignoreErrors: true }"],

        [prefix + "{ overwriteMode: true }", "overwriteMode"],
        [prefix + "{ method: 'hash' }", "method"],
        [prefix + "{ tititi: 'piff' }", "tititi"],
        [prefix + "{ indexHint: 'primary' }", "indexHint"],
        [prefix + "{ forceIndexHint: true }", "forceIndexHint"],
        [prefix + "{ disableIndex: true }", "disableIndex"],
        [prefix + "{ maxProjections: 123 }", "maxProjections"],
        [prefix + "{ useCache: true }", "useCache"],
        [prefix + "{ lookahead: 0 }", "lookahead"],
      ];

      checkQueries("INSERT", queries);
    },

    testUpdate: function () {
      const prefix = "FOR doc IN " + cn + " UPDATE doc WITH {'value_nested': [{'nested_1': [{'nested_2': 'foo'}]}]} IN " + cn + " OPTIONS ";
      const queries = [
        [prefix + "{ waitForSync: false }"],
        [prefix + "{ waitForSync: true }"],
        [prefix + "{ waitForSync: +1 }"],
        [prefix + "{ waitForSync: -1 }"],
        [prefix + "{ skipDocumentValidation: true }"],
        [prefix + "{ keepNull: true }"],
        [prefix + "{ mergeObjects: true }"],
        [prefix + "{ overwrite: true }"],
        [prefix + "{ overwriteMode: 'ignore' }"],
        [prefix + "{ ignoreRevs: true }"],
        [prefix + "{ exclusive: true }"],
        [prefix + "{ ignoreErrors: true }"],

        [prefix + "{ overwriteMode: true }", "overwriteMode"],
        [prefix + "{ method: 'hash' }", "method"],
        [prefix + "{ tititi: 'piff' }", "tititi"],
        [prefix + "{ indexHint: 'primary' }", "indexHint"],
        [prefix + "{ forceIndexHint: true }", "forceIndexHint"],
        [prefix + "{ disableIndex: true }", "disableIndex"],
        [prefix + "{ maxProjections: 123 }", "maxProjections"],
        [prefix + "{ useCache: true }", "useCache"],
        [prefix + "{ lookahead: 0 }", "lookahead"],
      ];

      checkQueries("UPDATE", queries);
    },

    testReplace: function () {
      const prefix = "FOR doc IN " + cn + " REPLACE doc WITH {'value_nested': [{'nested_1': [{'nested_2': 'foo'}]}]} IN " + cn + " OPTIONS ";
      const queries = [
        [prefix + "{ waitForSync: false }"],
        [prefix + "{ waitForSync: true }"],
        [prefix + "{ waitForSync: +1 }"],
        [prefix + "{ waitForSync: -1 }"],
        [prefix + "{ skipDocumentValidation: true }"],
        [prefix + "{ keepNull: true }"],
        [prefix + "{ mergeObjects: true }"],
        [prefix + "{ overwrite: true }"],
        [prefix + "{ overwriteMode: 'ignore' }"],
        [prefix + "{ ignoreRevs: true }"],
        [prefix + "{ exclusive: true }"],
        [prefix + "{ ignoreErrors: true }"],

        [prefix + "{ overwriteMode: true }", "overwriteMode"],
        [prefix + "{ method: 'hash' }", "method"],
        [prefix + "{ tititi: 'piff' }", "tititi"],
        [prefix + "{ indexHint: 'primary' }", "indexHint"],
        [prefix + "{ forceIndexHint: true }", "forceIndexHint"],
        [prefix + "{ disableIndex: true }", "disableIndex"],
        [prefix + "{ maxProjections: 123 }", "maxProjections"],
        [prefix + "{ useCache: true }", "useCache"],
        [prefix + "{ lookahead: 0 }", "lookahead"],
      ];

      checkQueries("REPLACE", queries);
    },

    testRemove: function () {
      const prefix = "FOR doc IN " + cn + " REMOVE doc IN " + cn + " OPTIONS ";
      const queries = [
        [prefix + "{ waitForSync: false }"],
        [prefix + "{ waitForSync: true }"],
        [prefix + "{ waitForSync: +1 }"],
        [prefix + "{ waitForSync: -1 }"],
        [prefix + "{ skipDocumentValidation: true }"],
        [prefix + "{ keepNull: true }"],
        [prefix + "{ mergeObjects: true }"],
        [prefix + "{ overwrite: true }"],
        [prefix + "{ overwriteMode: 'ignore' }"],
        [prefix + "{ ignoreRevs: true }"],
        [prefix + "{ exclusive: true }"],
        [prefix + "{ ignoreErrors: true }"],

        [prefix + "{ overwriteMode: true }", "overwriteMode"],
        [prefix + "{ method: 'hash' }", "method"],
        [prefix + "{ tititi: 'piff' }", "tititi"],
        [prefix + "{ indexHint: 'primary' }", "indexHint"],
        [prefix + "{ forceIndexHint: true }", "forceIndexHint"],
        [prefix + "{ disableIndex: true }", "disableIndex"],
        [prefix + "{ maxProjections: 123 }", "maxProjections"],
        [prefix + "{ useCache: true }", "useCache"],
        [prefix + "{ lookahead: 0 }", "lookahead"],
      ];

      checkQueries("REMOVE", queries);
    },

    testUpsert: function () {
      const prefix = "FOR doc IN " + cn + " UPSERT { testi: 1234 } INSERT { testi: 1234 } UPDATE { testi: OLD.testi + 1 } IN " + cn + " OPTIONS ";
      // note: we have to set readOwnWrites: false here to avoid
      // a warning about the UPSERT not being able to observe its
      // own writes in sharded collections or CE cluster runs.
      const queries = [
        [prefix + "{ waitForSync: false, readOwnWrites: false }"],
        [prefix + "{ waitForSync: true, readOwnWrites: false }"],
        [prefix + "{ waitForSync: +1, readOwnWrites: false }"],
        [prefix + "{ waitForSync: -1, readOwnWrites: false }"],
        [prefix + "{ indexHint: 'primary', readOwnWrites: false }"],
        [prefix + "{ forceIndexHint: true, readOwnWrites: false }"],
        [prefix + "{ disableIndex: true, readOwnWrites: false }"],
        [prefix + "{ skipDocumentValidation: true, readOwnWrites: false }"],
        [prefix + "{ keepNull: true, readOwnWrites: false }"],
        [prefix + "{ mergeObjects: true, readOwnWrites: false }"],
        [prefix + "{ overwrite: true, readOwnWrites: false }"],
        [prefix + "{ overwriteMode: 'ignore', readOwnWrites: false }"],
        [prefix + "{ ignoreRevs: true, readOwnWrites: false }"],
        [prefix + "{ exclusive: true, readOwnWrites: false }"],
        [prefix + "{ ignoreErrors: true, readOwnWrites: false }"],
        [prefix + "{ useCache: true, readOwnWrites: false }"],
        [prefix + "{ useCache: false, readOwnWrites: false }"],

        [prefix + "{ overwriteMode: true, readOwnWrites: false }", "overwriteMode"],
        [prefix + "{ method: 'hash', readOwnWrites: false }", "method"],
        [prefix + "{ tititi: 'piff', readOwnWrites: false }", "tititi"],
        [prefix + "{ maxProjections: 123, readOwnWrites: false }", "maxProjections"],
        [prefix + "{ lookahead: 0, readOwnWrites: false }", "lookahead"],
      ];

      checkQueries("UPSERT", queries);
    },

    testUpsertWithIndexHint: function () {
      try {
        db[cn].ensureIndex({type: 'persistent', fields: ['value', '1234'], name: 'index1'});
        db[cn].ensureIndex({type: 'persistent', fields: ['value'], name: 'index2'});
        db[cn].ensureIndex({type: 'persistent', fields: ['value', '5678'], name: 'index3'});
        const prefix = "FOR doc IN " + cn + " UPSERT { testi: 1234 } INSERT { testi: 1234 } UPDATE { testi: OLD.testi + 1 } IN " + cn + " OPTIONS ";
        // note: we have to set readOwnWrites: false here to avoid
        // a warning about the UPSERT not being able to observe its
        // own writes in sharded collections or CE cluster runs.
        const queries = [
          [prefix + "{ waitForSync: false, readOwnWrites: false }"],
          [prefix + "{ waitForSync: true, readOwnWrites: false }"],
          [prefix + "{ waitForSync: +1, readOwnWrites: false }"],
          [prefix + "{ waitForSync: -1, readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index1', readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index1', exclusive: true , readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index1', waitForSync: true, readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index2', readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index2', exclusive: true , readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index2', waitForSync: true, readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index3', readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index3', exclusive: true , readOwnWrites: false }"],
          [prefix + "{ indexHint: 'index3', waitForSync: true, readOwnWrites: false }"],
        ];

        checkQueries("UPSERT", queries);
      } finally {
        db[cn].dropIndex("index1");
        db[cn].dropIndex("index2");
        db[cn].dropIndex("index3");
      }
    }

  };
}

function aqlOptionsVerificationArangoSearchSuite() {
  let suite = {};
  deriveTestSuite(
    aqlOptionsVerificationSuite(false),
    suite,
    "_arangosearch"
  );
  return suite;
}

function aqlOptionsVerificationSearchAliasSuite() {
  let suite = {};
  deriveTestSuite(
    aqlOptionsVerificationSuite(true),
    suite,
    "_search-alias"
  );
  return suite;
}

jsunity.run(aqlOptionsVerificationArangoSearchSuite);
jsunity.run(aqlOptionsVerificationSearchAliasSuite);

return jsunity.done();
