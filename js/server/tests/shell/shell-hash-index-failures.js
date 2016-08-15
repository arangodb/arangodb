/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests failure cases in HashIndex
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Unique Hash Index
////////////////////////////////////////////////////////////////////////////////

function UniqueHashIndexFailuresSuite () {
  'use strict';
  var cn = "UnitTestsCollectionHash";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      internal.debugClearFailAt();
      collection = internal.db._create(cn);
      collection.ensureUniqueConstraint("a");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.debugClearFailAt();
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: unique hash index fill element oom
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
/// @brief test: unique hash index fill element oom, other position
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
/// @brief test suite: Hash Index Multi
////////////////////////////////////////////////////////////////////////////////

function HashIndexMultiFailuresSuite () {
  'use strict';
  var cn = "UnitTestsCollectionHash";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.debugClearFailAt();
      internal.db._drop(cn);
      collection = internal.db._create(cn);
      collection.ensureHashIndex("a");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.debugClearFailAt();
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: hash index multi fill element oom
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexElementOOM : function () {
      collection.ensureUniqueConstraint("a");
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
/// @brief test: hash index multi fill element oom, other position
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexElementOOMOther : function () {
      collection.ensureUniqueConstraint("a");
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
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (internal.debugCanUseFailAt()) {
  jsunity.run(UniqueHashIndexFailuresSuite);
  jsunity.run(HashIndexMultiFailuresSuite);
}

return jsunity.done();

