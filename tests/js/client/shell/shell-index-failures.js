/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests failure cases in index
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");

function UniqueIndexFailuresSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  let collection = null;

  return {

    setUp : function () {
      internal.db._drop(cn);
      internal.debugClearFailAt();
      collection = internal.db._create(cn);
      collection.ensureIndex({ type: "persistent", fields: ["a"], unique: true });
    },

    tearDown : function () {
      internal.debugClearFailAt();
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: unique index fill element oom
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexElementOOM : function () {
      internal.debugSetFailAt("FillElementOOM");
      try {
        collection.save({a: 1});
        fail();
      } catch (e) {
        assertTrue(e.errorNum === internal.errors.ERROR_OUT_OF_MEMORY.code ||
                   e.errorNum === internal.errors.ERROR_INTERNAL.code);
      }
      assertEqual(collection.count(), 0);
      assertEqual(collection.firstExample({a: 1}), null);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: unique index fill element oom, other position
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexElementOOMOther : function () {
      internal.debugSetFailAt("FillElementOOM2");
      try {
        collection.save({a: 1});
        fail();
      } catch (e) {
        assertTrue(e.errorNum === internal.errors.ERROR_OUT_OF_MEMORY.code ||
                   e.errorNum === internal.errors.ERROR_INTERNAL.code);
      }
      assertEqual(collection.count(), 0);
      assertEqual(collection.firstExample({a: 1}), null);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: index multi
////////////////////////////////////////////////////////////////////////////////

function IndexMultiFailuresSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  let collection = null;

  return {

    setUp : function () {
      internal.debugClearFailAt();
      internal.db._drop(cn);
      collection = internal.db._create(cn);
      collection.ensureIndex({ type: "persistent", fields: ["a"] });
    },

    tearDown : function () {
      internal.debugClearFailAt();
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index multi fill element oom
////////////////////////////////////////////////////////////////////////////////

    testMultiFailCreateIndexElementOOM : function () {
      collection.ensureIndex({ type: "persistent", fields: ["a"], unique: true });
      internal.debugSetFailAt("FillElementOOM");
      try {
        collection.save({a: 1});
        fail();
      } catch (e) {
        assertTrue(e.errorNum === internal.errors.ERROR_OUT_OF_MEMORY.code ||
                   e.errorNum === internal.errors.ERROR_INTERNAL.code);
      }
      assertEqual(collection.count(), 0);
      assertEqual(collection.firstExample({a: 1}), null);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index multi fill element oom, other position
////////////////////////////////////////////////////////////////////////////////

    testMultiFailCreateIndexElementOOMOther : function () {
      collection.ensureIndex({ type: "persistent", fields: ["a"], unique: true });
      internal.debugSetFailAt("FillElementOOM2");
      try {
        collection.save({a: 1});
        fail();
      } catch (e) {
        assertTrue(e.errorNum === internal.errors.ERROR_OUT_OF_MEMORY.code ||
                   e.errorNum === internal.errors.ERROR_INTERNAL.code);
      }
      assertEqual(collection.count(), 0);
      assertEqual(collection.firstExample({a: 1}), null);
    }

  };
}

function IndexMultiBugsSuite() {
  'use strict';
  const cn = "UnitTestsCollectionBug";
  let collection = null;

  return {

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

    tearDown : function () {
      internal.db._drop(cn);
    },

    testCollisionFail: function() {
      collection.ensureIndex({"type" : "persistent", "fields" : ["ipList[*].ip"],"unique" : false, "sparse" : false });
      collection.insert({
        "_key" : "233808782_Et0;0.32", 
        "_id" : "copy/233808782_Et0;0.32", 
        "_rev" : "_UHWKDi6---", 
        "ipList" : [ 
          { 
            "ip" : "10.73.32.32", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "primary" 
          }, 
          { 
            "ip" : "10.73.32.11", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.12", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.13", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.14", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.15", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.16", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.17", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.18", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.19", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.20", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.21", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.22", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          }, 
          { 
            "ip" : "10.73.32.26", 
            "mask" : 24, 
            "net" : "10.73.32.0/24", 
            "type" : "secondary" 
          } 
        ], 
        "name" : "Et0/0.32", 
        "vrf" : "" 
      });
      assertEqual(collection.truncate({ compact: false }), undefined);
    }
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(UniqueIndexFailuresSuite);
  jsunity.run(IndexMultiFailuresSuite);
}
jsunity.run(IndexMultiBugsSuite);

return jsunity.done();
