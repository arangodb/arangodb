/* jshint globalstrict:false, strict:false */
/* global assertEqual, assertTrue, assertFalse, assertNotNull */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the document interface
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2018, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var arangodb = require('@arangodb');
var db = arangodb.db;

function EdgeIndexCacheSuite () {
  let vn = 'UnitTestsCollectionVertex';
  let en = 'UnitTestsCollectionEdge';
  let vertex = null;
  let edge = null;

  return {

    setUp: function () {
      db._drop(en);
      edge = db._createEdgeCollection(en);

      db._drop(vn);
      vertex = db._create(vn);
    },

    tearDown: function () {
      db._drop(vn);
      db._drop(en);
    },
    
    testEdgeCacheRemoveNonEmpty: function () {
      vertex.insert({ _key: "test1" });
      vertex.insert({ _key: "test2" });
      edge.insert({ _from: vn + "/test1", _to: vn + "/test2", _key: "test" });

      assertEqual(2, vertex.count());
      assertEqual(1, edge.count());

      let q = "WITH " + vn + " FOR v IN OUTBOUND '" + vn + "/test1' " + en + " RETURN v";

      // execute to fill cache
      assertEqual(1, db._query(q).toArray().length);
      // execute once more so result is going to be served from cache
      assertEqual(1, db._query(q).toArray().length);

      edge.remove("test");
      assertEqual(2, vertex.count());
      assertEqual(0, edge.count());
      
      // execute to fill cache
      assertEqual(0, db._query(q).toArray().length);
      // execute once more so result is going to be served from cache
      assertEqual(0, db._query(q).toArray().length);
    },
    

    testEdgeCacheTruncateEmpty: function () {
      edge.insert({ _from: vn + "/test1", _to: vn + "/test2" });

      assertEqual(0, vertex.count());
      assertEqual(1, edge.count());

      let q = "WITH " + vn + " FOR v IN OUTBOUND '" + vn + "/test1' " + en + " RETURN v";

      // execute to fill cache
      let result = db._query(q).toArray();
      assertEqual(1, result.length);
      assertEqual([ null ], result);
      // execute once more so result is going to be served from cache
      result = db._query(q).toArray();
      assertEqual(1, result.length);
      assertEqual([ null ], result);

      edge.truncate({ compact: false });
      assertEqual(0, vertex.count());
      assertEqual(0, edge.count());
      
      // execute to fill cache
      result = db._query(q).toArray();
      assertEqual(0, result.length);
      assertEqual([ ], result);
      // execute once more so result is going to be served from cache
      result = db._query(q).toArray();
      assertEqual(0, result.length);
      assertEqual([ ], result);
    },
    
    testEdgeCacheTruncateNonEmpty: function () {
      vertex.insert({ _key: "test1" });
      vertex.insert({ _key: "test2" });
      edge.insert({ _from: vn + "/test1", _to: vn + "/test2" });

      assertEqual(2, vertex.count());
      assertEqual(1, edge.count());

      let q = "WITH " + vn + " FOR v IN OUTBOUND '" + vn + "/test1' " + en + " RETURN v";

      // execute to fill cache
      assertEqual(1, db._query(q).toArray().length);
      // execute once more so result is going to be served from cache
      assertEqual(1, db._query(q).toArray().length);

      edge.truncate({ compact: false });
      assertEqual(2, vertex.count());
      assertEqual(0, edge.count());
      
      // execute to fill cache
      assertEqual(0, db._query(q).toArray().length);
      // execute once more so result is going to be served from cache
      assertEqual(0, db._query(q).toArray().length);
    },
    
    testEdgeCacheTruncateMultiple: function () {
      vertex.insert({ _key: "test1" });
      vertex.insert({ _key: "test2" });
      vertex.insert({ _key: "test3" });
      edge.insert({ _from: vn + "/test1", _to: vn + "/test2" });
      edge.insert({ _from: vn + "/test1", _to: vn + "/test3" });

      assertEqual(3, vertex.count());
      assertEqual(2, edge.count());

      let q = "WITH " + vn + " FOR v IN OUTBOUND '" + vn + "/test1' " + en + " RETURN v";

      // execute to fill cache
      assertEqual(2, db._query(q).toArray().length);
      // execute once more so result is going to be served from cache
      assertEqual(2, db._query(q).toArray().length);

      edge.truncate({ compact: false });
      assertEqual(3, vertex.count());
      assertEqual(0, edge.count());
      
      // execute to fill cache
      assertEqual(0, db._query(q).toArray().length);
      // execute once more so result is going to be served from cache
      assertEqual(0, db._query(q).toArray().length);
    },

    testEdgeCacheTruncateMany: function () {
      let i;
      for (i = 0; i < 100; ++i) {
        vertex.insert({ _key: "test" + i });
      }
      for (i = 0; i < 100; ++i) {
        edge.insert({ _from: vn + "/test" + i, _to: vn + "/test" + (i + 1) });
      }

      assertEqual(100, vertex.count());
      assertEqual(100, edge.count());

      let q = "WITH " + vn + " FOR v IN 1..100 OUTBOUND '" + vn + "/test1' " + en + " RETURN v";

      // execute to fill cache
      let result = db._query(q).toArray();
      assertEqual(99, db._query(q).toArray().length);
      // execute once more so result is going to be served from cache
      assertEqual(99, db._query(q).toArray().length);

      edge.truncate({ compact: false });
      assertEqual(100, vertex.count());
      assertEqual(0, edge.count());
      
      // execute to fill cache
      assertEqual(0, db._query(q).toArray().length);
      // execute once more so result is going to be served from cache
      assertEqual(0, db._query(q).toArray().length);
    }

  };
}

jsunity.run(EdgeIndexCacheSuite);

return jsunity.done();
