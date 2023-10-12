/*jshint globalstrict:false, strict:false, maxlen: 200, unused: false*/
/*global assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the query cache
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
const arangodb = require("@arangodb");
const db = arangodb.db;

const cache = require("@arangodb/aql/cache");

function AqlQueryCacheSuite () {
  'use strict';
  
  const cn = "UnitTestsQueryCache";
  const cached = { cache: true };
  const notCached = { cache: false };

  let assertNotInCache = function(query) {
    let queries = cache.toArray().filter(function(q) {
      return q.query === query;
    });
    assertEqual(0, queries.length);
  };

  let assertInCache = function(query, hits) {
    let queries = cache.toArray().filter(function(q) {
      return q.query === query;
    });
    assertEqual(1, queries.length);
    assertEqual(hits, queries[0].hits);
  };

  let resetCache = function() {
    cache.properties({ 
      mode: "off", 
      maxResults: 128, 
      maxResultsSize: 8 * 1024 * 1024, 
      maxEntrySize: 16 * 1024 * 1024
    });
    cache.clear();
  };

  return {

    setUp : function () { resetCache(); },
    tearDown : function () { resetCache(); },
    
    testProperties : function () {
      cache.properties({ mode: "demand", maxResults: 3, maxResultsSize: 16399, maxEntrySize: 123456 });

      let p = cache.properties();
      assertEqual("demand", p.mode);
      assertEqual(3, p.maxResults);
      assertEqual(16399, p.maxResultsSize);
      assertEqual(123456, p.maxEntrySize);
      
      cache.properties({ mode: "off", maxResults: 99, maxEntrySize: 6886, maxResultsSize: 65538 });
      p = cache.properties();
      
      assertEqual("off", p.mode);
      assertEqual(99, p.maxResults);
      assertEqual(65538, p.maxResultsSize);
      assertEqual(6886, p.maxEntrySize);
      
      cache.properties({ mode: "demand", maxResults: 99999, maxEntrySize: 123456788113 });
      p = cache.properties();
      
      assertEqual("demand", p.mode);
      assertEqual(99999, p.maxResults);
      assertEqual(65538, p.maxResultsSize);
      assertEqual(123456788113, p.maxEntrySize);
      
      cache.properties({ mode: "on" });
      p = cache.properties();
      
      assertEqual("on", p.mode);
      assertEqual(99999, p.maxResults);
      assertEqual(65538, p.maxResultsSize);
      assertEqual(123456788113, p.maxEntrySize);
      
      cache.properties({});
      p = cache.properties();
      
      assertEqual("on", p.mode);
      assertEqual(99999, p.maxResults);
      assertEqual(65538, p.maxResultsSize);
      assertEqual(123456788113, p.maxEntrySize);
      
      cache.properties({ maxResults: 7 });
      p = cache.properties();
      
      assertEqual("on", p.mode);
      assertEqual(7, p.maxResults);
      assertEqual(65538, p.maxResultsSize);
      assertEqual(123456788113, p.maxEntrySize);
      
      cache.properties({ maxResultsSize: 1948468 });
      p = cache.properties();
      
      assertEqual("on", p.mode);
      assertEqual(7, p.maxResults);
      assertEqual(1948468, p.maxResultsSize);
      assertEqual(123456788113, p.maxEntrySize);
      
      cache.properties({ maxEntrySize: 245 });
      p = cache.properties();
      
      assertEqual("on", p.mode);
      assertEqual(7, p.maxResults);
      assertEqual(1948468, p.maxResultsSize);
      assertEqual(245, p.maxEntrySize);
    },

    testPropertiesDatabases : function () {
      try {
        db._createDatabase("UnitTestQueryCache1");
        db._createDatabase("UnitTestQueryCache2");
        
        db._useDatabase("UnitTestQueryCache1");
        cache.properties({ mode: "demand", maxResults: 98765, maxEntrySize: 4096 });
        let p = cache.properties();
        assertEqual("demand", p.mode);
        assertEqual(98765, p.maxResults);
        assertEqual(4096, p.maxEntrySize);
        
        db._useDatabase("UnitTestQueryCache2");
        cache.properties({ mode: "off", maxResults: 12345, maxEntrySize: 8192 });
        p = cache.properties();
        assertEqual("off", p.mode);
        assertEqual(12345, p.maxResults);
        assertEqual(8192, p.maxEntrySize);
        
        db._useDatabase("UnitTestQueryCache1");
        p = cache.properties();
        assertEqual("off", p.mode);
        assertEqual(12345, p.maxResults);
        assertEqual(8192, p.maxEntrySize);
        
        db._useDatabase("UnitTestQueryCache2");
        p = cache.properties();
        assertEqual("off", p.mode);
        assertEqual(12345, p.maxResults);
        assertEqual(8192, p.maxEntrySize);
      } finally {
        db._useDatabase("_system");
        try {
          db._dropDatabase("UnitTestQueryCache1");
        } catch (err) {}
        try {
          db._dropDatabase("UnitTestQueryCache2");
        } catch (err) {}
      }
    },
    
    testResultsDatabases : function () {
      const query = "FOR doc IN @@cn SORT doc.value RETURN doc.value";
      cache.properties({ mode: "demand", maxResults: 4 });

      try {
        db._createDatabase("UnitTestQueryCache1");
        db._createDatabase("UnitTestQueryCache2");
        
        db._useDatabase("UnitTestQueryCache1");
        let c = db._create(cn);
        for (let i = 0; i < 5; ++i) {
          c.insert({ value : i });
        }

        let res = db._query(query, { "@cn" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query, 0);
        assertEqual([ 0, 1, 2, 3, 4 ], res.toArray());
          
        db._useDatabase("UnitTestQueryCache2");
        c = db._create(cn);
        for (let i = 0; i < 5; ++i) {
          c.insert({ value : i * 2 });
        }
        
        res = db._query(query, { "@cn" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query, 0);
        assertEqual([ 0, 2, 4, 6, 8 ], res.toArray());
        
        db._useDatabase("UnitTestQueryCache1");
        res = db._query(query, { "@cn" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query, 1);
        assertEqual([ 0, 1, 2, 3, 4 ], res.toArray());
        
        db._useDatabase("UnitTestQueryCache2");
        res = db._query(query, { "@cn" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query, 1);
        assertEqual([ 0, 2, 4, 6, 8 ], res.toArray());
      } finally {
        db._useDatabase("_system");
        try {
          db._dropDatabase("UnitTestQueryCache1");
        } catch (err) {}
        try {
          db._dropDatabase("UnitTestQueryCache2");
        } catch (err) {}
      }
    },
    
    testNonCollectionQuery : function () {
      const query = "FOR i IN 1..10 RETURN i";
      cache.properties({ mode: "demand" });
      
      let res = db._query(query, {}, cached);
      assertFalse(res._cached);
      assertInCache(query, 0);
        
      let datasources = cache.toArray()[0].dataSources;
      assertEqual(0, datasources.length);
    },
    
    testEmptyQuery : function () {
      const query = "FOR doc IN @@c RETURN doc";
      cache.properties({ mode: "demand" });
      
      try {
        let c = db._create(cn);

        let res = db._query(query, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query, 0);
        
        res = db._query(query, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query, 1);

        let datasources = cache.toArray()[0].dataSources;
        assertEqual(1, datasources.length);
        assertEqual(cn, datasources[0]);
      } finally {
        db._drop(cn);
      }
    },
    
    testNonEmptyQuery : function () {
      const query = "FOR doc IN @@c RETURN doc";
      cache.properties({ mode: "demand" });
      
      try {
        let c = db._create(cn);
        for (let i = 0; i < 100; ++i) {
          c.insert({ value : i });
        }

        let res = db._query(query, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query, 0);
        
        res = db._query(query, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query, 1);
        
        let datasources = cache.toArray()[0].dataSources;
        assertEqual(1, datasources.length);
        assertEqual(cn, datasources[0]);
      } finally {
        db._drop(cn);
      }
    },
    
    testDifferentQueries : function () {
      const query1 = "FOR doc IN @@c RETURN doc";
      const query2 = "FOR doc IN @@c RETURN doc.value";
      cache.properties({ mode: "demand" });
      
      try {
        let c = db._create(cn);
        for (let i = 0; i < 100; ++i) {
          c.insert({ value : i });
        }

        let res = db._query(query1, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query1, 0);
        
        res = db._query(query2, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query2, 0);
        
        res = db._query(query1, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query1, 1);
        
        res = db._query(query2, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query2, 1);
        
        res = db._query(query2, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query2, 2);
        
        res = db._query(query1, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query1, 2);
      } finally {
        db._drop(cn);
      }
    },
    
    testNonCacheable : function () {
      const query = "FOR doc IN @@c SORT RAND() RETURN doc";
      cache.properties({ mode: "on" });
      
      try {
        let c = db._create(cn);

        let res = db._query(query, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertNotInCache(query);
        
        res = db._query(query, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertNotInCache(query);
      } finally {
        db._drop(cn);
      }
    },
    
    testCachingOff1 : function () {
      const query = "FOR doc IN @@c RETURN doc";
      cache.properties({ mode: "off" });
      
      try {
        let c = db._create(cn);

        let res = db._query(query, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertNotInCache(query);
        
        res = db._query(query, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertNotInCache(query);
      } finally {
        db._drop(cn);
      }
    },
    
    testCachingOff2 : function () {
      const query = "FOR doc IN @@c RETURN doc";
      cache.properties({ mode: "off" });
      
      try {
        let c = db._create(cn);

        let res = db._query(query, { "@c" : cn }, notCached);
        assertFalse(res._cached);
        assertNotInCache(query);
        
        res = db._query(query, { "@c" : cn }, notCached);
        assertFalse(res._cached);
        assertNotInCache(query);
      } finally {
        db._drop(cn);
      }
    },
    
    testCachingOn1 : function () {
      const query = "FOR doc IN @@c RETURN doc";
      cache.properties({ mode: "on" });
      
      try {
        let c = db._create(cn);

        let res = db._query(query, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query, 0);
        
        res = db._query(query, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query, 1);
      } finally {
        db._drop(cn);
      }
    },
    
    testCachingOn2 : function () {
      const query = "FOR doc IN @@c RETURN doc";
      cache.properties({ mode: "on" });
      
      try {
        let c = db._create(cn);

        let res = db._query(query, { "@c" : cn }, notCached);
        assertFalse(res._cached);
        assertNotInCache(query);
        
        res = db._query(query, { "@c" : cn }, notCached);
        assertFalse(res._cached);
        assertNotInCache(query);
      } finally {
        db._drop(cn);
      }
    },
    
    testCachingDemand1 : function () {
      const query = "FOR doc IN @@c RETURN doc";
      cache.properties({ mode: "demand" });
      
      try {
        let c = db._create(cn);

        let res = db._query(query, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query, 0);
        
        res = db._query(query, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertInCache(query, 1);
      } finally {
        db._drop(cn);
      }
    },
    
    testCachingDemand2 : function () {
      const query = "FOR doc IN @@c RETURN doc";
      cache.properties({ mode: "demand" });
      
      try {
        let c = db._create(cn);

        let res = db._query(query, { "@c" : cn }, notCached);
        assertFalse(res._cached);
        assertNotInCache(query);
        
        res = db._query(query, { "@c" : cn }, notCached);
        assertFalse(res._cached);
        assertNotInCache(query);
      } finally {
        db._drop(cn);
      }
    },
    
    testMaxResults1 : function () {
      cache.properties({ mode: "demand", maxResults: 10 });
      
      try {
        let c = db._create(cn);
        for (let i = 0; i < 100; ++i) {
          c.insert({ value : i });
        }

        for (let i = 0; i < 100; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          let res = db._query(query, { "@c" : cn }, cached);
          assertFalse(res._cached);
          assertInCache(query, 0);
        }

        assertEqual(10, cache.toArray().length);

        // we will not see the same queries again        
        for (let i = 0; i < 100; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          let res = db._query(query, { "@c" : cn }, cached);
          assertFalse(res._cached);
          assertInCache(query, 0);
        }
        
        assertEqual(10, cache.toArray().length);
      } finally {
        db._drop(cn);
      }
    },
    
    testMaxResults2 : function () {
      cache.properties({ mode: "demand", maxResults: 10 });
      
      try {
        let c = db._create(cn);
        for (let i = 0; i < 10; ++i) {
          c.insert({ value : i });
        }

        for (let i = 0; i < 10; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          let res = db._query(query, { "@c" : cn }, cached);
          assertFalse(res._cached);
          assertInCache(query, 0);
        }

        assertEqual(10, cache.toArray().length);

        // we will now see the same queries again        
        for (let i = 0; i < 10; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          let res = db._query(query, { "@c" : cn }, cached);
          assertTrue(res._cached);
          assertInCache(query, 1);
        }
        
        assertEqual(10, cache.toArray().length);
        
        // we will now see the same queries again        
        for (let i = 0; i < 10; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          let res = db._query(query, { "@c" : cn }, cached);
          assertTrue(res._cached);
          assertInCache(query, 2);
        }
      } finally {
        db._drop(cn);
      }
    },
    
    testMaxResults3 : function () {
      const query1 = "FOR doc IN @@c RETURN doc";
      const query2 = "FOR doc IN @@c RETURN doc.value";
      cache.properties({ mode: "demand", maxResults: 1 });
      
      try {
        let c = db._create(cn);
        for (let i = 0; i < 100; ++i) {
          c.insert({ value : i });
        }

        let res = db._query(query1, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertInCache(query1, 0);
        
        res = db._query(query2, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertNotInCache(query1);
        assertInCache(query2, 0);
        
        res = db._query(query1, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertNotInCache(query2);
        assertInCache(query1, 0);
        
        res = db._query(query2, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertNotInCache(query1);
        assertInCache(query2, 0);
        
        res = db._query(query2, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertNotInCache(query1);
        assertInCache(query2, 1);
        
        res = db._query(query2, { "@c" : cn }, cached);
        assertTrue(res._cached);
        assertNotInCache(query1);
        assertInCache(query2, 2);
        
        res = db._query(query1, { "@c" : cn }, cached);
        assertFalse(res._cached);
        assertNotInCache(query2);
        assertInCache(query1, 0);
      } finally {
        db._drop(cn);
      }
    },
    
    testMaxResultsShrink : function () {
      cache.properties({ mode: "demand", maxResults: 10 });
      
      try {
        let c = db._create(cn);
        for (let i = 0; i < 10; ++i) {
          c.insert({ value : i });
        }

        for (let i = 0; i < 10; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          let res = db._query(query, { "@c" : cn }, cached);
          assertFalse(res._cached);
          assertInCache(query, 0);
        }

        assertEqual(10, cache.toArray().length);
      
        cache.properties({ mode: "demand", maxResults: 5 });

        assertEqual(5, cache.toArray().length);
        
        for (let i = 0; i < 10; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          if (i < 5) {
            assertNotInCache(query);
          } else {
            assertInCache(query, 0);
          }
        }
      } finally {
        db._drop(cn);
      }
    },
    
    testMaxResultsSize1 : function () {
      const query = "FOR i IN 1..@value RETURN i";
      cache.properties({ mode: "demand", maxResults: 10, maxResultsSize: 8192 });
      
      let q = query + " // 1000";
      let res = db._query(q, { value: 1000 }, cached);
      assertFalse(res._cached);
      assertInCache(q, 0);
      
      q = query + " // 1001";
      res = db._query(q, { value: 1001 }, cached);
      assertFalse(res._cached);
      assertInCache(q, 0);
      
      q = query + " // 5000";
      res = db._query(q, { value: 5000 }, cached);
      assertFalse(res._cached);
      assertNotInCache(q);
      
      q = query + " // 1000";
      res = db._query(q, { value: 1000 }, cached);
      assertTrue(res._cached);
      assertInCache(q, 1);

      q = query + " // 1001";
      res = db._query(q, { value: 1001 }, cached);
      assertTrue(res._cached);
      assertInCache(q, 1);
    },
    
    testMaxResultsSize2 : function () {
      const query = "FOR i IN 1..@value RETURN i";
      cache.properties({ mode: "demand", maxResults: 10 });
      
      let res = db._query(query, { value: 5000 }, cached);
      assertFalse(res._cached);
      assertInCache(query, 0);
      
      assertEqual(1, cache.toArray().length);

      cache.properties({ mode: "demand", maxResults: 10, maxResultsSize: 8192 });

      res = db._query(query, { value: 5000 }, cached);
      assertFalse(res._cached);
      assertNotInCache(query);

      assertEqual(0, cache.toArray().length);
    },
    
    testMaxEntrySize1 : function () {
      cache.properties({ mode: "demand", maxResults: 10, maxEntrySize: 2048 });
      
      try {
        let c = db._create(cn);
        for (let i = 0; i < 10; ++i) {
          c.insert({ value : i });
        }

        for (let i = 0; i < 10; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          let res = db._query(query, { "@c" : cn }, cached);
          assertFalse(res._cached);
          assertInCache(query, 0);
        }

        assertEqual(10, cache.toArray().length);

        // we will see the same queries again        
        for (let i = 0; i < 10; ++i) {
          let query = "FOR doc IN @@c FILTER doc.value == " + i + " RETURN doc";
          let res = db._query(query, { "@c" : cn }, cached);
          assertTrue(res._cached);
          assertInCache(query, 1);
        }
        
        assertEqual(10, cache.toArray().length);
      } finally {
        db._drop(cn);
      }
    },
    
    testMaxEntrySize2 : function () {
      cache.properties({ mode: "demand", maxResults: 10, maxEntrySize: 2048 });
      
      let query = "FOR i IN 1..1000 RETURN i";
      let res = db._query(query, {}, cached);
      assertFalse(res._cached);
      assertNotInCache(query);

      assertEqual(0, cache.toArray().length);
      
      res = db._query(query, {}, cached);
      assertFalse(res._cached);
      assertNotInCache(query);

      assertEqual(0, cache.toArray().length);
    },
    
    testMaxEntrySize3 : function () {
      cache.properties({ mode: "demand", maxResults: 10, maxEntrySize: 16 * 1024 * 1024 });
      
      let query = "FOR i IN 1..1000 RETURN i";
      let res = db._query(query, {}, cached);
      assertFalse(res._cached);
      assertInCache(query, 0);

      assertEqual(1, cache.toArray().length);
      
      res = db._query(query, {}, cached);
      assertTrue(res._cached);
      assertInCache(query, 1);

      assertEqual(1, cache.toArray().length);
      
      cache.properties({ mode: "demand", maxResults: 10, maxEntrySize: 1024 });
      assertNotInCache(query);

      assertEqual(0, cache.toArray().length);
    },
    
    testMaxEntrySize4 : function () {
      const query = "FOR i IN 1..@value RETURN i";
      cache.properties({ mode: "demand", maxEntrySize: 8192 });
      
      let q = query + " // 10000";
      let res = db._query(q, { value: 10000 }, cached);
      assertFalse(res._cached);
      assertNotInCache(q);

      assertEqual(0, cache.toArray().length);
      
      q = query + " // 8000";
      res = db._query(q, { value: 8000 }, cached);
      assertFalse(res._cached);
      assertNotInCache(q);

      assertEqual(0, cache.toArray().length);
      
      q = query + " // 100";
      res = db._query(q, { value: 100 }, cached);
      assertFalse(res._cached);
      assertInCache(q, 0);

      assertEqual(1, cache.toArray().length);
      
      q = query + " // 8000";
      res = db._query(q, { value: 8000 }, cached);
      assertFalse(res._cached);
      assertNotInCache(q);

      assertEqual(1, cache.toArray().length);
      
      q = query + " // 100";
      res = db._query(q, { value: 100 }, cached);
      assertTrue(res._cached);
      assertInCache(q, 1);

      assertEqual(1, cache.toArray().length);
    },
    
    testToArray : function () {
      const query = "FOR i IN 1..@value RETURN i";
      cache.properties({ mode: "demand" });
      
      let q = query + " // 10000";
      let res = db._query(q, { value: 10000 }, cached);
      assertInCache(q, 0);

      assertEqual(1, cache.toArray().length);
      
      q = query + " // 1000";
      res = db._query(q, { value: 1000 }, cached);
      assertInCache(q, 0);

      assertEqual(2, cache.toArray().length);
      
      q = query + " // 100";
      res = db._query(q, { value: 100 }, cached);
      assertInCache(q, 0);

      assertEqual(3, cache.toArray().length);

      cache.toArray().forEach(function(e) {
        assertTrue(e.hasOwnProperty("hash"));
        assertTrue(typeof e.hash === "string");
        assertTrue(e.hasOwnProperty("query"));
        assertTrue(typeof e.query === "string");
        assertTrue(e.hasOwnProperty("size"));
        assertTrue(typeof e.size === "number");
        assertTrue(e.hasOwnProperty("results"));
        assertTrue(typeof e.results === "number");
        assertTrue(e.hasOwnProperty("runTime"));
        assertTrue(typeof e.runTime === "number");
        assertTrue(e.hasOwnProperty("started"));
        assertTrue(typeof e.started === "string");
      });

      cache.clear();
      assertEqual(0, cache.toArray().length);
    },
    
    testResults : function () {
      const query = "FOR i IN 1..@value RETURN i";
      cache.properties({ mode: "demand" });
      
      let q = query + " // 10000";
      let res = db._query(q, { value: 10000 }, cached);
      assertInCache(q, 0);
      assertEqual(1, cache.toArray().length);
      assertEqual(10000, cache.toArray()[0].results);
     
      cache.clear();
       
      q = query + " // 1000";
      res = db._query(q, { value: 1000 }, cached);
      assertInCache(q, 0);
      assertEqual(1, cache.toArray().length);
      assertEqual(1000, cache.toArray()[0].results);

      cache.clear();
       
      q = query + " // 100";
      res = db._query(q, { value: 100 }, cached);
      assertInCache(q, 0);
      assertEqual(1, cache.toArray().length);
      assertEqual(100, cache.toArray()[0].results);
    },

  };
}

jsunity.run(AqlQueryCacheSuite);

return jsunity.done();
