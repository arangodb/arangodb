/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, fail */

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const errors = internal.errors;
const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();

const collectionName = "UnitTestWindowCollection";

function WindowCumulativeSumTestSuite () {
  let validateResult = function(result, size, from, to, preceding, following) {
    assertEqual(to - from + 1, result.length);
    result.forEach((value, index) => {
      let low = Math.max(0, index - preceding) + from;
      let high = Math.min(size, index + following) + from;
      let expectedSum = 0;
      for (let i = low; i <= high; ++i) {
        expectedSum += i;
      };

      assertEqual(expectedSum, value, { from, to, preceding, following, size, expectedSum, value, index, low, high });
    });
  };

  let sizedCumulativeSumTest = function(size, offset, preceding, following) {
    let from = offset;
    let to = offset + size;

    let query = `
FOR i IN @from .. @to 
  LET value = NOOPT(i) 
  WINDOW {preceding: @preceding, following: @following} AGGREGATE sum = SUM(value)
  RETURN sum
`;
    const bind = { from, to, preceding, following };
    let result = db._query(query, bind).toArray();
    validateResult(result, size, from, to, preceding, following);

    // test range based variety
    query = `
    FOR i IN @from .. @to 
      LET value = NOOPT(i) 
      WINDOW value WITH {preceding: @preceding, following: @following} AGGREGATE sum = SUM(value)
      RETURN sum
    `;
    result = db._query(query, bind).toArray();
    validateResult(result, size, from, to, preceding, following);
  };
  
  let cumulativeSumTests = function(preceding, following) {
    const sizes = [ 5, 10, 50, 250, 999, 1000, 1001, 5000 ];

    sizes.forEach((size) => sizedCumulativeSumTest(size, 0, preceding, following));
    sizes.forEach((size) => sizedCumulativeSumTest(size, 1, preceding, following));
    sizes.forEach((size) => sizedCumulativeSumTest(size, 42, preceding, following));
  };

  return {
    testResultsPreceding0Following0 : function () {
      cumulativeSumTests(0, 0);
    },
  
    testResultsPreceding0Following1 : function () {
      cumulativeSumTests(0, 1);
    },
    
    testResultsPreceding0Following2 : function () {
      cumulativeSumTests(0, 2);
    },
    
    testResultsPreceding0Following3 : function () {
      cumulativeSumTests(0, 3);
    },
    
    testResultsPreceding0Following5 : function () {
      cumulativeSumTests(0, 5);
    },
    
    testResultsPreceding0Following10 : function () {
      cumulativeSumTests(0, 10);
    },
    
    testResultsPreceding0Following50 : function () {
      cumulativeSumTests(0, 50);
    },
    
    testResultsPreceding0Following100 : function () {
      cumulativeSumTests(0, 100);
    },
    
    testResultsPreceding0Following150 : function () {
      cumulativeSumTests(0, 150);
    },
    
    testResultsPreceding0Following999 : function () {
      cumulativeSumTests(0, 999);
    },
    
    testResultsPreceding0Following1000 : function () {
      cumulativeSumTests(0, 1000);
    },
    
    testResultsPreceding0Following1001 : function () {
      cumulativeSumTests(0, 1001);
    },
    
    /*testResultsPreceding0Following5000 : function () {
      cumulativeSumTests(0, 5000);
    },
    
    testResultsPreceding0Following10000 : function () {
      cumulativeSumTests(0, 10000);
    },*/
    
    testResultsPreceding0Following10001 : function () {
      cumulativeSumTests(0, 10001);
    },
    
    testResultsPreceding1Following0 : function () {
      cumulativeSumTests(1, 0);
    },
  
    testResultsPreceding2Following0 : function () {
      cumulativeSumTests(2, 0);
    },
    
    testResultsPreceding3Following0 : function () {
      cumulativeSumTests(3, 0);
    },
    
    testResultsPreceding5Following0 : function () {
      cumulativeSumTests(5, 0);
    },
    
    testResultsPreceding010Following0 : function () {
      cumulativeSumTests(10, 0);
    },
    
    testResultsPreceding50Following0 : function () {
      cumulativeSumTests(50, 0);
    },
    
    testResultsPreceding100Following0 : function () {
      cumulativeSumTests(100, 0);
    },
    
    testResultsPreceding150Following0 : function () {
      cumulativeSumTests(150, 0);
    },
    
    testResultsPreceding999Following0 : function () {
      cumulativeSumTests(999, 0);
    },
    
    testResultsPreceding1000Following0 : function () {
      cumulativeSumTests(1000, 0);
    },
    
    testResultsPreceding1001Following0 : function () {
      cumulativeSumTests(1001, 0);
    },
    
    /*testResultsPreceding5000Following0 : function () {
      cumulativeSumTests(5000, 0);
    },
    
    testResultsPreceding10000Following0 : function () {
      cumulativeSumTests(10000, 0);
    },
    
    testResultsPreceding10001Following0 : function () {
      cumulativeSumTests(10001, 0);
    },*/
  
    testResultsPreceding1Following1 : function () {
      cumulativeSumTests(1, 1);
    },
    
    testResultsPreceding1Following2 : function () {
      cumulativeSumTests(1, 2);
    },
    
    testResultsPreceding1Following10 : function () {
      cumulativeSumTests(1, 10);
    },
    
    testResultsPreceding1Following50 : function () {
      cumulativeSumTests(1, 50);
    },
    
    testResultsPreceding2Following1 : function () {
      cumulativeSumTests(2, 1);
    },
    
    testResultsPreceding2Following2 : function () {
      cumulativeSumTests(2, 2);
    },
    
    testResultsPreceding2Following10 : function () {
      cumulativeSumTests(2, 10);
    },
    
    testResultsPreceding2Following50 : function () {
      cumulativeSumTests(2, 10);
    },
    
    testResultsPreceding5Following1 : function () {
      cumulativeSumTests(5, 1);
    },
    
    testResultsPreceding5Following2 : function () {
      cumulativeSumTests(5, 1);
    },
    
    testResultsPreceding5Following5 : function () {
      cumulativeSumTests(5, 5);
    },
    
    testResultsPreceding5Following10 : function () {
      cumulativeSumTests(5, 10);
    },
    
    testResultsPreceding5Following50 : function () {
      cumulativeSumTests(5, 50);
    },
    
    testResultsPreceding10Following1 : function () {
      cumulativeSumTests(10, 1);
    },
    
    testResultsPreceding10Following10 : function () {
      cumulativeSumTests(10, 10);
    },
    
    testResultsPreceding10Following100 : function () {
      cumulativeSumTests(10, 100);
    },
    
    testResultsPreceding100Following1 : function () {
      cumulativeSumTests(100, 1);
    },
    
    testResultsPreceding100Following10 : function () {
      cumulativeSumTests(100, 10);
    },
    
    testResultsPreceding100Following100 : function () {
      cumulativeSumTests(100, 100);
    },
    
    testResultsPreceding100Following1000 : function () {
      cumulativeSumTests(100, 1000);
    },

    testResultUnboundedPreceding: function() {
      let query = `
      FOR i IN @from .. @to 
        LET value = NOOPT(i) 
        WINDOW {preceding: 'unbounded', following: 0} AGGREGATE sum = SUM(value)
        RETURN sum
      `;

      const sizes = [5, 999, 1000, 1001, 5000];

      sizes.forEach(size => {
        const froms = [0, 1, 42, 1000, 1001, 5000];
        froms.forEach(from => {
          const bind = {from, to: size + from};
          let result = db._query(query, bind).toArray();
          validateResult(result, size, /*from*/from, /*to*/size + from, /*preceding*/size, /*following*/0);
        });
      });      
    },

    // test invalid preceding values
    testInvalidPrecedingValues : function () {
      const query = `
FOR i IN 1..10 
  WINDOW {preceding: @preceding, following: @following} AGGREGATE sum = SUM(i)
  RETURN sum
`;

      const invalid = [null, false, true, -1, -10, -1000, -1.1, "foo", "bar", [], {}];
      invalid.forEach((value) => {
        try {
          db._query(query, { preceding: value, following: 1 });
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_BAD_PARAMETER.code, value);
        }
      });
    },
   
    // test invalid following values
    testInvalidFollowingValues : function () {
      const query = `
FOR i IN 1..10 
  WINDOW {preceding: @preceding, following: @following} AGGREGATE sum = SUM(i)
  RETURN sum
`;

      const invalid = [null, false, true, -1, -10, -1000, -1.1, "foo", "bar", [], {}];
      invalid.forEach((value) => {
        try {
          db._query(query, { preceding: 1, following: value });
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_BAD_PARAMETER.code, value);
        }
      });
    },
    
    // test invalid preceding and following values
    testInvalidPrecedingAndFollowingValues : function () {
      const query = `
FOR i IN 1..10 
  WINDOW {preceding: @preceding, following: @following} AGGREGATE sum = SUM(i)
  RETURN sum
`;

      const invalid = [null, false, true, -1, -10, -1000, -1.1, "foo", "bar", [], {}];
      invalid.forEach((value1) => {
        invalid.forEach((value2) => {
          try {
            db._query(query, { preceding: value1, following: value2 });
            fail();
          } catch (err) {
            assertEqual(err.errorNum, errors.ERROR_BAD_PARAMETER.code, { value1, value2 });
          }
        });
      });
    },
    
    // test dynamic preceding/following values
    testDynamicPrecedingValue : function () {
      const query = `
FOR i IN 1..10 
  WINDOW {preceding: i, following: 1} AGGREGATE sum = SUM(i)
  RETURN sum
`;

      try {
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_COMPILE_TIME_OPTIONS.code);
      }
    },
    
    // test dynamic preceding/following values
    testDynamicFollowingValue : function () {
      const query = `
FOR i IN 1..10 
  WINDOW {preceding: 1, following: i} AGGREGATE sum = SUM(i)
  RETURN sum
`;

      try {
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_COMPILE_TIME_OPTIONS.code);
      }
    },

    testChainedWindowStatements : function () {
      let query = `
      FOR i IN @from .. @to 
        LET value = NOOPT(i) 
        WINDOW {preceding: 'unbounded', following: 0} AGGREGATE sum = SUM(value)
        WINDOW {preceding: 'unbounded', following: 0} AGGREGATE length = LENGTH(value)
        WINDOW {preceding: 'unbounded', following: 0} AGGREGATE m = MAX(value)
        RETURN {sum: sum, length: length, max: m}
      `;

      const sizes = [5, 999, 1000, 1001, 5000];

      sizes.forEach(size => {
        const froms = [0, 1, 42, 1000, 1001, 5000];
        froms.forEach(from => {
          const bind = {from, to: size + from};
          let result = db._query(query, bind).toArray();

          let sums = result.map(o => o.sum);
          validateResult(sums, size, /*from*/from, /*to*/size + from, /*preceding*/size, /*following*/0);

          result.forEach( (value, index) => {
            assertEqual(index + 1, value.length, value);
            assertEqual(index + from, value.max, value);
          });
        });
      });   
    },

    testChainedWindowStatements2 : function () {
      let query = `
      FOR i IN @from .. @to 
        LET value = NOOPT(i) 
        WINDOW {preceding: 'unbounded', following: 0} AGGREGATE sum = SUM(value)
        WINDOW {preceding: 10, following: 0} AGGREGATE length = LENGTH(value)
        WINDOW {preceding: 0, following: 10} AGGREGATE m = MAX(value)
        RETURN {sum: sum, length: length, max: m}
      `;

      const sizes = [5, 999, 1000, 1001, 5000];

      sizes.forEach(size => {
        const froms = [0, 1, 42, 1000, 1001, 5000];
        froms.forEach(from => {
          const bind = {from, to: size + from};
          let result = db._query(query, bind).toArray();

          let sums = result.map(o => o.sum);
          validateResult(sums, size, /*from*/from, /*to*/size + from, /*preceding*/size, /*following*/0);

          result.forEach( (value, index) => {
            assertEqual(Math.min(11, index + 1), value.length, value);
            assertEqual(Math.min(index + from + 10, size + from), value.max, value);
          });
        });
      });   
    },
  };
}

const generateWindowBoundsSuite = (namePrefix) => {
  // This test suite expect a collection with 10 documents and keys 0..9 and value v: 0..9 respectively
  // And timestamps like `"time": "2021-05-25 07:00:00",` increasing by 1 minute each

  return {
    [`testAggregate${namePrefix}`]: function() {
      const query = `
        FOR doc IN ${collectionName}
        SORT doc.key
        LET key = doc.key
        WINDOW {preceding: "unbounded", following: 0} AGGREGATE i = SUM(1)
        RETURN {key, i}`;
      const result = db._query(query).toArray();
      for (let i = 0; i < result.length; ++i) {
        assertEqual(result[i].key, `${i}`);
        assertEqual(result[i].i, i + 1);
      }
    },

    [`testLength${namePrefix}`]: function() {
      const query = `
        FOR doc IN ${collectionName}
        SORT doc.key
        LET key = doc.key
        WINDOW {preceding: "unbounded", following: 0} AGGREGATE i = LENGTH()
        RETURN {key, i}`;
      const result = db._query(query).toArray();
      for (let i = 0; i < result.length; ++i) {
        assertEqual(result[i].key, `${i}`);
        assertEqual(result[i].i, i + 1);
      }
    },

    [`testRangeWindow${namePrefix}`]: function() {
      const query = `
        FOR doc IN ${collectionName}
        LET key = doc.key
        WINDOW doc.v WITH {preceding: 100, following: 0} AGGREGATE i = LENGTH()
        RETURN {key, i}`;
      const result = db._query(query).toArray();
      // As Docs key and v are identical, this should yield
      // a result sorted by key.
      for (let i = 0; i < result.length; ++i) {
        assertEqual(result[i].key, `${i}`);
        assertEqual(result[i].i, i + 1);
      }
    },

    [`testRangeDuration${namePrefix}`]: function() {
      const query = `
        FOR doc IN ${collectionName}
        LET key = doc.key
        WINDOW DATE_TIMESTAMP(doc.time) WITH { preceding: "PT03M" }
        AGGREGATE i = LENGTH()
        RETURN {key, i}`;
      const result = db._query(query).toArray();
      // As Docs key and v are identical, this should yield
      // a result sorted by key.
      for (let i = 0; i < result.length; ++i) {
        assertEqual(result[i].key, `${i}`);
        if (i < 4) {
          assertEqual(result[i].i, i + 1);
        } else {
          // All other will have 3 in range + themself
          assertEqual(result[i].i, 4);
        }
      }
    },
  };
};

function WindowBoundsManyShardSuite() {
  const suite = generateWindowBoundsSuite("ManyShards");
  suite.setUpAll = function() {
    db._create(collectionName, {numberOfShards: 3});
    let docs = [];
    for (let i = 0; i < 10; ++i) {
      docs.push({
        key: `${i}`,
        v: i,
        time: `2021-05-25 07:${i.toString().padStart(2, '0')}:00`,
      });
    }
    db._collection(collectionName).save(docs);
  };
  suite.tearDownAll = function() {
    db._drop(collectionName);
  };
  return suite;
}

function WindowBoundsNonKeyShardedSuite() {
  const suite = generateWindowBoundsSuite("NonKeySharded");
  suite.setUpAll = function() {
    db._create(collectionName, {numberOfShards: 3, shardKeys: ["value"]});
    let docs = [];
    for (let i = 0; i < 10; ++i) {
      docs.push({key: `${i}`,
        value: `v_${i % 3}`,
        v: i,
        time: `2021-05-25 07:${i.toString().padStart(2, '0')}:00`,
      });
    }
    db._collection(collectionName).save(docs);
  };
  suite.tearDownAll = function() {
    db._drop(collectionName);
  };
  return suite;
}

function WindowBoundsSuite() {
  const suite = generateWindowBoundsSuite("SingleShard");
  suite.setUpAll = function() {
    db._create(collectionName, {numberOfShards: 1});
    let docs = [];
    for (let i = 0; i < 10; ++i) {
      docs.push({key: `${i}`,
        v: i,
        time: `2021-05-25 07:${i.toString().padStart(2, '0')}:00`
      });
    }
    db._collection(collectionName).save(docs);
  };
  suite.tearDownAll = function() {
    db._drop(collectionName);
  };
  return suite;
}


function WindowBoundsOneShardSuite() {
  const suite = generateWindowBoundsSuite("OneShardDB");
  suite.setUpAll = function() {
    db._createDatabase("UnitTestOneShard", {sharding: "single"});
    db._useDatabase("UnitTestOneShard");
    db._create(collectionName);
    let docs = [];
    for (let i = 0; i < 10; ++i) {
      docs.push({key: `${i}`,
        v: i,
        time: `2021-05-25 07:${i.toString().padStart(2, '0')}:00`
      });
    }
    db._collection(collectionName).save(docs);
  };
  suite.tearDownAll = function() {
    db._useDatabase("_system");
    db._dropDatabase("UnitTestOneShard");
  };
  return suite;
}

jsunity.run(WindowCumulativeSumTestSuite);
jsunity.run(WindowBoundsSuite);
if (isCluster) {
  jsunity.run(WindowBoundsManyShardSuite);
  jsunity.run(WindowBoundsNonKeyShardedSuite);
  if (isEnterprise) {
    jsunity.run(WindowBoundsOneShardSuite);
  }
}
return jsunity.done();
