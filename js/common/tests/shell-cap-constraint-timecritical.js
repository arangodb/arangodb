/*global require, db, assertEqual, assertTrue, ArangoCollection */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the cap constraint
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var testHelper = require("org/arangodb/test-helper").Helper;

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
///   these tests seem to fail when run in cluster or with valgrind from arangosh
///   (while they succeed when run on the frontend)
///   test-helper.js:Helper.waitUnload expects timeley behaviour
////////////////////////////////////////////////////////////////////////////////

function CapConstraintTimecriticalSuite() {
  var ERRORS = internal.errors;
  var cn = "UnitTestsCollectionCap";
  var collection = null;

  var nsort = function (l, r) {
    if (l !== r) {
      return (l < r) ? -1 : 1;
    }
    return 0;
  };

  var assertBadParameter = function (err) {
    assertTrue(err.errorNum === ERRORS.ERROR_BAD_PARAMETER.code ||
               err.errorNum === ERRORS.ERROR_HTTP_BAD_PARAMETER.code);
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  setUp : function () {
    internal.db._drop(cn);
    collection = internal.db._create(cn);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  tearDown : function () {
    if (collection !== null) {
      internal.wait(0);
      collection.unload();
      collection.drop();
      collection = null;
    }
    internal.wait(0.0);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: reload
////////////////////////////////////////////////////////////////////////////////

    testReload : function () {
      var idx = collection.ensureCapConstraint(5);
      var fun = function(d) { return d.n; };

      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(5, idx.size);
      assertEqual(0, idx.byteSize);

      assertEqual(0, collection.count());

      for (var i = 0;  i < 10;  ++i) {
        collection.save({ n : i });
      }

      assertEqual(5, collection.count());
      assertEqual([5, 6, 7, 8, 9], collection.toArray().map(fun).sort(nsort));

      testHelper.waitUnload(collection, true);

      assertEqual(5, collection.count());
      assertEqual([5, 6, 7, 8, 9], collection.toArray().map(fun).sort(nsort));

      collection.save({ n : 10 });
      assertEqual(5, collection.count());
      assertEqual([6, 7, 8, 9, 10], collection.toArray().map(fun).sort(nsort));

      collection.save({ n : 11 });
      assertEqual(5, collection.count());
      assertEqual([7, 8, 9, 10, 11], collection.toArray().map(fun).sort(nsort));

      testHelper.waitUnload(collection, true);

      assertEqual(5, collection.count());
      assertEqual([7, 8, 9, 10, 11], collection.toArray().map(fun).sort(nsort));

      for (var i = 15;  i < 20;  ++i) {
        collection.save({ n : i });
      }

      assertEqual(5, collection.count());
      assertEqual([15, 16, 17, 18, 19], collection.toArray().map(fun).sort(nsort));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: reload
////////////////////////////////////////////////////////////////////////////////

    testReloadMulti : function () {
      var idx = collection.ensureCapConstraint(5);
      var fun = function(d) { return d.n; };

      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(5, idx.size);
      assertEqual(0, idx.byteSize);

      assertEqual(0, collection.count());

      testHelper.waitUnload(collection, true);

      assertEqual(0, collection.count());

      for (var i = 0; i < 10; ++i) {
        collection.save({ n : i });
      }

      testHelper.waitUnload(collection, true);

      assertEqual(5, collection.count());
      assertEqual([5, 6, 7, 8, 9], collection.toArray().map(fun).sort(nsort));

      collection.save({ n : 10 });

      testHelper.waitUnload(collection, true);

      assertEqual(5, collection.count());
      assertEqual([6, 7, 8, 9, 10], collection.toArray().map(fun).sort(nsort));

      collection.save({ n: 0 });
      assertEqual([0, 7, 8, 9, 10], collection.toArray().map(fun).sort(nsort));
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CapConstraintTimecriticalSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
