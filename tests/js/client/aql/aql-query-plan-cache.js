/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, assertUndefined, assertNotUndefined, assertMatch, assertNotMatch, fail  */

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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const graphs = require("@arangodb/general-graph");
const planCache = require("@arangodb/aql/plan-cache");
const isCluster = internal.isCluster();
      
const cn1 = "UnitTestsQueryPlanCache1";
const cn2 = "UnitTestsQueryPlanCache2";

const assertNotCached = (query, bindVars = {}, options = {}) => {
  options.optimizePlanForCaching = true;
  options.usePlanCache = true;
  // execute query once
  let res = db._query(query, bindVars, options);
  assertFalse(res.hasOwnProperty("planCacheKey"));
  
  // execute query again
  res = db._query(query, bindVars, options);
  // should still not have the planCacheKey attribute
  assertFalse(res.hasOwnProperty("planCacheKey"));
};

const assertErrorsOut = (query, bindVars = {}, options = {}) => {
  options.optimizePlanForCaching = true;
  options.usePlanCache = true;
  // execute query once
  try {
    let res = db._query(query, bindVars, options);
    fail();
  } catch (e) {
    assertEqual(1584, e.errorNum);
  }
};

const assertCached = (query, bindVars = {}, options = {}) => {
  options.optimizePlanForCaching = true;
  options.usePlanCache = true;
  // execute query once
  let res = db._query(query, bindVars, options);
  assertFalse(res.hasOwnProperty("planCacheKey"));
  
  // execute query again
  res = db._query(query, bindVars, options);
  // should now have the planCacheKey attribute
  assertTrue(res.hasOwnProperty("planCacheKey"));
};

let id = 1;
const uniqid = () => {
  return `/* query-${id++} */ `;
};

function QueryPlanCacheTestSuite () {
  let c1, c2;

  return {
    setUp : function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      planCache.clear();
    },

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = null;
      c2 = null;
    },
    
    testExplicitlyDisabled : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value == 25 RETURN doc`;
      const options = { optimizePlanForCaching: false };
  
      // execute query once
      let res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
  
      // execute query again
      res = db._query(query, null, options);
      // should still not have the planCacheKey attribute
      assertFalse(res.hasOwnProperty("planCacheKey"));
    },
    
    testWarningsDuringParsing : function () {
      const query = `${uniqid()} RETURN 1 / 0`;

      assertNotCached(query);
    },

    testWarningsDuringExecution : function () {
      const query = `${uniqid()} RETURN 1 / NOOPT(0)`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };
  
      // execute query once
      let res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(1, res.getExtra().warnings.length, res.getExtra());
  
      // execute query again
      res = db._query(query, null, options);
      // query plan can be cached, because warnings during query
      // execution do not matter for plan caching
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertEqual(1, res.getExtra().warnings.length, res.getExtra());
    },
    
    testAttributeNameBindParameter : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} FILTER doc.@attr == 25 RETURN doc`;

      // attribute name bind parameters are not supported
      assertErrorsOut(query, { attr: "foo" });
    },
    
    testUpsert : function () {
      const query = `${uniqid()} UPSERT {foo: "bar"} INSERT {} UPDATE {} IN ${cn1}`;

      // value bind parameters are not supported when defining the lookup values for UPSERT
      assertErrorsOut(query, {});
    },
    
    testCollectionBindParameter : function () {
      const query = `${uniqid()} FOR doc IN @@collection FILTER doc.value == 25 RETURN doc`;

      assertCached(query, { "@collection" : cn1 });
    },
    
    testDifferentCollectionsBindParameter : function () {
      const query = `${uniqid()} FOR doc IN @@collection FILTER doc.value <= 6 RETURN doc`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      db._query(`FOR i IN 1..10 INSERT {value: i} INTO ${cn1}`);
      db._query(`FOR i IN 1..10 INSERT {value: i * 2} INTO ${cn2}`);

      // execute for collection 1
      let res = db._query(query, { "@collection": cn1 }, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(6, res.toArray().length);
      
      res = db._query(query, { "@collection": cn1 }, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      let key = res.planCacheKey;
      assertEqual(6, res.toArray().length);

      // execute for collection 2 - this must produce a different plan
      res = db._query(query, { "@collection": cn2 }, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(3, res.toArray().length);
      
      res = db._query(query, { "@collection": cn2 }, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      // plan cache keys must differ
      assertNotEqual(key, res.planCacheKey);
      assertEqual(3, res.toArray().length);
    },
    
    testValueBindParameter : function () {
      const query = `${uniqid()} FOR doc IN 0..99 FILTER doc == @value RETURN doc`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      let key;
      for (let i = 0; i < 100; ++i) {
        let res = db._query(query, { value: i }, options);
        if (i === 0) {
          assertFalse(res.hasOwnProperty("planCacheKey"));
        } else {
          assertTrue(res.hasOwnProperty("planCacheKey"));
          if (i === 1) {
            key = res.planCacheKey;
          } else {
            // we must always see the same plan cache key
            assertEqual(res.planCacheKey, key);
          }
        }
        assertEqual([ i ], res.toArray());
      }
    },
    
    testExplain : function () {
      const query = `${uniqid()} FOR doc IN 0..99 RETURN doc`;
      const bindVars = {};
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      let stmt = db._createStatement({ query, bindVars, options });
      let res = stmt.explain();
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertNotUndefined(res.plan, res);
      
      stmt = db._createStatement({ query, bindVars, options });
      res = stmt.explain();
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertNotUndefined(res.plan, res);
    },
    
    testExplainAllPlans : function () {
      const query = `${uniqid()} FOR doc IN 0..99 RETURN doc`;
      const bindVars = {};
      const options = { optimizePlanForCaching: true, usePlanCache: true, allPlans: true };

      // allPlans is not supported with plan caching
      let stmt = db._createStatement({ query, bindVars, options });
      try {
        let res = stmt.explain();
      } catch (e) {
        assertEqual(1584, e.errorNum);
      }
    },
    
    testProfiling : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value == 1 SORT doc.value LIMIT 2 RETURN doc`;
      const options = { optimizePlanForCaching: true, usePlanCache: true, profile: 2 };

      let res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      let extra = res.getExtra();
      assertTrue(extra.hasOwnProperty("stats"), extra);
      assertTrue(extra.hasOwnProperty("profile"), extra);
      assertTrue(extra.hasOwnProperty("plan"), extra);
      let plan = extra.plan;
      
      res = db._query(query, null, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      extra = res.getExtra();
      assertTrue(extra.hasOwnProperty("stats"), extra);
      assertTrue(extra.hasOwnProperty("profile"), extra);
      assertTrue(extra.hasOwnProperty("plan"), extra);

      // compare selected attributes of the execution plans.
      // we cannot compare the literal plans because the cached execution
      // plan contains more internal details (such as variable usage information and registers)
      ["collections", "variables", "estimatedCost", "estimatedNrItems", "rules"].forEach((a) => {
        assertEqual(plan[a], extra.plan[a], { a, expected: plan[a], actual: extra.plan[a] });
      });

      // compare plan node types
      plan.nodes.forEach((n, i) => {
        assertEqual(n.type, extra.plan.nodes[i].type);
      });
    },
    
    testProfilingOutput : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value == 1 SORT doc.value LIMIT 2 RETURN doc`;
      const options = { optimizePlanForCaching: true, usePlanCache: true, profile: 2 };

      let output = require('@arangodb/aql/explainer').profileQuery({ query, options }, false);
      assertNotMatch(/plan cache key:/i, output);
      
      output = require('@arangodb/aql/explainer').profileQuery({ query, options }, false);
      assertMatch(/plan cache key:/i, output);
    },
    
    testOptimizerRulesSet : function () {
      const query = `${uniqid()} RETURN 1`;

      assertCached(query);
      assertErrorsOut(query, null, { optimizer: { rules: ["-all"] } });
      assertErrorsOut(query, null, { optimizer: { rules: ["+async-prefetch"] } });
      assertErrorsOut(query, null, { optimizer: { rules: ["-async-prefetch"] } });
    },
    
    testFullCount : function () {
      const query = `${uniqid()} FOR doc IN 0..99 LIMIT 10 RETURN doc`;
      const options1 = { optimizePlanForCaching: true, usePlanCache: true };
      const options2 = { optimizePlanForCaching: true, usePlanCache: true, fullCount: true };

      // execute without fullCount
      let res = db._query(query, null, options1);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      assertFalse(res.getExtra().stats.hasOwnProperty("fullCount"));
      
      res = db._query(query, null, options1);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      let key = res.planCacheKey;
      assertEqual(10, res.toArray().length);
      assertFalse(res.getExtra().stats.hasOwnProperty("fullCount"));

      // execute with fullCount - this must produce a different plan
      res = db._query(query, null, options2);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(100, res.getExtra().stats.fullCount);
      assertEqual(10, res.toArray().length);
      
      res = db._query(query, null, options2);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      // plan cache keys must differ
      assertNotEqual(key, res.planCacheKey);
      assertEqual(100, res.getExtra().stats.fullCount);
      assertEqual(10, res.toArray().length);
    },

    testInvalidationAfterCollectionRecreation : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} RETURN doc`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      db._query(`FOR i IN 1..10 INSERT {} INTO ${cn1}`);

      let res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      
      res = db._query(query, null, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      let key = res.planCacheKey;
      assertEqual(10, res.toArray().length);

      // drop and recreate the collection
      db._drop(cn1);
      db._create(cn1);
      
      res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(0, res.toArray().length);
      
      res = db._query(query, null, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertEqual(0, res.toArray().length);
      // plan cache keys are identical, but the plan is
      // refererring to a different instance of the collection
      // now.
      assertEqual(key, res.planCacheKey);
    },
    
    testNoInvalidationAfterUnrelatedCollectionDropping : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} RETURN doc`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      db._query(`FOR i IN 1..10 INSERT {} INTO ${cn1}`);

      let res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      
      res = db._query(query, null, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      let key = res.planCacheKey;
      assertEqual(10, res.toArray().length);

      // drop an unrelated collection
      db._drop(cn2);
      
      res = db._query(query, null, options);
      assertEqual(10, res.toArray().length);
      if (isCluster) {
        assertFalse(res.hasOwnProperty("planCacheKey"));
      } else {
        assertTrue(res.hasOwnProperty("planCacheKey"));
        // plan cache keys must be identical
        assertEqual(key, res.planCacheKey);
      }
    },
    
    testInvalidationAfterCollectionRenaming : function () {
      if (isCluster) {
        // renaming not supported in cluster
        return;
      }

      const query = `${uniqid()} FOR doc IN ${cn1} RETURN doc`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      db._query(`FOR i IN 1..10 INSERT {} INTO ${cn1}`);

      let res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      
      res = db._query(query, null, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      let key = res.planCacheKey;
      assertEqual(10, res.toArray().length);

      // rename the collection
      db._drop(cn2);
      db[cn1].rename(cn2);

      // rename it back to original name
      db[cn2].rename(cn1);
      
      res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      
      res = db._query(query, null, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      // plan cache keys are identical, but the plan is
      // refererring to a different instance of the collection
      // now.
      assertEqual(key, res.planCacheKey);
    },
    
    testInvalidationAfterCollectionPropertyChange : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} RETURN doc`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      db._query(`FOR i IN 1..10 INSERT {} INTO ${cn1}`);

      let res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      
      res = db._query(query, null, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      let key = res.planCacheKey;
      assertEqual(10, res.toArray().length);

      // change properties
      db[cn1].properties({ cacheEnabled: true });
      
      res = db._query(query, null, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      
      res = db._query(query, null, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertEqual(10, res.toArray().length);
      assertEqual(key, res.planCacheKey);
    },
    
    testInvalidationAfterIndexCreation : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value == @value RETURN doc.value`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };
      
      db._query(`FOR i IN 1..10 INSERT {value: i} INTO ${cn1}`);

      let res = db._query(query, {value: 1}, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual([1], res.toArray());
      
      res = db._query(query, {value: 2}, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      let key = res.planCacheKey;
      assertEqual([2], res.toArray());

      // create a new index on the collection
      db[cn1].ensureIndex({ type: "persistent", fields: ["value"] });
      
      res = db._query(query, {value: 3}, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual([3], res.toArray());

      res = db._query(query, {value: 4}, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertEqual([4], res.toArray());
      // plan cache keys must be identical
      assertEqual(key, res.planCacheKey);
    },
    
    testInvalidationAfterIndexDrop : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value == @value RETURN doc.value`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };
      
      db[cn1].ensureIndex({ type: "persistent", fields: ["value"], name: "value" });
      db._query(`FOR i IN 1..10 INSERT {value: i} INTO ${cn1}`);

      let res = db._query(query, {value: 1}, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual([1], res.toArray());
      
      res = db._query(query, {value: 2}, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      let key = res.planCacheKey;
      assertEqual([2], res.toArray());

      // drop the index
      db[cn1].dropIndex("value");
      
      res = db._query(query, {value: 3}, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertEqual([3], res.toArray());

      res = db._query(query, {value: 4}, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertEqual([4], res.toArray());
      // plan cache keys must be identical
      assertEqual(key, res.planCacheKey);
    },
    
    testNamedGraphQuery : function () {
      const gn = 'UnitTestsNamedGraph';
      const en = 'UnitTestsNamedGraphEdge';
      const vn = 'UnitTestsNamedGraphVertex';

      let g = graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor: 2 });

      try {
        for (let i = 0; i < 5; ++i) { 
          db[vn].insert({ _key: "test" + i });
          if (i > 0) {
            db[en].insert({ _from: vn + "/test" + (i - 1), _to: vn + "/test" + i });
          }
        }
        const query = `${uniqid()} FOR v, e, p IN 1..4 OUTBOUND '${vn}/test0' GRAPH '${gn}' RETURN {v, e, p}`;
        const options = { optimizePlanForCaching: true, usePlanCache: true };

        let res = db._query(query, null, options);
        assertFalse(res.hasOwnProperty("planCacheKey"));
        assertEqual(4, res.toArray().length);
      
        res = db._query(query, null, options);
        assertTrue(res.hasOwnProperty("planCacheKey"));
        let key = res.planCacheKey;
        assertEqual(4, res.toArray().length);
      } finally {
        graphs._drop(gn, true);
      }
    },
    
    testNamedGraphNameAsValueBindParameter : function () {
      const gn = 'UnitTestsNamedGraph';
      const en = 'UnitTestsNamedGraphEdge';
      const vn = 'UnitTestsNamedGraphVertex';

      let g = graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 1, replicationFactor: 1 });
      try {
        const query = `${uniqid()} FOR v, e, p IN 1..4 OUTBOUND '${vn}/test0' GRAPH @g RETURN {v, e, p}`;
        const options = { optimizePlanForCaching: true, usePlanCache: true };

        assertErrorsOut(query, {g: gn}, options);
      } finally {
        graphs._drop(gn, true);
      }
    },
    
    testTraversalDepthAsValueBindParameter : function () {
      const gn = 'UnitTestsNamedGraph';
      const en = 'UnitTestsNamedGraphEdge';
      const vn = 'UnitTestsNamedGraphVertex';

      let g = graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 1, replicationFactor: 1 });
      try {
        const query = `${uniqid()} FOR v, e, p IN @min..@max OUTBOUND '${vn}/test0' GRAPH '${gn}' RETURN {v, e, p}`;
        const options = { optimizePlanForCaching: true, usePlanCache: true };

        assertErrorsOut(query, {min: 1, max: 2}, options);
      } finally {
        graphs._drop(gn, true);
      }
    },
    
    testViewQuery : function () {
      const vn = 'UnitTestsView';
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      db._createView(vn, "arangosearch", {});
      try {
        db._view(vn).properties({ links: { [cn1]: { includeAllFields: true } } });

        const query = `${uniqid()} FOR doc IN ${vn} SEARCH doc.value == @value OPTIONS {waitForSync: true} RETURN doc.value`;
        
        db._query(`FOR i IN 1..10 INSERT {value: i} INTO ${cn1}`);

        let res = db._query(query, {value: 1}, options);
        assertFalse(res.hasOwnProperty("planCacheKey"));
        assertEqual([1], res.toArray());
    
        // it is possible here that the background link creation is still ongoing
        // and invalidates the plan cache entries. so we try repeatedly.
        let tries = 0;
        while (++tries < 40) {
          res = db._query(query, {value: 2}, options);
          if (res.hasOwnProperty("planCacheKey")) {
            break;
          }
          internal.sleep(0.25);
        }
        assertTrue(res.hasOwnProperty("planCacheKey"));
        assertEqual([2], res.toArray());
        // Now try to run the same query again to see if it works when the plan
        // comes from the cache:
        res = db._query(query, {value: 2}, options);
        assertTrue(res.hasOwnProperty("planCacheKey"),
          "planCacheKey missing, view query not cached");
      } finally {
        db._dropView(vn);
      }
    },

    testSingleRemoteNodePreventsCaching : function () {
      if (!isCluster) {
        // SingleRemoteOperationNode is only used in cluster
        return;
      }
      const query = `${uniqid()} INSERT {} INTO ${cn1}`;
     
      assertNotCached(query);

      let stmt = db._createStatement({ query });
      let res = stmt.explain();
      assertEqual(1, res.plan.nodes.filter((n) => n.type === 'SingleRemoteOperationNode').length, res.plan.nodes);
    },
    
    testMultipleRemoteNodePreventsCaching : function () {
      if (!isCluster) {
        // MultipleRemoteModificationNode is only used in cluster
        return;
      }
      const query = `${uniqid()} FOR doc IN [{}, {}, {}] INSERT doc INTO ${cn1}`;
      
      assertNotCached(query);

      let stmt = db._createStatement({ query });
      let res = stmt.explain();
      assertEqual(1, res.plan.nodes.filter((n) => n.type === 'MultipleRemoteModificationNode').length, res.plan.nodes);
    },
    
    testPlanAttributeNotSetForQueryFromCache : function () {
      const query = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value == @value RETURN doc.value`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };
      
      let res = db._query(query, {value: 1}, options);
      assertFalse(res.hasOwnProperty("planCacheKey"));
      assertUndefined(res.data.extra.plan, res);
      
      res = db._query(query, {value: 2}, options);
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertUndefined(res.data.extra.plan, res);
     
      // try again with profiling enabled. this must populate the plan attribute
      res = db._query(query, {value: 2}, Object.assign(options, {profile: 2}));
      assertTrue(res.hasOwnProperty("planCacheKey"));
      assertNotUndefined(res.data.extra.plan, res);
    },
    
    testPlanCacheClearing : function () {
      const options = { optimizePlanForCaching: true, usePlanCache: true };

      for (let i = 0; i < 100; ++i) {
        const query = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value${i} == @value RETURN doc.value`;
      
        db._query(query, {value: 1}, options);
      }

      let entries = planCache.toArray();
      assertNotEqual(0, entries.length, entries);
      assertTrue(entries.length > 10, entries);

      planCache.clear();
      
      entries = planCache.toArray();
      assertEqual(0, entries.length, entries);
    },

    testPlanCacheContents : function () {
      const query1 = `${uniqid()} FOR doc IN ${cn1} FILTER doc.value1 == @value RETURN doc.value`;
      const query2 = `${uniqid()} FOR doc IN @@collection FILTER doc.value1 == @value RETURN doc.value`;
      const query3 = `${uniqid()} FOR doc IN ${cn2} FILTER doc.value1 == @value RETURN doc.value`;
      const options = { optimizePlanForCaching: true, usePlanCache: true };
        
      db._query(query1, {value: 1}, options);
      db._query(query2, {"@collection": cn1, value: 1}, options);
      db._query(query3, {value: 1}, options);
      db._query(query2, {"@collection": cn2, value: 1}, options);
      db._query(query3, {value: 1}, Object.assign(options, {fullCount: true}));
      
      let buildFilter = (values) => {
        return (entry) => {
          return Object.keys(values).every((key) => {
            return JSON.stringify(entry[key]) === JSON.stringify(values[key]);
          });
        };
      };

      let entries = planCache.toArray();
      assertEqual(1, entries.filter(buildFilter({ query: query1, bindVars: {}, fullCount: false, dataSources: [cn1] })).length, entries);
      assertEqual(1, entries.filter(buildFilter({ query: query2, bindVars: {"@collection": cn1}, fullCount: false, dataSources: [cn1] })).length, entries);
      assertEqual(1, entries.filter(buildFilter({ query: query3, bindVars: {}, fullCount: false, dataSources: [cn2] })).length, entries);
      assertEqual(1, entries.filter(buildFilter({ query: query2, bindVars: {"@collection": cn2}, fullCount: false, dataSources: [cn2] })).length, entries);
      assertEqual(1, entries.filter(buildFilter({ query: query3, bindVars: {}, fullCount: true, dataSources: [cn2] })).length, entries);
    },
  };
}

jsunity.run(QueryPlanCacheTestSuite);
return jsunity.done();
