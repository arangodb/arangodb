/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ COUNT
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = require("@arangodb").db;
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const isCluster = require("@arangodb/cluster").isCluster();
const isEnterprise = require("internal").isEnterprise();
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

function optimizerCollectInClusterSuite(isSearchAlias) {
  let c;
  let v;

  return {
    setUpAll: function () {
      db._dropView("UnitTestViewSorted");
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", {numberOfShards: 3});
      if (isSearchAlias) {
        let c = db._collection("UnitTestsCollection");
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {
            type: "inverted",
            name: "inverted",
            includeAllFields: true,
            primarySort: {fields: [{"field": "value", "direction": "asc"}]},
            fields: [
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "group"
            ]
          };
        } else {
          indexMeta = {
            type: "inverted",
            name: "inverted",
            includeAllFields: true,
            primarySort: {fields: [{"field": "value", "direction": "asc"}]},
            fields: [
              {"name": "value_nested[*]"},
              "value",
              "group"
            ]
          };
        }
        let i = c.ensureIndex(indexMeta);
        v = db._createView("UnitTestViewSorted", "search-alias", {
          indexes: [{collection: "UnitTestsCollection", index: i.name}]
        });
      } else {
        let viewMeta = {};
        if (isEnterprise) {
          viewMeta = {
            primarySort: [{"field": "value", "direction": "asc"}],
            links: {
              UnitTestsCollection: {
                includeAllFields: false,
                fields: {
                  value: {analyzers: ["identity"]},
                  group: {analyzers: ["identity"]},
                  "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}
                }
              }
            }
          };
        } else {
          viewMeta = {
            primarySort: [{"field": "value", "direction": "asc"}],
            links: {
              UnitTestsCollection: {
                includeAllFields: false,
                fields: {
                  value: {analyzers: ["identity"]},
                  group: {analyzers: ["identity"]},
                 "value_nested": {}
                }
              }
            }
          };
        }
        v = db._createView("UnitTestViewSorted", "arangosearch", viewMeta);
      }
  
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({group: "test" + (i % 10), value: i, "value_nested": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]});
      }
      c.insert(docs);
  
      // sync the view
      db._query("FOR d IN UnitTestViewSorted OPTIONS {waitForSync:true} LIMIT 1 RETURN d");

      if (isSearchAlias) {
        // sync the index
        db._query(`FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.value > 0 LIMIT 1 RETURN doc`);
      }
    },

    tearDownAll: function () {
      db._dropView("UnitTestViewSorted");
      db._drop("UnitTestsCollection");
    },

    testCount: function () {
      let query = "FOR doc IN " + c.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);

      if (isSearchAlias) {
        let indexQuery = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.value >= 0 COLLECT WITH COUNT INTO length RETURN length`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(1, results.json.length);
        assertEqual(1000, results.json[0]);
      }
    },

    testCountView: function () {
      let query = "FOR doc IN " + v.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateViewNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);

      if (isSearchAlias) {
        let indexQuery = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER STARTS_WITH(doc.group, "test") COLLECT WITH COUNT INTO length RETURN length`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(1, results.json.length);
        assertEqual(1000, results.json[0]);
      }
    },

    testCountMulti: function () {
      let query = "FOR doc1 IN " + c.name() + " FILTER doc1.value < 10 FOR doc2 IN " + c.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(10000, results.json[0]);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc1 IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc1.value < 10 
          FOR doc2 in UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER doc2.value >= 0
          COLLECT WITH COUNT INTO length RETURN length`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(1, results.json.length);
        assertEqual(10000, results.json[0]);
      }
    },

    testCountMultiView: function () {
      let query = "FOR doc1 IN " + v.name() + " SEARCH doc1.value < 10 FOR doc2 IN " + v.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(10000, results.json[0]);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateViewNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateViewNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc1 IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER STARTS_WITH(doc1.group, "test")
          FOR doc2 in UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER doc2.value >= 0
          COLLECT WITH COUNT INTO length RETURN length`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(1, results.json.length);
        assertEqual(1000000, results.json[0]);
      }
    },
    
    testCountGroups: function () {
      let query = "FOR doc IN " + c.name() + " COLLECT value = doc.group WITH COUNT INTO length SORT value RETURN { value, length }";
      let results = AQL_EXECUTE(query);
      
      let expected = [];
      for (let i = 0; i < 10; ++i) {
        expected.push({ value: "test" + i, length: 100 });
      }
      assertEqual(expected.length, results.json.length);
      assertEqual(expected, results.json);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "SortNode", "CalculationNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER STARTS_WITH(doc.group, "test")
          COLLECT value = doc.group WITH COUNT INTO length SORT value RETURN { value, length }`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(expected.length, results.json.length);
        assertEqual(expected, results.json);
      }
    },

    testDistinct: function () {
      let query = "FOR doc IN " + c.name() + " SORT doc.value RETURN DISTINCT doc.value";

      let results = AQL_EXECUTE(query);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
      
      if (isSearchAlias) {
        /*
          PLEASE UNCOMMENT LINES BELOW AFTER FIXING https://arangodb.atlassian.net/browse/SEARCH-446
        */  
        // let indexQuery = `FOR doc IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
        //   FILTER doc.value >= 0
        //   SORT doc.value RETURN DISTINCT doc.value`;
        // let indexResults = AQL_EXECUTE(indexQuery);
        // assertEqual(1000, indexResults.json.length);
        // for (let i = 0; i < 1000; ++i) {
        //   assertEqual(i, indexResults.json[i]);
        // }

        // if (isEnterprise) {
        //   let indexQuery = `FOR doc IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
        //     FILTER doc.value_nested[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]]
        //     SORT doc.value RETURN DISTINCT doc.value`;
        //   let indexResults = AQL_EXECUTE(indexQuery);
        //   assertEqual(1000, indexResults.json.length);
        //   for (let i = 0; i < 1000; ++i) {
        //     assertEqual(i, indexResults.json[i]);
        //   }
        // }
      } 
    },

    testDistinctView: function () {
      let query = "FOR doc IN " + v.name() + " SORT doc.value RETURN DISTINCT doc.value";

      let results = AQL_EXECUTE(query);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateViewNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);

      /*
        PLEASE UNCOMMENT LINES BELOW AFTER FIXING https://arangodb.atlassian.net/browse/SEARCH-446
      */  
      // if (isSearchAlias) {
      //   let indexQuery = `FOR doc IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
      //     FILTER doc.value >= 0 SORT doc.value RETURN DISTINCT doc.value`;
      //   let indexResults = AQL_EXECUTE(indexQuery);
      //   assertEqual(1000, indexResults.json.length);
      //   for (let i = 0; i < 1000; ++i) {
      //     assertEqual(i, indexResults.json[i]);
      //   }
      // }
    },

    testDistinctMulti: function () {
      let query = "FOR doc1 IN " + c.name() + " FILTER doc1.value < 10 FOR doc2 IN " + c.name() + " SORT doc2.value RETURN DISTINCT doc2.value";

      let results = AQL_EXECUTE(query, null, {optimizer: {rules: ["-interchange-adjacent-enumerations"]}});
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
      
      if (isSearchAlias) {
        let indexQuery = `FOR doc1 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc1.value < 10 
        FOR doc2 in ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
        FILTER doc2.value >= 0
        SORT doc2.value RETURN DISTINCT doc2.value`;
        let indexResults = AQL_EXECUTE(indexQuery);
        // print(JSON.stringify(indexResults));
        assertEqual(1000, indexResults.json.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, indexResults.json[i]);
        }
      }
    },

    testDistinctMultiView: function () {
      let query = "FOR doc1 IN " + v.name() + " SEARCH doc1.value < 10 FOR doc2 IN " + v.name() + " SORT doc2.value RETURN DISTINCT doc2.value";

      let results = AQL_EXECUTE(query, null, {optimizer: {rules: ["-interchange-adjacent-enumerations"]}});
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateViewNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateViewNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc1 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc1.value < 10 
          FOR doc2 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER doc2.value >= 0
          SORT doc2.value RETURN DISTINCT doc2.value`;

        let indexResults = AQL_EXECUTE(indexQuery);
        assertEqual(1000, indexResults.json.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, indexResults.json[i]);
        }

        if (isEnterprise) {
          let indexQueryEE = `FOR doc1 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc1.value < 10 
            FOR doc2 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
            FILTER doc2.value_nested[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]]
            SORT doc2.value RETURN DISTINCT doc2.value`;

          indexResults = AQL_EXECUTE(indexQueryEE);
          assertEqual(1000, indexResults.json.length);
          for (let i = 0; i < 1000; ++i) {
            assertEqual(i, indexResults.json[i]);
          }  
        }
      }
    }

  };
}

function optimizerCollectInClusterSingleShardSuite(isSearchAlias) {
  let c;
  let v;
  let opt = {optimizer: {rules: ["-cluster-one-shard"]}};
  let opt2 = {optimizer: {rules: ["-cluster-one-shard", "-interchange-adjacent-enumerations"]}};

  return {
    setUpAll: function () {
      db._dropView("UnitTestViewSorted");
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", {numberOfShards: 1});
      if (isSearchAlias) {
        let c = db._collection("UnitTestsCollection");
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {
            type: "inverted",
            name: "inverted",
            includeAllFields: true,
            fields: [
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "group"],
            primarySort: {fields: [{"field": "value", "direction": "asc"}]}
          };
        } else {
          indexMeta = {
            type: "inverted",
            name: "inverted",
            includeAllFields: true,
            primarySort: {fields: [
              {"field": "value", "direction": "asc"}]},
            fields: [
              {"name": "value_nested[*]"},
              "value",
              "group"
            ],
          };
        }
        let i = c.ensureIndex(indexMeta);
        v = db._createView("UnitTestViewSorted", "search-alias", {
          indexes: [{collection: "UnitTestsCollection", index: i.name}]
        });
      } else {
        let viewMeta = {};
        if (isEnterprise) {
          viewMeta = {
            primarySort: [{"field": "value", "direction": "asc"}],
            links: {
              UnitTestsCollection: {
                includeAllFields: false,
                fields: {
                  value: {analyzers: ["identity"]},
                  group: {analyzers: ["identity"]},
                  "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}
                }
              }
            }
          };
        } else {
          viewMeta = {
            primarySort: [{"field": "value", "direction": "asc"}],
            links: {
              UnitTestsCollection: {
                includeAllFields: false,
                fields: {
                  value: {analyzers: ["identity"]},
                  group: {analyzers: ["identity"]},
                  "value_nested": {}
                }
              }
            }
          };
        }
        v = db._createView("UnitTestViewSorted", "arangosearch", viewMeta);
      }

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({group: "test" + (i % 10), value: i, "value_nested": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]});
      }
      c.insert(docs);

      // sync the view
      db._query("FOR d IN UnitTestViewSorted OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
    
      if (isSearchAlias) {
        // sync the index
        db._query(`FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.value > 0 LIMIT 1 RETURN doc`);
      }
    },

    tearDownAll: function () {
      db._dropView("UnitTestViewSorted");
      db._drop("UnitTestsCollection");
    },

    testSingleCount: function () {
      let query = "FOR doc IN " + c.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query, null, opt);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);

      let plan = AQL_EXPLAIN(query, null, opt).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.value >= 0 COLLECT WITH COUNT INTO length RETURN length`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(1, results.json.length);
        assertEqual(1000, results.json[0]);
      }
    },

    testSingleCountView: function () {
      let query = "FOR doc IN " + v.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query, null, opt);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);

      let plan = AQL_EXPLAIN(query, null, opt).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateViewNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER STARTS_WITH(doc.group, "test") COLLECT WITH COUNT INTO length RETURN length`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(1, results.json.length);
        assertEqual(1000, results.json[0]);
      }
    },

    testSingleCountMulti: function () {
      let query = "FOR doc1 IN " + c.name() + " FILTER doc1.value < 10 FOR doc2 IN " + c.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query, null, opt);
      assertEqual(1, results.json.length);
      assertEqual(10000, results.json[0]);

      let plan = AQL_EXPLAIN(query, null, opt).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc1 IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc1.value < 10 
          FOR doc2 in UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER doc2.value >= 0
          COLLECT WITH COUNT INTO length RETURN length`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(1, results.json.length);
        assertEqual(10000, results.json[0]);
      }
    },

    testSingleCountMultiView: function () {
      let query = "FOR doc1 IN " + v.name() + " SEARCH doc1.value < 10 FOR doc2 IN " + v.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query, null, opt);
      assertEqual(1, results.json.length);
      assertEqual(10000, results.json[0]);

      let plan = AQL_EXPLAIN(query, null, opt).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateViewNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateViewNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc1 IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER STARTS_WITH(doc1.group, "test")
          FOR doc2 in UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER doc2.value >= 0
          COLLECT WITH COUNT INTO length RETURN length`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(1, results.json.length);
        assertEqual(1000000, results.json[0]);
      }
    },
    
    testSingleCountGroups: function () {
      let query = "FOR doc IN " + c.name() + " COLLECT value = doc.group WITH COUNT INTO length SORT value RETURN { value, length }";
      let results = AQL_EXECUTE(query);
      
      let expected = [];
      for (let i = 0; i < 10; ++i) {
        expected.push({ value: "test" + i, length: 100 });
      }
      assertEqual(expected.length, results.json.length);
      assertEqual(expected, results.json);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));

      if (internal.isEnterprise()) {
        assertNotEqual(-1, plan.rules.indexOf("cluster-one-shard"));
        // one shard rule has kicked in as well and modified the plan accordingly
        assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "CollectNode", "SortNode", "CalculationNode", "RemoteNode", "GatherNode", "ReturnNode"], nodeTypes);
      } else {
        assertEqual(-1, plan.rules.indexOf("cluster-one-shard"));
        assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "RemoteNode", "GatherNode", "CollectNode", "SortNode", "CalculationNode", "ReturnNode"], nodeTypes);
      }

      if (isSearchAlias) {
        let indexQuery = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER STARTS_WITH(doc.group, "test")
          COLLECT value = doc.group WITH COUNT INTO length SORT value RETURN { value, length }`;
        results = AQL_EXECUTE(indexQuery);
        assertEqual(expected.length, results.json.length);
        assertEqual(expected, results.json);
      }
    },

    testSingleDistinct: function () {
      let query = "FOR doc IN " + c.name() + " SORT doc.value RETURN DISTINCT doc.value";

      let results = AQL_EXECUTE(query, null, opt);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query, null, opt).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER doc.value_nested[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]]
          SORT doc.value RETURN DISTINCT doc.value`;
        let indexResults = AQL_EXECUTE(indexQuery);
        // print(JSON.stringify(indexResults));
        assertEqual(1000, indexResults.json.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, indexResults.json[i]);
        }
      }
    },

    testSingleDistinctView: function () {
      let query = "FOR doc IN " + v.name() + " SORT doc.value RETURN DISTINCT doc.value";

      let results = AQL_EXECUTE(query, null, opt);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query, null, opt).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateViewNode", "SortNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER doc.value >= 0 SORT doc.value RETURN DISTINCT doc.value`;
        let indexResults = AQL_EXECUTE(indexQuery);
        assertEqual(1000, indexResults.json.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, indexResults.json[i]);
        }
      }
    },

    testSingleDistinctMulti: function () {
      let query = "FOR doc1 IN " + c.name() + " FILTER doc1.value < 10 FOR doc2 IN " + c.name() + " SORT doc2.value RETURN DISTINCT doc2.value";

      let results = AQL_EXECUTE(query, null, opt2);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query, null, opt2).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc1 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc1.value < 10 
        FOR doc2 in ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
        FILTER doc2.value >= 0
        SORT doc2.value RETURN DISTINCT doc2.value`;
        let indexResults = AQL_EXECUTE(indexQuery);
        // print(JSON.stringify(indexResults));
        assertEqual(1000, indexResults.json.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, indexResults.json[i]);
        }
      }
    },

    testSingleDistinctMultiView: function () {
      let query = "FOR doc1 IN " + v.name() + " SEARCH doc1.value < 10 FOR doc2 IN " + v.name() + " SORT doc2.value RETURN DISTINCT doc2.value";

      let results = AQL_EXECUTE(query, null, opt2);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query, null, opt2).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateViewNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateViewNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    
      if (isSearchAlias) {
        let indexQuery = `FOR doc1 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc1.value < 10 
          FOR doc2 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
          FILTER doc2.value >= 0
          SORT doc2.value RETURN DISTINCT doc2.value`;
        let indexResults = AQL_EXECUTE(indexQuery);
        assertEqual(1000, indexResults.json.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, indexResults.json[i]);
        }

        if (isEnterprise) {
          let indexQuery = `FOR doc1 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc1.value < 10 
            FOR doc2 IN ${c.name()} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
            FILTER doc2.value_nested[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]]
            SORT doc2.value RETURN DISTINCT doc2.value`;
          let indexResults = AQL_EXECUTE(indexQuery);
          assertEqual(1000, indexResults.json.length);
          for (let i = 0; i < 1000; ++i) {
            assertEqual(i, indexResults.json[i]);
          }  
        }
      }
    },

  };
}

function optimizerCollectInClusterSuiteSmartGraph() {
  const vn = "UnitTestsVertex";
  const en = "UnitTestsEdge";
  const gn = "UnitTestsSmartGraph";
  const smrt = require("@arangodb/smart-graph");

  return {
    setUpAll: function () {
      try {
        smrt._drop(gn, true);
      } catch (err) {}

      smrt._create(gn, [smrt._relation(en, vn, vn)], [], { smartGraphAttribute: "group", numberOfShards: 3, isSmart: true });

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({group: "test" + (i % 10), value: i});
      }
      let keys = db[vn].insert(docs);
      assertEqual(1000, db[vn].count());
      
      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({_from: keys[i]._id, _to: keys[keys.length - i - 1]._id, value: i, group: "test" + (i % 10)});
      }
      db[en].insert(docs);

      assertEqual(1000, db[en].count());
    },

    tearDownAll: function () {
      smrt._drop(gn, true);
    },

    testCount: function () {
      let query = "FOR doc IN " + en + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },

    testCountMulti: function () {
      let query = "FOR doc1 IN " + en + " FILTER doc1.value < 10 FOR doc2 IN " + en + " COLLECT WITH COUNT INTO length RETURN length";
      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(10000, results.json[0]);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },
    
    testCountGroups: function () {
      let query = "FOR doc IN " + en + " COLLECT value = doc.group WITH COUNT INTO length SORT value RETURN { value, length }";
      let results = AQL_EXECUTE(query);
      
      let expected = [];
      for (let i = 0; i < 10; ++i) {
        expected.push({ value: "test" + i, length: 100 });
      }
      assertEqual(expected.length, results.json.length);
      assertEqual(expected, results.json);

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "SortNode", "CalculationNode", "ReturnNode"], nodeTypes);
    },

    testDistinct: function () {
      let query = "FOR doc IN " + en + " SORT doc.value RETURN DISTINCT doc.value";

      let results = AQL_EXECUTE(query);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },

    testDistinctMulti: function () {
      let query = "FOR doc1 IN " + en + " FILTER doc1.value < 10 FOR doc2 IN " + en + " SORT doc2.value RETURN DISTINCT doc2.value";

      let results = AQL_EXECUTE(query, null, {optimizer: {rules: ["-interchange-adjacent-enumerations"]}});
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function (node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },

  };
}

function optimizerCollectInClusterArangoSearchSuite() {
  let suite = {};
  deriveTestSuite(
    optimizerCollectInClusterSuite(false),
    suite,
    "_arangosearch"
  );
  return suite;
}

function optimizerCollectInClusterSearchAliasSuite() {
  let suite = {};
  deriveTestSuite(
    optimizerCollectInClusterSuite(true),
    suite,
    "_search-alias"
  );
  return suite;
}

function optimizerCollectInClusterSingleShardArangoSearchSuite() {
  let suite = {};
  deriveTestSuite(
    optimizerCollectInClusterSingleShardSuite(false),
    suite,
    "_arangosearch"
  );
  return suite;
}

function optimizerCollectInClusterSingleShardSearchAliasSuite() {
  let suite = {};
  deriveTestSuite(
    optimizerCollectInClusterSingleShardSuite(true),
    suite,
    "_search-alias"
  );
  return suite;
}

jsunity.run(optimizerCollectInClusterArangoSearchSuite);
jsunity.run(optimizerCollectInClusterSearchAliasSuite);
jsunity.run(optimizerCollectInClusterSingleShardArangoSearchSuite);
jsunity.run(optimizerCollectInClusterSingleShardSearchAliasSuite);
if (internal.isEnterprise()) {
  jsunity.run(optimizerCollectInClusterSuiteSmartGraph);
}

return jsunity.done();
