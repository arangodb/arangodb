/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue, assertNull */

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
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;
var _ = require("lodash");

var createCollection = function (properties) {
  'use strict';
  try {
    db._drop("UnitTestsClusterCrud");
  } catch (err) {
  }

  if (properties === undefined) {
    properties = { };
  }

  return db._create("UnitTestsClusterCrud", properties);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudSimpleSuite () {
  'use strict';

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
      } else {
        result.next();
      }
      count++;
    }
    assertEqual(n, count);
  };

  var checkQuery = function (q, n, cb) {
    q = _.toPlainObject(q); // Clone inherited methods too
    var qc = _.clone(q);
    queryToArray(qc, n, cb);
    qc = _.clone(q);
    queryToCursor(qc, n, cb);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for all
////////////////////////////////////////////////////////////////////////////////

  var executeAll = function (c) {
    const n = 6000;
    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : i, value2 : "test" + i });
    }
    c.insert(docs);

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
    const n = 6000;
    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
    c.insert(docs);

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
    const n = 1000;
    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
    c.insert(docs);

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

    r0 = c.firstExample({ _key : "test2000", foobar : "baz" });
    assertNull(r0);

    r0 = c.firstExample({ foobar : "baz" });
    assertNull(r0);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for count
////////////////////////////////////////////////////////////////////////////////

  var executeCount = function (c) {
    const n = 1000;
    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
    c.insert(docs);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for any
////////////////////////////////////////////////////////////////////////////////

  var executeAny = function (c) {
    const n = 1000;

    assertNull(c.any());

    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
    c.insert(docs);

    var doc = c.any();
    assertTrue(doc.hasOwnProperty("_id"));
    assertTrue(doc.hasOwnProperty("_key"));
    assertTrue(doc.hasOwnProperty("_rev"));
    assertTrue(doc.hasOwnProperty("value1"));
    assertTrue(doc.hasOwnProperty("value2"));
    assertTrue(doc.hasOwnProperty("value3"));
    assertEqual(doc._key, doc.value2);
    assertEqual(1, doc.value3);

    c.truncate({ compact: false });
    assertNull(c.any());
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for truncate
////////////////////////////////////////////////////////////////////////////////

  var executeTruncate = function (c) {
    const n = 1000;

    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
    c.insert(docs);

    assertEqual(n, c.count());

    c.truncate({ compact: false });
    assertEqual(0, c.count());

    c.truncate({ compact: false });
    assertEqual(0, c.count());

    c.save({ _key : "foo" });
    assertEqual(1, c.count());

    c.truncate({ compact: false });
    assertEqual(0, c.count());
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for removeByExample
////////////////////////////////////////////////////////////////////////////////

  var executeRemoveByExample = function (c) {
    const n = 1000;

    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
    c.insert(docs);

    assertEqual(n, c.count());

    assertEqual(0, c.removeByExample({ value1 : 11 }));
    assertEqual(n, c.count());

    assertEqual(0, c.removeByExample({ value1 : 4, value2 : "test37" }));
    assertEqual(n, c.count());

    assertEqual(0, c.removeByExample({ foobar : "baz" }));
    assertEqual(n, c.count());

    assertEqual(n / 10, c.removeByExample({ value1 : 1 }));
    assertEqual(n - (n / 10), c.count());

    assertEqual(n / 10, c.removeByExample({ value1 : 2 }));
    assertEqual(n - 2 * (n / 10), c.count());

    assertEqual(1, c.removeByExample({ _key : "test44" }));
    assertEqual(n - 2 * (n / 10) - 1, c.count());

    assertEqual(0, c.removeByExample({ _key : "test44" }));

    assertEqual(n - 2 * (n / 10) - 1, c.removeByExample({ value3 : 1 }));
    assertEqual(0, c.count());

    assertEqual(0, c.removeByExample({ value3 : 1 }));
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replaceByExample
////////////////////////////////////////////////////////////////////////////////

  var executeReplaceByExample = function (c) {
    const n = 1000;

    assertEqual(0, c.replaceByExample({ value3 : 1 }, { value9 : 17 }));

    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
    c.insert(docs);

    assertEqual(n, c.count());

    assertEqual(0, c.replaceByExample({ value1 : 11 }, { value1 : 17 }));
    assertEqual(n, c.count());

    assertEqual(0, c.replaceByExample({ value1 : 4, value2 : "test37" }, { value2 : "foxx" }));
    assertEqual(n, c.count());

    assertEqual(0, c.replaceByExample({ foobar : "baz" }, { boom : "bar" }));
    assertEqual(n, c.count());

    assertEqual(n / 10, c.replaceByExample({ value1 : 1 }, { value1 : 4 }));
    assertEqual(0, c.replaceByExample({ value1 : 1 }, { value1 : 4 }));
    assertEqual(n, c.count());

    assertEqual(n / 10, c.replaceByExample({ value1 : 2 }, { value1 : 5 }));
    assertEqual(0, c.replaceByExample({ value1 : 2 }, { value1 : 5 }));
    assertEqual(n, c.count());

    assertEqual(1, c.replaceByExample({ _key : "test44" }, { value3 : 2 }));
    assertEqual(1, c.replaceByExample({ _key : "test44" }, { value3 : 4 }));

    assertEqual(n, c.replaceByExample({ }, { value3 : 99, value2 : 1 }));
    assertEqual(0, c.replaceByExample({ value3 : 99, value2 : 2 }, { }));
    assertEqual(n, c.replaceByExample({ value3 : 99, value2 : 1 }, { value2 : 2, value3 : "test" }));
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replaceByExample
////////////////////////////////////////////////////////////////////////////////

  var executeReplaceByExampleShardKeys = function (c) {
    const n = 1000;

    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ a : (i % 10), b : (i % 100) });
    }
    c.insert(docs);

    assertEqual(0, c.replaceByExample({ a : 11 }, { a : 11, b : 13, c : 1 }));

    try {
      c.replaceByExample({ a : 1, b : 1 }, { a : 12542, b : 13239, c : 1 });
      fail();
    } catch (err1) {
      if (ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code !== err1.errorNum) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err1.errorNum);
      }
    }

    try {
      c.replaceByExample({ a : 2 }, { a : 2, b : 13, c : 1 });
      fail();
    } catch (err2) {
      if (ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code !== err2.errorNum) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
      }
    }

    try {
      c.replaceByExample({ a : 2 }, { a : 2, c : 1 });
      fail();
    } catch (err3) {
      if (ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code !== err3.errorNum) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err3.errorNum);
      }
    }

    assertEqual(n / 100, c.replaceByExample({ a : 7, b : 17 }, { a : 7, b : 17, c : 12 }));
    assertEqual(n / 100, c.replaceByExample({ a : 7, b : 17, c : 12 }, { a : 7, b : 17, d : 9 }));
    assertEqual(0, c.replaceByExample({ a : 7, b : 17, c : 12 }, { }));
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for updateByExample
////////////////////////////////////////////////////////////////////////////////

  var executeUpdateByExample = function (c) {
    const n = 1000;

    assertEqual(0, c.updateByExample({ value3 : 1 }, { value9 : 17 }));

    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value1 : (i % 10), value2 : "test" + i, value3 : 1 });
    }
    c.insert(docs);

    assertEqual(0, c.updateByExample({ value1 : 11 }, { value1 : 17 }));

    assertEqual(0, c.updateByExample({ value1 : 4, value2 : "test37" }, { value2 : "foxx" }));

    assertEqual(0, c.updateByExample({ foobar : "baz" }, { boom : "bar" }));

    assertEqual(n / 10, c.updateByExample({ value1 : 1 }, { value1 : 4 }));
    assertEqual(0, c.updateByExample({ value1 : 1 }, { value1 : 4 }));

    assertEqual(n / 10, c.updateByExample({ value1 : 2 }, { value1 : 5 }));
    assertEqual(0, c.updateByExample({ value1 : 2 }, { value1 : 5 }));

    assertEqual(1, c.updateByExample({ _key : "test44" }, { value3 : 2 }));
    assertEqual(1, c.updateByExample({ _key : "test44" }, { value3 : 4 }));

    assertEqual(n - 1, c.updateByExample({ value3 : 1 }, { value3 : 99 }));
    assertEqual(1, c.updateByExample({ value3 : 4 }, { value3 : 99 }));

    assertEqual(n / 10, c.updateByExample({ value3 : 99, value1 : 0 }, { value1 : 9 }));
    assertEqual(n, c.updateByExample({ value3 : 99 }, { }));
    assertEqual(2 * (n / 10), c.updateByExample({ value3 : 99, value1 : 9 }, { value12 : 7 }));
    assertEqual(n, c.updateByExample({ }, { value12 : 7 }));

    assertEqual(n, c.updateByExample({ value12 : 7 }, { value12 : null }, false));
    assertEqual(0, c.byExample({ value12: 7 }).toArray().length);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for updateByExample
////////////////////////////////////////////////////////////////////////////////

  var executeUpdateByExampleShardKeys = function (c) {
    const n = 1000;

    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ a : (i % 10), b : (i % 100) });
    }
    c.insert(docs);

    assertEqual(0, c.updateByExample({ a : 11 }, { a : 17 }));
    assertEqual(0, c.updateByExample({ foobar : "baz" }, { a : 17 }));

    assertEqual(10, c.updateByExample({ a : 1, b : 1 }, { a : 1, b : 1, c : "test" }));

    try {
      c.updateByExample({ a : 1, b : 1 }, { a : 2, b : 1 });
      fail();
    } catch (err1) {
      assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, err1.errorNum);
    }

    assertEqual(10, c.updateByExample({ a : 2, b : 2 }, { d : "test" }));

    try {
      c.updateByExample({ a : 1, b : 1 }, { a : 2, d : 4 });
      fail();
    } catch (err2) {
      assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, err2.errorNum);
    }

    try {
      c.updateByExample({ a : 1, b : 1 }, { a : 2 });
      fail();
    } catch (err3) {
      assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, err3.errorNum);
    }
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        db._drop("UnitTestsClusterCrud");
      } catch (err) {
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test any, one shard
////////////////////////////////////////////////////////////////////////////////

    testAnyOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeAny(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test any, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testAnyMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeAny(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test truncate, one shard
////////////////////////////////////////////////////////////////////////////////

    testTruncateOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeTruncate(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test truncate, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testTruncateMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeTruncate(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeByExample, one shard
////////////////////////////////////////////////////////////////////////////////

    testRemoveByExampleOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeRemoveByExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeByExample, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testRemoveByExampleMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeRemoveByExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replaceByExample, one shard
////////////////////////////////////////////////////////////////////////////////

    testReplaceByExampleOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeReplaceByExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replaceByExample, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testReplaceByExampleMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeReplaceByExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replaceByExample, one shard
////////////////////////////////////////////////////////////////////////////////

    testReplaceByExampleShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
      executeReplaceByExampleShardKeys(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replaceByExample, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testReplaceByExampleShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5, shardKeys: [ "a", "b" ] });
      executeReplaceByExampleShardKeys(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test updateByExample, one shard
////////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeUpdateByExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test updateByExample, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeUpdateByExample(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test updateByExample, one shard
////////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
      executeUpdateByExampleShardKeys(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test updateByExample, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5, shardKeys: [ "a", "b" ] });
      executeUpdateByExampleShardKeys(c);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ClusterCrudSimpleSuite);

return jsunity.done();
