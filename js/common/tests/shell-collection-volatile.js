////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface w/ volatile collections
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              volatile collections
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: volatile collections
////////////////////////////////////////////////////////////////////////////////

function CollectionVolatileSuite () {
  var ERRORS = require("internal").errors;
  var cn = "UnittestsVolatileCollection";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a volatile collection
////////////////////////////////////////////////////////////////////////////////

    testCreation1 : function () {
      var c = internal.db._create(cn, { isVolatile : true, waitForSync : false });
      assertEqual(cn, c.name());
      assertEqual(true, c.properties().isVolatile);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a volatile collection
////////////////////////////////////////////////////////////////////////////////

    testCreation2 : function () {
      var c = internal.db._create(cn, { isVolatile : true });
      assertEqual(cn, c.name());
      assertEqual(true, c.properties().isVolatile);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create w/ error
////////////////////////////////////////////////////////////////////////////////

    testCreationError : function () {
      try {
        // cannot set isVolatile and waitForSync at the same time
        var c = internal.db._create(cn, { isVolatile : true, waitForSync : true });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test property change
////////////////////////////////////////////////////////////////////////////////

    testPropertyChange : function () {
      var c = internal.db._create(cn, { isVolatile : true, waitForSync : false });

      try {
        // cannot set isVolatile and waitForSync at the same time
        c.properties({ waitForSync : true });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief load/unload
////////////////////////////////////////////////////////////////////////////////

    testLoadUnload : function () {
      var c = internal.db._create(cn, { isVolatile : true });
      assertEqual(cn, c.name());
      assertEqual(true, c.properties().isVolatile);
      c.unload();

      internal.wait(4);
      assertEqual(true, c.properties().isVolatile);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief data storage
////////////////////////////////////////////////////////////////////////////////

    testStorage : function () {
      var c = internal.db._create(cn, { isVolatile : true });
      assertEqual(true, c.properties().isVolatile);

      c.save({"test": true});
      assertEqual(1, c.count());
      c.unload();
      
      internal.wait(4);
      assertEqual(true, c.properties().isVolatile);
      assertEqual(0, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief data storage
////////////////////////////////////////////////////////////////////////////////

    testStorageMany : function () {
      var c = internal.db._create(cn, { isVolatile : true, journalSize: 1024 * 1024 });
      assertEqual(true, c.properties().isVolatile);

      for (var i = 0; i < 10000; ++i) {
        c.save({"test": true, "foo" : "bar"});
      }
      
      assertEqual(10000, c.count());
      
      c.unload();
      c = null;
      
      internal.wait(5);
      c = internal.db[cn];
      assertEqual(true, c.properties().isVolatile);
      assertEqual(0, c.count());
      assertEqual([ ], c.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction
////////////////////////////////////////////////////////////////////////////////

    testCompaction : function () {
      var c = internal.db._create(cn, { isVolatile : true, journalSize: 1024 * 1024 });
      assertEqual(true, c.properties().isVolatile);

      for (var i = 0; i < 10000; ++i) {
        c.save({"test": "the quick brown fox jumped over the lazy dog", "foo" : "bar"});
      }
      
      assertEqual(10000, c.count());
      c.truncate();
      c.save({"test": 1});
      assertEqual(1, c.count());
      c.truncate();
      
      c.unload();
      
      internal.wait(5);
      assertEqual(0, c.count());
      assertEqual([ ], c.toArray());
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionVolatileSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

