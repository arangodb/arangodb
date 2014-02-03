/*jslint indent: 2, nomen: true, maxlen: 80 */
/*global require, assertEqual, assertTrue */

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
var arangodb = require("org/arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;

// -----------------------------------------------------------------------------
// --SECTION--                                                Cluster CRUD tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for save
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudSaveSuite () {
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
/// @brief test save
////////////////////////////////////////////////////////////////////////////////

    testSaveOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save
////////////////////////////////////////////////////////////////////////////////

    testSaveMultipleShards : function () {
      var c = createCollection({ numberOfShards: 5 });
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save
////////////////////////////////////////////////////////////////////////////////

    testSaveShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save
////////////////////////////////////////////////////////////////////////////////

    testSaveShardKeysMultipleShards : function () {
      var c = createCollection({ numberOfShards: 7, shardKeys: [ "a", "b" ] });
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
    }   

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for replace
////////////////////////////////////////////////////////////////////////////////

function ClusterCrudReplaceSuite () {
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
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceOneShard : function () {
      var c = createCollection({ numberOfShards: 1 });
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
      assertNotEqual(old._rev, doc._rev);
      assertEqual(true, c.document(doc._key).a);
      assertUndefined(c.document(doc._key).b);
      assertNull(c.document(doc._key).c);
      assertEqual([ 1 ], c.document(doc._key).d);

      try {
        doc = c.replace(old, { "b" : null, "c" : "foo" });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceMultipleShards : function () {
      var c = createCollection({ numberOfShards: 7 });
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
      assertNotEqual(old._rev, doc._rev);

      assertNull(c.document(doc._key).b);
      assertEqual("foo", c.document(doc._key).c);
      assertUndefined(c.document(doc._key).a);
      assertUndefined(c.document(doc._key).d);
      
      // use document object to replace 
      doc = c.replace(doc, { "a" : true, "c" : null, "d" : [ 1 ] });
      assertEqual(old._id, doc._id);
      assertEqual(old._key, doc._key);
      assertNotEqual(old._rev, doc._rev);
      assertNotEqual(old._rev, doc._rev);
      assertEqual(true, c.document(doc._key).a);
      assertUndefined(c.document(doc._key).b);
      assertNull(c.document(doc._key).c);
      assertEqual([ 1 ], c.document(doc._key).d);

      // use an old revision for the replace
      try {
        doc = c.replace(old, { "b" : null, "c" : "foo" });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceShardKeysOneShard : function () {
      var c = createCollection({ numberOfShards: 1, shardKeys: [ "a", "b" ] });
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
        assertEqual(ERRORS.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, err2.errorNum);
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ClusterCrudSaveSuite);
jsunity.run(ClusterCrudReplaceSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
