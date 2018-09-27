/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, assertUndefined, assertNull, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test CRUD functionality in a cluster
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

var createCollection = function (properties) {
  'use strict';
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



////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for save
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudSaveSuite () {
  'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to execute save tests
////////////////////////////////////////////////////////////////////////////////

  var executeSave = function (c) {
    var doc;

    doc = c.save({ "a" : 1, "b" : "2", "c" : true });
    assertEqual(doc._id, c.document(doc._id)._id);
    assertEqual(doc._id, c.document(doc._key)._id);
    assertEqual(doc._key, c.document(doc._id)._key);
    assertEqual(doc._key, c.document(doc._key)._key);
    assertEqual(1, c.document(doc).a);
    assertEqual("2", c.document(doc).b);
    assertEqual(true, c.document(doc).c);

    doc = c.save({ "a" : 2, "b" : "3", "c" : false});
    assertEqual(doc._id, c.document(doc._id)._id);
    assertEqual(doc._id, c.document(doc._key)._id);
    assertEqual(doc._key, c.document(doc._id)._key);
    assertEqual(doc._key, c.document(doc._key)._key);
    assertEqual(2, c.document(doc).a);
    assertEqual("3", c.document(doc).b);
    assertEqual(false, c.document(doc).c);

    doc = c.save({ "a" : 3, "b" : "4", "c" : null});
    assertEqual(doc._id, c.document(doc._id)._id);
    assertEqual(doc._id, c.document(doc._key)._id);
    assertEqual(doc._key, c.document(doc._id)._key);
    assertEqual(doc._key, c.document(doc._key)._key);
    assertEqual(3, c.document(doc).a);
    assertEqual("4", c.document(doc).b);
    assertNull(c.document(doc).c);

    // use an invalid key
    try {
      doc = c.save({ _key : "  foo  .kmnh" });
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to execute save tests
////////////////////////////////////////////////////////////////////////////////

  var executeSaveShardKeys = function (c) {
    var doc;

    doc = c.save({ "a" : 1, "b" : "2", "c" : true });
    assertEqual(doc._id, c.document(doc._id)._id);
    assertEqual(doc._id, c.document(doc._key)._id);
    assertEqual(doc._key, c.document(doc._id)._key);
    assertEqual(doc._key, c.document(doc._key)._key);
    assertEqual(1, c.document(doc).a);
    assertEqual("2", c.document(doc).b);
    assertEqual(true, c.document(doc).c);

    doc = c.save({ "a" : 2, "b" : "3", "c" : false});
    assertEqual(doc._id, c.document(doc._id)._id);
    assertEqual(doc._id, c.document(doc._key)._id);
    assertEqual(doc._key, c.document(doc._id)._key);
    assertEqual(doc._key, c.document(doc._key)._key);
    assertEqual(2, c.document(doc).a);
    assertEqual("3", c.document(doc).b);
    assertEqual(false, c.document(doc).c);

    doc = c.save({ "a" : 3, "b" : "4", "c" : null});
    assertEqual(doc._id, c.document(doc._id)._id);
    assertEqual(doc._id, c.document(doc._key)._id);
    assertEqual(doc._key, c.document(doc._id)._key);
    assertEqual(doc._key, c.document(doc._key)._key);
    assertEqual(3, c.document(doc).a);
    assertEqual("4", c.document(doc).b);
    assertNull(c.document(doc).c);

    // shard key values defined only partially
    doc = c.save({ "b" : "this-is-a-test", "c" : "test" });
    assertEqual(doc._id, c.document(doc._id)._id);
    assertEqual(doc._id, c.document(doc._key)._id);
    assertEqual(doc._key, c.document(doc._id)._key);
    assertEqual(doc._key, c.document(doc._key)._key);
    assertEqual("this-is-a-test", c.document(doc).b);
    assertEqual("test", c.document(doc).c);

    // no shard key values defined!!
    doc = c.save({ "c" : "test" });
    assertEqual(doc._id, c.document(doc._id)._id);
    assertEqual(doc._id, c.document(doc._key)._id);
    assertEqual(doc._key, c.document(doc._id)._key);
    assertEqual(doc._key, c.document(doc._key)._key);
    assertEqual("test", c.document(doc).c);

    // use a value for _key. this is disallowed
    try {
      doc = c.save({ _key : "test" });
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, err.errorNum);
    }
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
/// @brief test save, one shard
////////////////////////////////////////////////////////////////////////////////

    testSaveOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeSave(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testSaveMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeSave(c);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test save with shard keys, one shard
////////////////////////////////////////////////////////////////////////////////

    testSaveShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
      executeSaveShardKeys(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save with shard keys, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testSaveShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 7, shardKeys: [ "a", "b" ] });
      executeSaveShardKeys(c);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for replace
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudReplaceSuite () {
  'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replace
////////////////////////////////////////////////////////////////////////////////

  var executeReplace = function (c) {
    var old, doc;

    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });

    doc = c.replace(old._key, { "a" : 4, "b" : "5", "c" : true });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);

    assertEqual(4, c.document(doc._key).a);
    assertEqual("5", c.document(doc._key).b);
    assertEqual(true, c.document(doc._key).c);
    assertUndefined(c.document(doc._key).d);

    doc = c.replace(doc._key, { "b": null, "c": "foo" });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);

    assertUndefined(c.document(doc._key).a);
    assertNull(c.document(doc._key).b);
    assertEqual("foo", c.document(doc._key).c);
    assertUndefined(c.document(doc._key).d);

    // use document object to replace
    doc = c.replace(doc, { "a" : true, "c" : null, "d" : [ 1 ] });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(true, c.document(doc._key).a);
    assertUndefined(c.document(doc._key).b);
    assertNull(c.document(doc._key).c);
    assertEqual([ 1 ], c.document(doc._key).d);

    try {
      doc = c.replace(old, { "b" : null, "c" : "foo" });
      fail();
    }
    catch (err1) {
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err1.errorNum);
    }

    try {
      doc = c.replace("foobar", { "value" : 1 });
      fail();
    }
    catch (err2) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replace
////////////////////////////////////////////////////////////////////////////////

  var executeReplaceShardKeys = function (c) {
    var old, doc;

    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });

    doc = c.replace(old._key, { "a" : 1, "b" : "2", "c" : false });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(1, c.document(doc._key).a);
    assertEqual("2", c.document(doc._key).b);
    assertEqual(false, c.document(doc._key).c);
    assertUndefined(c.document(doc._key).d);

    doc = c.replace(doc, { "a" : 1, "c" : null, "d" : [ 1 ], "b" : "2" });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(1, c.document(doc._key).a);
    assertEqual("2", c.document(doc._key).b);
    assertNull(c.document(doc._key).c);
    assertEqual([ 1 ], c.document(doc._key).d);

    // use an old revision for the replace
    try {
      doc = c.replace(old, { "a" : 1, "b" : "2", "c" : "fail" });
      fail();
    }
    catch (err1) {
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err1.errorNum);
    }

    // change shard keys
    try {
      doc = c.replace(doc._key, { "a" : 2, "b" : "2", "c" : false });
      fail();
    }
    catch (err2) {
      if (err2.errorNum !== ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, err2.errorNum);
      }
    }


    // don't specify any values for shard keys, but update later
    old = c.save({ "value" : 1 });
    try {
      c.replace(old, { "foo" : 2 });
      fail();
    } catch (err5) {
      assertEqual(ERRORS.ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN.code, err5.errorNum);
    }

    assertEqual(1, c.document(old._key).value);
    assertUndefined(c.document(old._key).foo);

    doc = c.replace(old, { "a" : null, "b" : null, "foo" : 3 });
    assertEqual(3, c.document(doc._key).foo);
    assertUndefined(c.document(doc._key).value);

    // specify null values for shard keys, but update later
    old = c.save({ "a" : null, "b" : null, "value" : 1 });
    try {
      doc = c.replace(old, { "foo" : 2 });
      fail();
    } catch (err6) {
      assertEqual(ERRORS.ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN.code, err6.errorNum);
    }

    // change shard keys
    try {
      doc = c.replace(doc._key, { "a" : 2, "b" : "2", "c" : false });
      fail();
    }
    catch (err3) {
      // We cannot determine if it is SHARDING_ATTRIBUTES_CHANGED or NOT_FOUND using one round-trip only.
      if (err3.errorNum !== ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err3.errorNum);
      } else {
        assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, err3.errorNum);
      }
    }

    try {
      doc = c.replace("foobar", { "value" : 1, a: 1, b: 2 });
      fail();
    }
    catch (err4) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err4.errorNum);
    }

    try {
      doc = c.replace("foobar", { "value" : 1 });
      fail();
    }
    catch (err9) {
      assertEqual(ERRORS.ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN.code, err9.errorNum);
    }
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
/// @brief test replace, one shard
////////////////////////////////////////////////////////////////////////////////

    testReplaceOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeReplace(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testReplaceMultipleShards : function () {
      var c = createCollection({ numberOfShards: 7 });
      executeReplace(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace with shard keys, one shard
////////////////////////////////////////////////////////////////////////////////

    testReplaceShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
      executeReplaceShardKeys(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace with shard keys, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testReplaceShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5, shardKeys: [ "a", "b" ] });
      executeReplaceShardKeys(c);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for update
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudUpdateSuite () {
  'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for update
////////////////////////////////////////////////////////////////////////////////

  var executeUpdate = function (c) {
    var old, doc;

    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });

    doc = c.update(old._key, { "a" : 4, "b" : "5", "c" : false, "e" : "foo" });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(4, c.document(doc._key).a);
    assertEqual("5", c.document(doc._key).b);
    assertEqual(false, c.document(doc._key).c);
    assertEqual("test", c.document(doc._key).d);
    assertEqual("foo", c.document(doc._key).e);

    doc = c.update(doc._key, { "b" : null, "c" : "foo" });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(4, c.document(doc._key).a);
    assertEqual("null", c.document(doc._key).b);
    assertEqual("foo", c.document(doc._key).c);
    assertEqual("test", c.document(doc._key).d);
    assertEqual("foo", c.document(doc._key).e);

    // use document object to update
    doc = c.update(doc, { "a" : true, "c" : null, "d" : [ 1 ] });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(true, c.document(doc._key).a);
    assertNull(c.document(doc._key).b);
    assertNull(c.document(doc._key).c);
    assertEqual([ 1 ], c.document(doc._key).d);
    assertEqual("foo", c.document(doc._key).e);

    // use keepNull = false
    doc = c.update(doc._key, { "b" : null, "c" : "foo", e : null }, false, false);
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual("true", c.document(doc._key).a);
    assertUndefined(c.document(doc._key).b);
    assertEqual("foo", c.document(doc._key).c);
    assertEqual([ 1 ], c.document(doc._key).d);
    assertUndefined(c.document(doc._key).e);

    // use keepNull = true
    doc = c.update(doc._key, { "b" : null, "c" : "foo", e : null }, false, true);
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual("true", c.document(doc._key).a);
    assertNull(c.document(doc._key).b);
    assertEqual("foo", c.document(doc._key).c);
    assertEqual([ 1 ], c.document(doc._key).d);
    assertNull(c.document(doc._key).e);

    try {
      doc = c.update(old, { "b" : null, "c" : "foo" });
      fail();
    }
    catch (err1) {
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err1.errorNum);
    }

    try {
      doc = c.update("foobar", { "value" : 1 });
      fail();
    }
    catch (err2) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for update
////////////////////////////////////////////////////////////////////////////////

  var executeUpdateShardKeys = function (c) {
    var old, doc;

    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });

    doc = c.update(old._key, { "a" : 1, "b" : "2", "c" : false, "e" : "foo" });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(1, c.document(doc._key).a);
    assertEqual("2", c.document(doc._key).b);
    assertEqual(false, c.document(doc._key).c);
    assertEqual("test", c.document(doc._key).d);
    assertEqual("foo", c.document(doc._key).e);

    doc = c.update(doc._key, { "b" : "2", "c" : "foo" });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(1, c.document(doc._key).a);
    assertEqual("2", c.document(doc._key).b);
    assertEqual("foo", c.document(doc._key).c);
    assertEqual("test", c.document(doc._key).d);
    assertEqual("foo", c.document(doc._key).e);

    // use document object to update
    doc = c.update(doc, { "a" : 1, "c" : null, "d" : [ 1 ] });
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(1, c.document(doc._key).a);
    assertEqual("2", c.document(doc._key).b);
    assertNull(c.document(doc._key).c);
    assertEqual([ 1 ], c.document(doc._key).d);
    assertEqual("foo", c.document(doc._key).e);

    // use keepNull = false
    doc = c.update(doc._key, { "c" : "foo", e : null }, false, false);
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(1, c.document(doc._key).a);
    assertEqual("2", c.document(doc._key).b);
    assertEqual("foo", c.document(doc._key).c);
    assertEqual([ 1 ], c.document(doc._key).d);
    assertUndefined(c.document(doc._key).e);

    // use keepNull = true
    doc = c.update(doc._key, { "c" : "foo", e : null }, false, true);
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertNotEqual(old._rev, doc._rev);
    assertEqual(1, c.document(doc._key).a);
    assertEqual("2", c.document(doc._key).b);
    assertEqual("foo", c.document(doc._key).c);
    assertEqual([ 1 ], c.document(doc._key).d);
    assertNull(c.document(doc._key).e);

    try {
      doc = c.update(old, { "c" : "food" });
      fail();
    }
    catch (err1) {
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err1.errorNum);
    }

    try {
      doc = c.update("foobar", { "value" : 1 });
      fail();
    }
    catch (err2) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
    }

    // change shard keys
    try {
      doc = c.update(doc._key, { "a" : 2, "b" : "2", "c" : false });
      fail();
    }
    catch (err2) {
      if (err2.errorNum !== ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, err2.errorNum);
      }
    }

    try {
      doc = c.update(doc._key, { "b" : "1", "c" : false });
      fail();
    }
    catch (err3) {
      assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, err3.errorNum);
    }


    // don't specify any values for shard keys, but update later
    old = c.save({ "value" : 1 });
    doc = c.update(old, { "foo" : 2 });

    assertEqual(2, c.document(doc._key).foo);
    assertEqual(1, c.document(doc._key).value);

    doc = c.update(doc, { "a" : null, "b" : null, "foo" : 3 });
    assertEqual(3, c.document(doc._key).foo);
    assertEqual(1, c.document(doc._key).value);

    // specify null values for shard keys, but update later
    old = c.save({ "a" : null, "b" : null, "value" : 1 });
    doc = c.update(old, { "foo" : 2 });
    assertNull(c.document(doc._key).a);
    assertNull(c.document(doc._key).b);
    assertEqual(2, c.document(doc._key).foo);
    assertEqual(1, c.document(doc._key).value);
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
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeUpdate(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateMultipleShards : function () {
      var c = createCollection({ numberOfShards: 7 });
      executeUpdate(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
      executeUpdateShardKeys(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5, shardKeys: [ "a", "b" ] });
      executeUpdateShardKeys(c);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for delete
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudDeleteSuite () {
  'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for delete
////////////////////////////////////////////////////////////////////////////////

  var executeDelete = function (c) {
    var old, doc;

    // remove by id
    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });
    assertTrue(c.remove(old._id));

    // remove by key
    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });
    assertTrue(c.remove(old._key));

    // remove by document
    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });
    assertTrue(c.remove(old));

    // remove by document, non-existing
    try {
      c.remove(old);
      fail();
    }
    catch (err1) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err1.errorNum);
    }

    // remove a non-existing revision
    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });
    doc = c.update(old._key, { "foo" : "bar" });
    assertTrue(old._rev !== doc._rev);

    try {
      c.remove(old);
      fail();
    }
    catch (err2) {
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err2.errorNum);
    }

    // remove non-existing
    try {
      c.document("foobarbaz");
    }
    catch (err3) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err3.errorNum);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for delete
////////////////////////////////////////////////////////////////////////////////

  var executeDeleteShardKeys = function (c) {
    var old, doc;

    // remove by id
    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });
    assertTrue(c.remove(old._id));

    // remove by key
    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });
    assertTrue(c.remove(old._key));

    // remove by document
    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });
    assertTrue(c.remove(old));

    // remove by document, non-existing
    try {
      c.remove(old);
      fail();
    }
    catch (err1) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err1.errorNum);
    }

    // remove a non-existing revision
    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });
    doc = c.update(old._key, { "foo" : "bar" });

    try {
      c.remove(old);
      fail();
    }
    catch (err2) {
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err2.errorNum);
    }

    // remove non-existing
    try {
      c.document("foobarbaz");
    }
    catch (err3) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err3.errorNum);
    }
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
/// @brief test delete
////////////////////////////////////////////////////////////////////////////////

    testDeleteOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeDelete(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test delete
////////////////////////////////////////////////////////////////////////////////

    testDeleteMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeDelete(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test delete
////////////////////////////////////////////////////////////////////////////////

    testDeleteShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
      executeDeleteShardKeys(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test delete
////////////////////////////////////////////////////////////////////////////////

    testDeleteShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5, shardKeys: [ "a", "b" ] });
      executeDeleteShardKeys(c);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for document
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudDocumentSuite () {
  'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for document
////////////////////////////////////////////////////////////////////////////////

  var executeDocument = function (c) {
    var old, doc;

    try {
      c.document(c.name() + "/foobar");
      fail();
    }
    catch (err1) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err1.errorNum);
    }

    try {
      c.document("foobar");
      fail();
    }
    catch (err2) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
    }

    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });

    // fetch by id
    doc = c.document(old._id);
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertEqual(old._rev, doc._rev);
    assertEqual(1, doc.a);
    assertEqual("2", doc.b);
    assertEqual(true, doc.c);
    assertEqual("test", doc.d);

    // fetch by key
    doc = c.document(old._key);
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertEqual(old._rev, doc._rev);
    assertEqual(1, doc.a);
    assertEqual("2", doc.b);
    assertEqual(true, doc.c);
    assertEqual("test", doc.d);

    // fetch by doc
    doc = c.document(old);
    assertEqual(old._id, doc._id);
    assertEqual(old._key, doc._key);
    assertEqual(old._rev, doc._rev);
    assertEqual(1, doc.a);
    assertEqual("2", doc.b);
    assertEqual(true, doc.c);
    assertEqual("test", doc.d);

    // fetch by doc, wrong revision
    doc = c.update(doc._key, { "a" : 1, "b" : "2", "c" : false });

    try {
      c.document(old);
      fail();
    }
    catch (err3) {
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err3.errorNum);
    }

    // fetch after deletion
    c.remove(doc);
    try {
      c.document(doc._key);
      fail();
    }
    catch (err4) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err4.errorNum);
    }
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
/// @brief test document
////////////////////////////////////////////////////////////////////////////////

    testDocumentOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeDocument(c);

      // use a user-defined key
      var doc, old;
      old = c.save({ "_key" : "meow", "a" : 1, "b" : "2", "c" : false });
      doc = c.document(old);
      assertEqual(c.name() + "/meow", doc._id);
      assertEqual("meow", doc._key);
      assertEqual(old._rev, doc._rev);
      assertEqual(1, doc.a);
      assertEqual("2", doc.b);
      assertEqual(false, doc.c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document
////////////////////////////////////////////////////////////////////////////////

    testDocumentMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeDocument(c);

      // use a user-defined key
      var doc, old;
      old = c.save({ "_key" : "meow", "a" : 1, "b" : "2", "c" : false });
      doc = c.document(old);
      assertEqual(c.name() + "/meow", doc._id);
      assertEqual("meow", doc._key);
      assertEqual(old._rev, doc._rev);
      assertEqual(1, doc.a);
      assertEqual("2", doc.b);
      assertEqual(false, doc.c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document
////////////////////////////////////////////////////////////////////////////////

    testDocumentShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
      executeDocument(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document
////////////////////////////////////////////////////////////////////////////////

    testDocumentShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5, shardKeys: [ "a", "b" ] });
      executeDocument(c);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for exists
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudExistsSuite () {
  'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for exists
////////////////////////////////////////////////////////////////////////////////

  var executeExists = function (c) {
    var old, doc;

    assertFalse(c.exists(c.name() + "/foobar"));
    assertFalse(c.exists("foobar"));

    old = c.save({ "a" : 1, "b" : "2", "c" : true, "d" : "test" });

    // fetch by id
    assertTrue(c.exists(old._id));
    assertTrue(c.exists(old._key));
    assertTrue(c.exists(old));

    // fetch by doc, wrong revision
    doc = c.update(old._key, { "a" : 1, "b" : "2", "c" : false });
    assertTrue(c.exists(old._id));
    assertTrue(c.exists(old._key));
    try {
      c.exists(old);
      fail();
    } catch (e) {
      assertEqual(e.errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
    }

    // fetch after deletion
    c.remove(doc);
    assertFalse(c.exists(old._id));
    assertFalse(c.exists(old._key));
    assertFalse(c.exists(old));
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
/// @brief test exists, one shard
////////////////////////////////////////////////////////////////////////////////

    testExistsOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
      executeExists(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test exists, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testExistsMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
      executeExists(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test exists, one shard
////////////////////////////////////////////////////////////////////////////////

    testExistsShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
      executeExists(c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test exists, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testExistsShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5, shardKeys: [ "a", "b" ] });
      executeExists(c);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ClusterCrudSaveSuite);
jsunity.run(ClusterCrudReplaceSuite);
jsunity.run(ClusterCrudUpdateSuite);
jsunity.run(ClusterCrudDeleteSuite);
jsunity.run(ClusterCrudDocumentSuite);
jsunity.run(ClusterCrudExistsSuite);

return jsunity.done();

