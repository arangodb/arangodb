/*jslint indent: 2,
         nomen: true,
         maxlen: 80 */
/*global require,
    db,
    assertEqual, assertTrue,
    ArangoCollection, ArangoEdgesCollection */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the index
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
var console = require("console");

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: basics
////////////////////////////////////////////////////////////////////////////////

function indexSuite() {
  var cn = "UnitTestsCollectionIdx";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.unload();

      while (collection.status() != internal.ArangoCollection.STATUS_UNLOADED) {
        console.log("waiting for collection '%s' to unload", cn);
        internal.wait(1);
      }

      collection.drop();

      while (collection.status() != internal.ArangoCollection.STATUS_DELETED) {
        console.log("waiting for collection '%s' to drop", cn);
        internal.wait(1);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get indexes
////////////////////////////////////////////////////////////////////////////////

    testGetIndexes : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      collection.ensureGeoIndex("a");
      collection.ensureGeoIndex("a", "b");

      res = collection.getIndexes();

      assertEqual(3, res.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get index
////////////////////////////////////////////////////////////////////////////////

    testIndex : function () {
      var id = collection.ensureGeoIndex("a");

      var idx = collection.index(id.id);
      assertEqual(id.id, idx.id);

      idx = collection.index(id);
      assertEqual(id.id, idx.id);

      idx = internal.db._index(id.id);
      assertEqual(id.id, idx.id);

      idx = internal.db._index(id);
      assertEqual(id.id, idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop index
////////////////////////////////////////////////////////////////////////////////

    testDropIndex : function () {
      var id = collection.ensureGeoIndex("a");
      var res = collection.dropIndex(id.id);
      assertEqual(true, res);

      res = collection.dropIndex(id.id);
      assertEqual(false, res);

      id = collection.ensureGeoIndex("a");
      res = collection.dropIndex(id);
      assertEqual(true, res);

      res = collection.dropIndex(id);
      assertEqual(false, res);

      id = collection.ensureGeoIndex("a");
      res = internal.db._dropIndex(id);
      assertEqual(true, res);

      res = internal.db._dropIndex(id);
      assertEqual(false, res);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(indexSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
