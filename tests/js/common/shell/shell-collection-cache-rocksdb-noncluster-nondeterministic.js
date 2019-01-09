/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertEqual, assertTypeOf, assertNotEqual, fail, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;

function CollectionCacheSuite () {
  const cn = "UnitTestsClusterCache";
  return {
    
    setUp : function () {
      try {
        db._drop(cn);
      } catch (err) {
      }
    },

    tearDown : function () {
      try {
        db._drop(cn);
      } catch (err) {
      }
    },
    
    testCollectionCache : function () {
      let c = db._create(cn, {cacheEnabled:true});
      let p = c.properties();
      assertTrue(p.cacheEnabled, p);

      let f = c.figures();
      assertTrue(f.cacheInUse, f);
      assertTrue(f.cacheSize > 0);
      assertEqual(f.cacheUsage, 0);
      assertEqual(f.cacheLifeTimeHitRate, 0);

      let idxs = c.getIndexes(true);
      idxs.forEach(function(idx) {
        if (idx.type === 'primary') {
          assertTrue(idx.figures.cacheInUse, idx);
          assertEqual(idx.figures.cacheUsage, 0);
        }
      });
    },

    testCollectionCacheModifyProperties : function () {
      // create collection without cache
      let c = db._create(cn, {cacheEnabled:false});
      let p = c.properties();
      assertFalse(p.cacheEnabled, p);
      let f = c.figures();
      assertFalse(f.cacheInUse, f);
      let idxs = c.getIndexes(true);
      idxs.forEach(function(idx) {
        if (idx.type === 'primary') {
          assertFalse(idx.figures.cacheInUse);
          assertEqual(idx.figures.cacheUsage, 0);
        }
      });

      // enable caches
      c.properties({cacheEnabled:true});
      p = c.properties();
      assertTrue(p.cacheEnabled, p);
      f = c.figures();
      assertTrue(f.cacheInUse, f);
      assertEqual(f.cacheLifeTimeHitRate, 0);
      idxs = c.getIndexes(true);
      idxs.forEach(function(idx) {
        if (idx.type === 'primary') {
          assertTrue(idx.figures.cacheInUse);
          assertEqual(idx.figures.cacheUsage, 0);
        }
      });

      // disable caches again
      c.properties({cacheEnabled:false});
      p = c.properties();
      assertFalse(p.cacheEnabled, p);
      f = c.figures();
      assertFalse(f.cacheInUse, f);
      idxs = c.getIndexes(true);
      idxs.forEach(function(idx) {
        if (idx.type === 'primary') {
          assertFalse(idx.figures.cacheInUse);
          assertEqual(idx.figures.cacheUsage, 0);
        }
      });
    },

    testCollectionCacheBehaviour : function () {
      let c = db._create(cn, {cacheEnabled:true});
      let p = c.properties();
      assertTrue(p.cacheEnabled, p);

      for(let i = 0; i < 10000; i++) {
        c.insert({_key:String(i), value : i});
      }

      let f = c.figures();
      assertTrue(f.cacheInUse, f);
      assertTrue(f.cacheSize > 0);
      assertEqual(f.cacheUsage, 0);
      assertEqual(f.cacheLifeTimeHitRate, 0);

      let idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idx.figures.cacheSize > 0);
          assertEqual(idx.figures.cacheUsage, 0);
          assertEqual(idx.figures.cacheLifeTimeHitRate, 0);
        }
      });

      // hit 25% 
      for(let i = 0; i < 2500; i++) {
        let doc = c.document(String(i));
        assertTrue(doc !== undefined);
      }

      f = c.figures();
      assertTrue(f.cacheInUse, f);
      assertTrue(f.cacheSize > 0);
      assertTrue(f.cacheUsage > 0);
      assertEqual(f.cacheLifeTimeHitRate, 0);

      idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idx.figures.cacheSize > 0);
          assertTrue(idx.figures.cacheUsage > 0);
          assertEqual(idx.figures.cacheLifeTimeHitRate, 0);
        }
      });

      // hit same 25% 
      for(let i = 0; i < 2500; i++) {
        let doc = c.document(String(i));
        assertTrue(doc !== undefined);
      }

      // no new documents were hit
      let f2 = c.figures();
      assertTrue(f2.cacheInUse);
      // anyway, the cache usage may be different, because there is
      // no guarantee any document makes it into the cache
      assertEqual(f.cacheSize, f2.cacheSize);
      /*
      assertTrue(Math.abs(f.cacheUsage - f2.cacheUsage) < 2048, 
                 Math.abs(f.cacheUsage - f2.cacheUsage));
      // first no hits, second time all hits ~= 50% hit-rate
      assertTrue(Math.abs(f2.cacheLifeTimeHitRate - 50) < 11, f2.cacheLifeTimeHitRate);
      */
      let idxs2 = c.getIndexes(true);
      idxs2.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertEqual(idx.figures.cacheSize, idxs[i].figures.cacheSize);
          assertTrue(Math.abs(idx.figures.cacheUsage - idxs[i].figures.cacheUsage) < 2048, 
                     Math.abs(idx.figures.cacheUsage - idxs[i].figures.cacheUsage));
          assertTrue(Math.abs(idx.figures.cacheLifeTimeHitRate - 50) < 11);
        }
      });

      // hit different 25% 
      for(let i = 5000; i < 7500; i++) {
        let doc = c.document(String(i));
        assertTrue(doc !== undefined);
      }

      let f3 = c.figures();
      assertTrue(f3.cacheInUse);
      assertTrue(f2.cacheSize <= f3.cacheSize);
      assertTrue(f2.cacheUsage < f3.cacheUsage);
      assertTrue(f3.cacheLifeTimeHitRate < f2.cacheLifeTimeHitRate);

      idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idxs2[i].figures.cacheSize < idx.figures.cacheSize);
          assertTrue(idxs2[i].figures.cacheUsage < idx.figures.cacheUsage);
          assertTrue(idx.figures.cacheLifeTimeHitRate < idxs2[i].figures.cacheLifeTimeHitRate);
        }
      });

      c.unload(); // should destroy cache
      f = c.figures(); // will load collection
      assertTrue(f.cacheSize > 0);
      assertEqual(f.cacheUsage, 0);
      assertEqual(f.cacheLifeTimeHitRate, 0);

      idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idx.figures.cacheSize > 0);
          assertEqual(idx.figures.cacheUsage, 0);
          assertEqual(idx.figures.cacheLifeTimeHitRate, 0);
        }
      });
    }
  };
}

jsunity.run(CollectionCacheSuite);

return jsunity.done();
