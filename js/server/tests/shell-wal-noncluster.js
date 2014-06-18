////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("org/arangodb");
var testHelper = require("org/arangodb/test-helper").Helper;
var db = arangodb.db;
var internal = require("internal");
 
// -----------------------------------------------------------------------------
// --SECTION--                                                     wal functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function WalSuite () {
  var cn = "UnitTestsWal";
  var c;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
      c = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max tick
////////////////////////////////////////////////////////////////////////////////

    testMaxTickEmptyCollection : function () {
      var fig = c.figures();
      // we shouldn't have a tick yet
      assertEqual("0", fig.lastTick);
      assertEqual(0, fig.uncollectedLogfileEntries);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief max tick
////////////////////////////////////////////////////////////////////////////////

    testMaxTickNonEmptyCollection : function () {
      internal.flushWal(true, true);
      var i;

      for (i = 0; i < 100; ++i) {
        c.save({ test:  i });
      }

      // we shouldn't have a tick yet
      var fig = c.figures();
      assertEqual("0", fig.lastTick);
      assertTrue(fig.uncollectedLogfileEntries > 0);

      internal.flushWal(true, true);

      // now we should have a tick
      fig = c.figures();
      assertNotEqual("0", fig.lastTick);
      assertEqual(0, fig.uncollectedLogfileEntries);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief max tick
////////////////////////////////////////////////////////////////////////////////

    testMaxTickAfterUnload : function () {
      var i;

      for (i = 0; i < 100; ++i) {
        c.save({ test:  i });
      }

      internal.flushWal(true, true);

      testHelper.waitUnload(c);

      // we should have a tick and no uncollected entries
      fig = c.figures();
      assertNotEqual("0", fig.lastTick);
      assertEqual(0, fig.uncollectedLogfileEntries);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(WalSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

