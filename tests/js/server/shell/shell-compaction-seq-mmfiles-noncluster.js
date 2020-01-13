/*jshint globalstrict:false, strict:false, sub: true */
/*global assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the compaction
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
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CompactionSuite () {
  'use strict';
  
  var cn = "UnitTestsCompaction";
  var c;

  return {

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
    },
    
    tearDown : function () {
      db._drop(cn);
      c = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and update
////////////////////////////////////////////////////////////////////////////////

    testInsertUpdateInterleaved : function () {
      // make sure we create a few datafiles
      c.properties({ journalSize: 1 * 1024 * 1024 });

      var fig, i, n = 200000;
      for (i = 0; i < n; ++i) {
        c.insert({ 
          _key: "test" + i, 
          value1: i, 
          value2: "test" + i, 
          value3: "this is a test value that depends on " + i + " and nothing else" 
        });
        c.update("test" + i, { value1: i * 2 });
      }

      assertEqual(n, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.alive.count === n) {
          break;
        }

        internal.wait(1, false);
      }

      assertEqual(0, fig.dead.deletion);
      assertEqual(n, fig.alive.count);
      assertEqual(0, fig.journals.count);
      
      for (i = 0; i < n; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i * 2, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and update
////////////////////////////////////////////////////////////////////////////////

    testInsertUpdateLinear : function () {
      // make sure we create a few datafiles
      c.properties({ journalSize: 1 * 1024 * 1024 });

      var fig, i, n = 200000;
      for (i = 0; i < n; ++i) {
        c.insert({ 
          _key: "test" + i, 
          value1: i, 
          value2: "test" + i, 
          value3: "this is a test value that depends on " + i + " and nothing else" 
        });
      }
      assertEqual(n, c.count());
      
      for (i = 0; i < n; ++i) {
        c.update("test" + i, { value1: i * 2 });
      }
      assertEqual(n, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.alive.count === n) {
          break;
        }

        internal.wait(1, false);
      }

      assertEqual(0, fig.dead.deletion);
      assertEqual(n, fig.alive.count);
      assertEqual(0, fig.journals.count);
      
      for (i = 0; i < n; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i * 2, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and update
////////////////////////////////////////////////////////////////////////////////

    testInsertUpdateSmallLinear : function () {
      var fig, i, n = 2000;
      for (i = 0; i < n; ++i) {
        c.insert({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      assertEqual(n, c.count());
      
      for (i = 0; i < n; ++i) {
        c.update("test" + i, { value1: i * 2 });
      }
      assertEqual(n, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.datafiles.count === 1 && fig.alive.count === n) {
          break;
        }

        internal.wait(1, false);
      }

      assertEqual(0, fig.dead.deletion);
      assertEqual(n, fig.alive.count);
      assertEqual(0, fig.journals.count);
      
      for (i = 0; i < n; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i * 2, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and update
////////////////////////////////////////////////////////////////////////////////

    testInsertUpdateRemoveLinear : function () {
      // make sure we create a few datafiles
      c.properties({ journalSize: 1 * 1024 * 1024 });

      var fig, i, n = 200000;
      for (i = 0; i < n; ++i) {
        c.insert({ 
          _key: "test" + i, 
          value1: i, 
          value2: "test" + i, 
          value3: "this is a test value that depends on " + i + " and nothing else" 
        });
      }
      assertEqual(n, c.count());
      
      for (i = 0; i < n; ++i) {
        c.update("test" + i, { value1: i * 2 });
      }
      assertEqual(n, c.count());
      
      for (i = 0; i < n / 2; ++i) {
        c.remove("test" + i);
      }
      assertEqual(n / 2, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.dead.deletion === n / 2) {
          break;
        }

        internal.wait(1, false);
      }

      assertEqual(n / 2, fig.dead.deletion);
      assertEqual(n / 2, fig.alive.count);
      assertEqual(0, fig.journals.count);
      
      for (i = n / 2; i < n; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i * 2, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and remove
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveInterleaved : function () {
      // make sure we create a few datafiles
      c.properties({ journalSize: 1 * 1024 * 1024 });

      var fig, i, n = 200000;
      for (i = 0; i < n; ++i) {
        c.insert({ 
          _key: "test" + i, 
          value1: i, 
          value2: "test" + i, 
          value3: "this is a test value that depends on " + i + " and nothing else" 
        });
        c.remove("test" + i);
      }

      assertEqual(0, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.dead.deletion === 0 && fig.dead.count === 0) {
          break;
        }

        internal.wait(1, false);
      }

      assertEqual(0, fig.dead.count);
      assertEqual(0, fig.dead.deletion);
      assertEqual(0, fig.alive.count);
      assertEqual(0, fig.journals.count);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and remove
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveLinear : function () {
      // make sure we create a few datafiles
      c.properties({ journalSize: 1 * 1024 * 1024 });

      var fig, i, n = 200000;
      for (i = 0; i < n; ++i) {
        c.insert({ 
          _key: "test" + i, 
          value1: i, 
          value2: "test" + i, 
          value3: "this is a test value that depends on " + i + " and nothing else" 
        });
      }
      assertEqual(n, c.count());

      for (i = 0; i < n; ++i) {
        c.remove("test" + i);
      }
      assertEqual(0, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.dead.deletion === 0) {
          break;
        }

        internal.wait(1, false);
      }

      assertEqual(0, fig.dead.count);
      assertEqual(0, fig.dead.deletion);
      assertEqual(0, fig.alive.count);
      assertEqual(0, fig.journals.count);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and remove
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveSmallLinear : function () {
      var fig, i, n = 2000;
      for (i = 0; i < n; ++i) {
        c.insert({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      assertEqual(n, c.count());

      for (i = 0; i < n; ++i) {
        c.remove("test" + i);
      }
      assertEqual(0, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.dead.deletion === 0) {
          break;
        }

        internal.wait(1, false);
      }

      assertEqual(0, fig.dead.count);
      assertEqual(0, fig.dead.deletion);
      assertEqual(0, fig.alive.count);
      assertEqual(0, fig.journals.count);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and remove
////////////////////////////////////////////////////////////////////////////////

    testInsertRemovePartial : function () {
      // make sure we create a few datafiles
      c.properties({ journalSize: 1 * 1024 * 1024 });

      var fig, i, n = 200000;
      for (i = 0; i < n; ++i) {
        c.insert({ 
          _key: "test" + i, 
          value1: i, 
          value2: "test" + i, 
          value3: "this is a test value that depends on " + i + " and nothing else" 
        });
      }
      assertEqual(n, c.count());

      for (i = 0; i < n / 2; ++i) {
        c.remove("test" + i);
      }
      assertEqual(n / 2, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.dead.deletion === n / 2) {
          break;
        }

        internal.wait(1, false);
      }

      assertTrue(fig.dead.count <= n / 2);
      assertEqual(n / 2, fig.dead.deletion);
      assertEqual(n / 2, fig.alive.count);
      assertEqual(0, fig.journals.count);
      
      for (i = n / 2; i < n; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert and remove
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveSmallPartial : function () {
      var fig, i, n = 2000;
      for (i = 0; i < n; ++i) {
        c.insert({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      assertEqual(n, c.count());

      for (i = 0; i < n / 2; ++i) {
        c.remove("test" + i);
      }
      assertEqual(n / 2, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }
      
      c.rotate();
      
      while (true) {
        fig = c.figures();
        if (fig.dead.deletion === n / 2) {
          break;
        }

        internal.wait(1, false);
      }

      assertTrue(fig.dead.count <= n / 2);
      assertEqual(n / 2, fig.dead.deletion);
      assertEqual(n / 2, fig.alive.count);
      assertEqual(0, fig.journals.count);
      
      for (i = n / 2; i < n; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test long sequences of insert
////////////////////////////////////////////////////////////////////////////////

    testInsert : function () {
      // make sure we create a few datafiles
      c.properties({ journalSize: 1 * 1024 * 1024 });

      var fig, i, n = 200000;
      for (i = 0; i < n; ++i) {
        c.insert({ 
          _key: "test" + i, 
          value1: i, 
          value2: "test" + i, 
          value3: "this is a test value that depends on " + i + " and nothing else" 
        });
      }

      assertEqual(n, c.count());

      internal.wal.flush(true, true);

      while (true) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }

        internal.wait(1, false);
      }

      assertEqual(0, fig.dead.count);
      assertEqual(0, fig.dead.deletion);
      assertTrue(fig.alive.count > 0);
      assertTrue(fig.journals.count > 0);

      c.rotate();
      fig = c.figures();

      assertEqual(0, fig.dead.count);
      assertEqual(0, fig.dead.deletion);
      assertEqual(n, fig.alive.count);
      assertEqual(0, fig.journals.count);
      assertTrue(fig.datafiles.count > 1);
     
      for (i = 0; i < n; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CompactionSuite);

return jsunity.done();

