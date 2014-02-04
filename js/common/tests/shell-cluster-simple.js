/*jslint indent: 2, nomen: true, maxlen: 80 */
/*global require, assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple queries in a cluster
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("org/arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;
var _ = require("underscore");

var createCollection = function (properties) {
  try {
    db._drop("UnitTestsClusterCrud");
  }
  catch (err) {
  }

  if (properties === undefined) {
    properties = { };
  }

  return db._create("UnitTestsClusterCrud", properties);
};


// -----------------------------------------------------------------------------
// --SECTION--                                                Simple query tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudSimpleSuite () {

  var queryToArray = function (q, n, cb) {
    var result = q.toArray();
    assertEqual(n, result.length);

    if (n > 0 && cb !== undefined) {
      cb(result[0]);
    }
  };
  
  var queryToCursor = function (q, n, cb) {
    var result = q, count = 0;

    while (result.hasNext()) {
      if (cb !== undefined) {
        cb(result.next());
      }
      else {
        result.next();
      }
      count++;
    }
    assertEqual(n, count);
  };

  var checkQuery = function (q, n, cb) {
    qc = _.clone(q);
    queryToArray(qc, n, cb);
    qc = _.clone(q);
    queryToCursor(qc, n, cb);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for all
////////////////////////////////////////////////////////////////////////////////

  var executeAll = function (c) {
    var i, n = 6000;
    for (i = 0; i < n; ++i) {
      c.save({ _key: "test" + i, value1 : i, value2 : "test" + i });
    }
    
    checkQuery(c.all().limit(20), 20, function (r0) {
      assertTrue(r0.hasOwnProperty("_id"));
      assertTrue(r0.hasOwnProperty("_key"));
      assertTrue(r0.hasOwnProperty("_rev"));
      assertTrue(r0.hasOwnProperty("value1"));
      assertTrue(r0.hasOwnProperty("value2"));
      assertEqual(r0._key, r0.value2);
    });

    checkQuery(c.all().skip(5981).limit(20), 19, function (r0) {
      assertTrue(r0.hasOwnProperty("_id"));
      assertTrue(r0.hasOwnProperty("_key"));
      assertTrue(r0.hasOwnProperty("_rev"));
      assertTrue(r0.hasOwnProperty("value1"));
      assertTrue(r0.hasOwnProperty("value2"));
      assertEqual(r0._key, r0.value2);
    });
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for byExample
////////////////////////////////////////////////////////////////////////////////
    
  var executeByExample = function (c) {
    var i, n = 6000;
    for (i = 0; i < n; ++i) {
      c.save({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
   
    checkQuery(c.byExample({ }).limit(20), 20, function (r0) {
      assertTrue(r0.hasOwnProperty("_id"));
      assertTrue(r0.hasOwnProperty("_key"));
      assertTrue(r0.hasOwnProperty("_rev"));
      assertTrue(r0.hasOwnProperty("value1"));
      assertTrue(r0.hasOwnProperty("value2"));
      assertTrue(r0.hasOwnProperty("value3"));
      assertEqual(r0._key, r0.value2);
      assertEqual(1, r0.value3);
    });
    
    checkQuery(c.byExample({ value3 : 1 }).limit(20), 20, function (r0) {
      assertTrue(r0.hasOwnProperty("_id"));
      assertTrue(r0.hasOwnProperty("_key"));
      assertTrue(r0.hasOwnProperty("_rev"));
      assertTrue(r0.hasOwnProperty("value1"));
      assertTrue(r0.hasOwnProperty("value2"));
      assertTrue(r0.hasOwnProperty("value3"));
      assertEqual(r0._key, r0.value2);
      assertEqual(1, r0.value3);
    });
   
    checkQuery(c.byExample({ value1 : 5 }), n / 10, function (r0) {
      assertTrue(r0.hasOwnProperty("_id"));
      assertTrue(r0.hasOwnProperty("_key"));
      assertTrue(r0.hasOwnProperty("_rev"));
      assertTrue(r0.hasOwnProperty("value1"));
      assertTrue(r0.hasOwnProperty("value2"));
      assertTrue(r0.hasOwnProperty("value3"));
      assertEqual(5, r0.value1);
      assertEqual(r0._key, r0.value2);
      assertEqual(1, r0.value3);
    });
    
    checkQuery(c.byExample({ value1 : 5 }).limit(4), 4, function (r0) {
      assertTrue(r0.hasOwnProperty("_id"));
      assertTrue(r0.hasOwnProperty("_key"));
      assertTrue(r0.hasOwnProperty("_rev"));
      assertTrue(r0.hasOwnProperty("value1"));
      assertTrue(r0.hasOwnProperty("value2"));
      assertTrue(r0.hasOwnProperty("value3"));
      assertEqual(5, r0.value1);
      assertEqual(r0._key, r0.value2);
      assertEqual(1, r0.value3);
    });
    
    checkQuery(c.byExample({ value2 : "test999" }), 1, function (r0) {
      assertTrue(r0.hasOwnProperty("_id"));
      assertTrue(r0.hasOwnProperty("_key"));
      assertTrue(r0.hasOwnProperty("_rev"));
      assertTrue(r0.hasOwnProperty("value1"));
      assertTrue(r0.hasOwnProperty("value2"));
      assertTrue(r0.hasOwnProperty("value3"));
      assertEqual(9, r0.value1);
      assertEqual("test999", r0.value2);
      assertEqual(1, r0.value3);
    });
    
    checkQuery(c.byExample({ value1 : 9, value2 : "test999", value3 : 1 }), 1);
    checkQuery(c.byExample({ _key : "test999" }), 1);
    checkQuery(c.byExample({ value2: "test999" }).skip(1), 0);
    checkQuery(c.byExample({ value1 : 9, value2 : "test999", value3 : 1, _key : "test999" }), 1);
    checkQuery(c.byExample({ value1 : 9, value2 : "test999", value3 : 1, _key : "test999" }).skip(1), 0);
    checkQuery(c.byExample({ value2: "foxx" }), 0);
    checkQuery(c.byExample({ clobber: "clubber" }), 0);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for firstExample
////////////////////////////////////////////////////////////////////////////////
    
  var executeFirstExample = function (c) {
    var i, n = 1000;
    for (i = 0; i < n; ++i) {
      c.save({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
   
    var r0 = c.firstExample({ _key : "test35" });
    assertTrue(r0.hasOwnProperty("_id"));
    assertTrue(r0.hasOwnProperty("_key"));
    assertTrue(r0.hasOwnProperty("_rev"));
    assertTrue(r0.hasOwnProperty("value1"));
    assertTrue(r0.hasOwnProperty("value2"));
    assertTrue(r0.hasOwnProperty("value3"));
    assertEqual("test35", r0._key);
    assertEqual(5, r0.value1);
    assertEqual("test35", r0.value2);
    assertEqual(1, r0.value3);
    
    r0 = c.firstExample({ _key : "test35", value1 : 5, value2 : "test35", value3 : 1 });
    assertTrue(r0.hasOwnProperty("_id"));
    assertTrue(r0.hasOwnProperty("_key"));
    assertTrue(r0.hasOwnProperty("_rev"));
    assertTrue(r0.hasOwnProperty("value1"));
    assertTrue(r0.hasOwnProperty("value2"));
    assertTrue(r0.hasOwnProperty("value3"));
    assertEqual("test35", r0._key);
    assertEqual(5, r0.value1);
    assertEqual("test35", r0.value2);
    assertEqual(1, r0.value3);

    r0 = c.firstExample({ _key : "test35", value1: 6 });
    assertNull(r0);
    
    r0 = c.firstExample({ _key : "test2000" });
    assertNull(r0);
    
    r0 = c.firstExample({ value2 : "test2000" });
    assertNull(r0);
    
    r0 = c.firstExample({ _key : "test2000", foobar : "baz" })
    assertNull(r0);
    
    r0 = c.firstExample({ foobar : "baz" })
    assertNull(r0);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for count
////////////////////////////////////////////////////////////////////////////////
    
  var executeCount = function (c) {
    var i, n = 1000;
    for (i = 0; i < n; ++i) {
      c.save({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }

    assertEqual(n, c.count());
   
    c.removeByExample({ value1 : 7 });
    assertEqual(n - (n / 10), c.count());
    
    c.removeByExample({ value1 : 6 });
    assertEqual(n - 2 * (n / 10), c.count());
    
    // this shouldn't remove anything
    c.removeByExample({ value1 : 6 });
    assertEqual(n - 2 * (n / 10), c.count());

    c.removeByExample({ value3 : 1 });
    assertEqual(0, c.count());

    var doc = c.save({ a : 1 });
    assertEqual(1, c.count());
    c.remove(doc);
    assertEqual(0, c.count());
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        db._drop("UnitTestsClusterCrud");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test all, one shard
////////////////////////////////////////////////////////////////////////////////

    testAllOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeAll(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test all, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testAllMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeAll(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test byExample, one shard
////////////////////////////////////////////////////////////////////////////////

    testByExampleOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeByExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test all, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testByExampleMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeByExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test firstExample, one shard
////////////////////////////////////////////////////////////////////////////////

    testFirstExampleOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeFirstExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test all, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testFirstExampleMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeFirstExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count, one shard
////////////////////////////////////////////////////////////////////////////////

    testCountOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeCount(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testCountMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeCount(c);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ClusterCrudSimpleSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
