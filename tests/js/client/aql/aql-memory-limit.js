/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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

const internal = require("internal");
const errors = internal.errors;
const jsunity = require("jsunity");
const db = require("@arangodb").db;
const isCluster = require("internal").isCluster();

function ahuacatlMemoryLimitStaticQueriesTestSuite () {
  return {

    testUnlimited : function () {
      let actual = db._query("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)").toArray();
      assertEqual(100000, actual.length);
      
      actual = db._query("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", null, { memoryLimit: 0 }).toArray();
      assertEqual(100000, actual.length);
    },

    testLimitedButValid : function () {
      let actual = db._query("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", null, { memoryLimit: 100 * 1000 * 1000 }).toArray();
      assertEqual(100000, actual.length);
      
      // should still be ok
      actual = db._query("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", null, { memoryLimit: 10 * 1000 * 1000 }).toArray();
      assertEqual(100000, actual.length);
      
      // should still be ok
      actual = db._query("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", null, { memoryLimit: 5 * 1000 * 1000 }).toArray();
      assertEqual(100000, actual.length);
      
      // should still be ok
      actual = db._query("FOR i IN 1..10000 RETURN i", null, { memoryLimit: 1000 * 1000 }).toArray();
      assertEqual(10000, actual.length);
      
      // should still be ok
      actual = db._query("FOR i IN 1..10000 RETURN i", null, { memoryLimit: 100 * 1000 + 4096 }).toArray();
      assertEqual(10000, actual.length);
    },

    testLimitedAndInvalid : function () {
      const queries = [
        [ "FOR i IN 1..100000 SORT CONCAT('foobarbaz', i) RETURN CONCAT('foobarbaz', i)", 200000 ],
        [ "FOR i IN 1..100000 SORT CONCAT('foobarbaz', i) RETURN CONCAT('foobarbaz', i)", 100000 ],
        [ "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", 20000 ],
        [ "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", 10000 ],
        [ "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", 1000 ],
        [ "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", 100 ],
        [ "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)", 10000 ],
        [ "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)", 1000 ],
        [ "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)", 100 ],
        [ "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)", 10 ]
      ];

      queries.forEach(function(q) {
        try {
          db._query(q[0], null, { memoryLimit: q[1] });
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        }
      });
    },

  };
}

function ahuacatlMemoryLimitReadOnlyQueriesTestSuite () {
  const cn = "UnitTestsCollection";

  let c;

  return {
    setUpAll : function () {
      // only one shard because that is more predictable for memory usage
      c = db._create(cn, { numberOfShards: 1 });

      let docs = [];
      for (let i = 0; i < 100 * 1000; ++i) {
        docs.push({ value1: i, value2: i % 10, _key: "test" + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
    },
    
    tearDownAll : function () {
      db._drop(cn);
    },

    testFullScan : function () {
      const query = "FOR doc IN " + cn + " RETURN doc";
      
      let actual = db._query(query, null, { memoryLimit: 10 * 1000 * 1000 }).toArray();
      assertEqual(100000, actual.length);
        
      try {
        db._query(query, null, { memoryLimit: 5 * 1000 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },
    
    testIndexScan : function () {
      const query = "FOR doc IN " + cn + " SORT doc._key RETURN doc";
      
      let actual = db._query(query, null, { memoryLimit: 10 * 1000 * 1000 }).toArray();
      assertEqual(100000, actual.length);
        
      try {
        db._query(query, null, { memoryLimit: 5 * 1000 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

    testSort : function () {
      // turn off constrained heap sort
      const optimizer = { rules: ["-sort-limit"] };
      const query = "FOR doc IN " + cn + " SORT doc.value1 LIMIT 10 RETURN doc";
      
      let actual = db._query(query, null, { memoryLimit: 15 * 1000 * 1000, optimizer }).toArray();
      assertEqual(10, actual.length);
        
      try {
        db._query(query, null, { memoryLimit: 10 * 1000 * 1000, optimizer });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },
    
    testCollectOnUniqueAttribute : function () {
      // values of doc.value1 are all unique
      const query = "FOR doc IN " + cn + " COLLECT v = doc.value1 OPTIONS { method: 'hash' } RETURN v";
      
      let actual = db._query(query, null, { memoryLimit: 10 * 1000 * 1000 }).toArray();
      assertEqual(100000, actual.length);
      
      try {
        db._query(query, null, { memoryLimit: 5 * 1000 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },
    
    testCollectOnRepeatedAttribute : function () {
      // values of doc.value2 are repeating a lot (only 10 different values)
      const query = "FOR doc IN " + cn + " COLLECT v = doc.value2 OPTIONS { method: 'hash' } RETURN v";
      
      let actual = db._query(query, null, { memoryLimit: 1000 * 1000 }).toArray();
      assertEqual(10, actual.length);
      
      actual = db._query(query, null, { memoryLimit: 500 * 1000 }).toArray();
      assertEqual(10, actual.length);
        
      try {
        db._query(query, null, { memoryLimit: 10 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },
  };
}

function ahuacatlMemoryLimitGraphQueriesTestSuite () {
  const vn = "UnitTestsVertex";
  const en = "UnitTestsEdge";

  return {
    setUpAll : function () {
      db._drop(en);
      db._drop(vn);
      
      const n = 400;

      // only one shard because that is more predictable for memory usage
      let c = db._create(vn, { numberOfShards: 1 });

      let docs = [];
      for (let i = 0; i <= n; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);

      c = db._createEdgeCollection(en, { numberOfShards: 1 });

      const weight = 1;
      
      docs = [];
      for (let i = 0; i < n; ++i) {
        for (let j = i + 1; j < n; ++j) {
          docs.push({ _from: vn + "/test" + i, _to: vn + "/test" + j, weight });
          if (docs.length === 5000) {
            c.insert(docs);
            docs = [];
          }
        }
      }
      if (docs.length) {
        c.insert(docs);
      }
    },
    
    tearDownAll : function () {
      db._drop(en);
      db._drop(vn);
    },
    
    testKShortestPaths : function () {
      for (let o of ['', ' OPTIONS { algorithm: "legacy" }']) {
        const query = "WITH " + vn + " FOR p IN OUTBOUND K_SHORTEST_PATHS '" + vn + "/test0' TO '" + vn + "/test11' " + en + o + " RETURN p";

        if (isCluster) {
          let actual = db._query(query, null, {memoryLimit: 20 * 1000 * 1000}).toArray();
          // no shortest path available
          assertEqual(1024, actual.length);
        } else {
          let actual = db._query(query, null, {memoryLimit: 9 * 1000 * 1000 + 4 * 1000}).toArray();
          // no shortest path available
          assertEqual(1024, actual.length);
        }

        try {
          db._query(query, null, {memoryLimit: 1000 * 1000});
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        }
      }
    },

    testAllShortestPaths : function () {
      const query = "WITH " + vn + " FOR p IN OUTBOUND ALL_SHORTEST_PATHS '" + vn + "/test0' TO '" + vn + "/test11' " + en + " RETURN p";

      let actual = db._query(query, null, { memoryLimit: 20 * 1000 * 1000 }).toArray();

      // no shortest path available
      assertEqual(1, actual.length);

      try {
        db._query(query, null, { memoryLimit: 30 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

    testKPaths : function () {
      const query = "WITH " + vn + " FOR p IN OUTBOUND K_PATHS '" + vn + "/test0' TO '" + vn + "/test317' " + en + " RETURN p";

      let actual = db._query(query, null, { memoryLimit: 20 * 1000 * 1000 }).toArray();
      // no shortest path available
      assertEqual(1, actual.length);
      
      try {
        db._query(query, null, { memoryLimit: 30 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },
    
    testShortestPathDefaultWeight : function () {
      const query = "WITH " + vn + " FOR p IN ANY SHORTEST_PATH '" + vn + "/test0' TO '" + vn + "/test310' " + en + " RETURN p";
      
      let actual = db._query(query, null, { memoryLimit: 30 * 1000 * 1000 }).toArray();
      assertEqual(2, actual.length);
      
      try {
        db._query(query, null, { memoryLimit: 30 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },
    
    testShortestPathWeightAttribute : function () {
      const query = "WITH " + vn + " FOR p IN ANY SHORTEST_PATH '" + vn + "/test0' TO '" + vn + "/test310' " + en + " RETURN p";
      
      let actual = db._query(query, null, { memoryLimit: 30 * 1000 * 1000, weightAttribute: "weight" }).toArray();
      assertEqual(2, actual.length);
      
      try {
        db._query(query, null, { memoryLimit: 30 * 1000, weightAttribute: "weight" });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

    testTraversal : function () {
      const query = "WITH " + vn + " FOR v, e, p IN 1..@maxDepth OUTBOUND '" + vn + "/test0' " + en + " RETURN v";

      if (isCluster) {
        let actual = db._query(query, { maxDepth: 2 }, { memoryLimit: 27 * 1000 * 1000 }).toArray();
        assertEqual(79800, actual.length);
      } else {
        let actual = db._query(query, { maxDepth: 2 }, { memoryLimit: 20 * 1000 * 1000 }).toArray();
        assertEqual(79800, actual.length);
      }
      
      try {
        // run query with same depth, but lower mem limit
        db._query("WITH " + vn + " FOR v, e, p IN 1..@maxDepth OUTBOUND '" + vn + "/test0' " + en + " RETURN v", { maxDepth: 2 }, { memoryLimit: 2 * 1000 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
        
      try {
        // increase traversal depth
        db._query("WITH " + vn + " FOR v, e, p IN 1..@maxDepth OUTBOUND '" + vn + "/test0' " + en + " RETURN v", { maxDepth: 5 }, { memoryLimit: 10 * 1000 * 1000 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

  };
}

function ahuacatlMemoryLimitSkipTestSuite () {
  const cn = "UnitTestsCollection";

  let c;

  return {
    setUpAll : function () {
      // only one shard because that is more predictable for memory usage
      c = db._create(cn, { numberOfShards: 1 });
      const payload = Array(1024).join("x");

      let docs = [];
      for (let i = 0; i < 80 * 1000; ++i) {
        docs.push({ payload });
        if (docs.length === 100) {
          c.insert(docs);
          docs = [];
        }
      }
    },
    
    tearDownAll : function () {
      db._drop(cn);
    },

    testUpdate : function () {
      const query = "FOR doc IN " + cn + " UPDATE doc WITH { foobar: 'bazbarkqux', p2: doc.payload } IN " + cn;
      const options = { memoryLimit: 420 * 1000 * 1000 };
      
      let actual = db._query(query, null, options).toArray();
      assertEqual(0, actual.length);
      
      // this checks whether the same query uses substantially more memory when followed by a COLLECT WITH COUNT INTO.
      // this is not expected to use more memory than the original query, but due to an issue in the ModificationExecutor
      // it actually did. This was reported in OASIS-25321.
      actual = db._query(query + " COLLECT WITH COUNT INTO c RETURN c", null, options).toArray();
      assertEqual(1, actual.length);
      assertEqual(c.count(), actual[0]);
    },
  };
}

function ahuacatMemoryLimitSortedCollectTestSuite() {
  const TEST_COLLECTION = "testDocs";
  const TEST_EMPTY_COLLECTION = "testEmpty";

  let testCollection;

  function tearDown() {
    db._drop(TEST_COLLECTION);
    db._drop(TEST_EMPTY_COLLECTION);
  }

  return {
    setUpAll: function () {
      tearDown();

      testCollection = db._create(TEST_COLLECTION);
      for (let i = 1; i <= 100; ++i) {

        testCollection.save({
          grp: "foobarbazfoobarbaz" + i,
          num: i,
          txt: "x".repeat(1024),
          obj: {object: "object" + i},
          bool: (i % 2 === 0),
          arr: [i * 10, "baz", {foo: "bar"}, (i % 2 === 0)],
          etc: null
        });
      }

      for (let i = 1; i <= 100; ++i) {
        testCollection.save({
          grp: "foobarbazfoobarbaz" + i,
          num: i * 3.14,
          txt: " ".repeat(1024),
          obj: {},
          bool: (i % 2 === 1),
          arr: [i * -3.14, "", {foo: []}, (i % 2 === 1)],
          etc: null
        });
      }
    },

    tearDownAll: tearDown,

    testSortedCollectAggregateUniqueIntoWithinLimit: function () { // leftover 99683
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE
            num  = UNIQUE(d.num),
            txt  = UNIQUE(d.txt),
            obj  = UNIQUE(d.obj),
            bool = UNIQUE(d.bool),
            arr  = UNIQUE(d.arr),
            etc  = UNIQUE(d.etc)
          INTO doc = d
          RETURN { grp, num, txt, obj, bool, arr, etc, doc }
      `;
      const res = db._query(query, null, {memoryLimit: 1024 * 1024 * 1024}).toArray();
      assertEqual(100, res.length);
      assertTrue(res[0] !== null);
    },

    testSortedCollectAggregateUniqueIntoExceedLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE
            num  = UNIQUE(d.num),
            txt  = UNIQUE(d.txt),
            obj  = UNIQUE(d.obj),
            bool = UNIQUE(d.bool),
            arr  = UNIQUE(d.arr),
            etc  = UNIQUE(d.etc)
          INTO doc = d
          RETURN { grp, num, txt, obj, bool, arr, etc, doc }
      `;
      try {
        db._query(query, null, {memoryLimit: 800 * 1024}).toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

    testSortedCollectAggregateUniqueIntoWithNullAggregateAndIntoWithinLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE notExist = UNIQUE(d.notExist)
          INTO doc = d.etc
          RETURN { grp, notExist, doc }
      `;
      const res = db._query(query, null, {memoryLimit: 1024 * 1024 * 1024}).toArray();
      assertEqual(100, res.length);
      assertTrue(res[0] !== null);
    },

    testSortedCollectAggregatePushIntoWithinLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE
            num  = PUSH(d.num),
            txt  = PUSH(d.txt),
            obj  = PUSH(d.obj),
            bool = PUSH(d.bool),
            arr  = PUSH(d.arr),
            etc  = PUSH(d.etc)
          INTO doc = d
          RETURN { grp, num, txt, obj, bool, arr, etc, doc }
      `;
      const res = db._query(query, null, {memoryLimit: 1024 * 1024 * 1024}).toArray();
      assertEqual(100, res.length);
      assertTrue(res[0] !== null);
    },

    testSortedCollectAggregatePushIntoExceedLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE
            num  = PUSH(d.num),
            txt  = PUSH(d.txt),
            obj  = PUSH(d.obj),
            bool = PUSH(d.bool),
            arr  = PUSH(d.arr),
            etc  = PUSH(d.etc)
          INTO doc = d
          RETURN { grp, num, txt, obj, bool, arr, etc, doc }
      `;
      try {
        db._query(query, null, {memoryLimit: 800 * 1024}).toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

    testSortedCollectAggregatePushIntoWithNullAggregateAndIntoWithinLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE notExist = PUSH(d.notExist)
          INTO doc = d.etc
          RETURN { grp, notExist, doc }
      `;
      const res = db._query(query, null, {memoryLimit: 1024 * 1024 * 1024}).toArray();
      assertEqual(100, res.length);
      assertTrue(res[0] !== null);
    },

    testSortedCollectAggregateMinIntoWithinLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE
            num  = MIN(d.num),
            txt  = MIN(d.txt),
            obj  = MIN(d.obj),
            bool = MIN(d.bool),
            arr  = MIN(d.arr),
            etc  = MIN(d.etc)
          INTO doc = d
          RETURN { grp, num, txt, obj, bool, arr, etc, doc }
      `;
      const res = db._query(query, null, {memoryLimit: 1024 * 1024 * 1024}).toArray();
      assertEqual(100, res.length);
      assertTrue(res[0] !== null);
    },

    testSortedCollectAggregateMinIntoExceedLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE
            num  = MIN(d.num),
            txt  = MIN(d.txt),
            obj  = MIN(d.obj),
            bool = MIN(d.bool),
            arr  = MIN(d.arr),
            etc  = MIN(d.etc)
          INTO doc = d
          RETURN { grp, num, txt, obj, bool, arr, etc, doc }
      `;
      try {
        db._query(query, null, {memoryLimit: 800 * 1024}).toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

    testSortedCollectAggregateMinIntoWithNullAggregateAndIntoWithinLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE notExist = MIN(d.notExist)
          INTO doc = d.etc
          RETURN { grp, notExist, doc }
      `;
      const res = db._query(query, null, {memoryLimit: 1024 * 1024 * 1024}).toArray();
      assertEqual(100, res.length);
      assertTrue(res[0] !== null);
    },

    testSortedCollectAggregateMaxIntoWithinLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE
            num  = MAX(d.num),
            txt  = MAX(d.txt),
            obj  = MAX(d.obj),
            bool = MAX(d.bool),
            arr  = MAX(d.arr),
            etc  = MAX(d.etc)
          INTO doc = d
          RETURN { grp, num, txt, obj, bool, arr, etc, doc }
      `;
      const res = db._query(query, null, {memoryLimit: 1024 * 1024 * 1024}).toArray();
      assertEqual(100, res.length);
      assertTrue(res[0] !== null);
    },

    testSortedCollectAggregateMaxIntoExceedLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE
            num  = MAX(d.num),
            txt  = MAX(d.txt),
            obj  = MAX(d.obj),
            bool = MAX(d.bool),
            arr  = MAX(d.arr),
            etc  = MAX(d.etc)
          INTO doc = d
          RETURN { grp, num, txt, obj, bool, arr, etc, doc }
      `;
      try {
        db._query(query, null, {memoryLimit: 800 * 1024}).toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

    testSortedCollectAggregateMaxIntoWithNullAggregateAndIntoWithinLimit: function () {
      const query = `
        FOR d IN ${TEST_COLLECTION}
          SORT d.grp
          COLLECT grp = d.grp
          AGGREGATE notExist = MAX(d.notExist)
          INTO doc = d.etc
          RETURN { grp, notExist, doc }
      `;
      const res = db._query(query, null, {memoryLimit: 1024 * 1024 * 1024}).toArray();
      assertEqual(100, res.length);
      assertTrue(res[0] !== null);
    },
  };
}

jsunity.run(ahuacatlMemoryLimitStaticQueriesTestSuite);
jsunity.run(ahuacatlMemoryLimitReadOnlyQueriesTestSuite);
jsunity.run(ahuacatlMemoryLimitGraphQueriesTestSuite);
jsunity.run(ahuacatlMemoryLimitSkipTestSuite);
jsunity.run(ahuacatMemoryLimitSortedCollectTestSuite);

return jsunity.done();
