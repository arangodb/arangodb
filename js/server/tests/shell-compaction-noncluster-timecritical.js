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
var testHelper = require("org/arangodb/test-helper").Helper;

// -----------------------------------------------------------------------------
// --SECTION--                                                        compaction
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CompactionSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief create movement of shapes
////////////////////////////////////////////////////////////////////////////////

    testShapesMovement : function () {
      var cn = "example";
      internal.db._drop(cn);

      var cn = internal.db._create(cn, { "journalSize" : 1048576 });
      var i, j;

      for (i = 0; i < 1000; ++i) {
        var doc = { _key: "old" + i, a: i, b: "test" + i, values: [ ], atts: { } }, x = { };
        for (j = 0; j < 10; ++j) { 
          doc.values.push(j);
          doc.atts["test" + i + j] = "test";
          x["foo" + i] = [ "1" ];
        }
        doc.atts.foo = x;
        cn.save(doc);
      }

      // now access the documents once, to build the shape accessors
      for (i = 0; i < 1000; ++i) {
        var doc = cn.document("old" + i);

        assertTrue(doc.hasOwnProperty("a"));
        assertEqual(i, doc.a);
        assertTrue(doc.hasOwnProperty("b"));
        assertEqual("test" + i, doc.b);
        assertTrue(doc.hasOwnProperty("values"));
        assertEqual(10, doc.values.length);
        assertTrue(doc.hasOwnProperty("atts"));
        assertEqual(11, Object.keys(doc.atts).length);

        for (j = 0; j < 10; ++j) {
          assertEqual("test", doc.atts["test" + i + j]);
        }
        for (j = 0; j < 10; ++j) {
          assertEqual([ "1" ], doc.atts.foo["foo" + i]);
        }
      }

      // fill the datafile with rubbish
      for (i = 0; i < 10000; ++i) {
        cn.save({ _key: "test" + i, value: "thequickbrownfox" });
      }

      for (i = 0; i < 10000; ++i) {
        cn.remove("test" + i);
      }

      internal.wait(7);

      assertEqual(1000, cn.count());

      // now access the "old" documents, which were probably moved
      for (i = 0; i < 1000; ++i) {
        var doc = cn.document("old" + i);

        assertTrue(doc.hasOwnProperty("a"));
        assertEqual(i, doc.a);
        assertTrue(doc.hasOwnProperty("b"));
        assertEqual("test" + i, doc.b);
        assertTrue(doc.hasOwnProperty("values"));
        assertEqual(10, doc.values.length);
        assertTrue(doc.hasOwnProperty("atts"));
        assertEqual(11, Object.keys(doc.atts).length);

        for (j = 0; j < 10; ++j) {
          assertEqual("test", doc.atts["test" + i + j]);
        }
        for (j = 0; j < 10; ++j) {
          assertEqual([ "1" ], doc.atts.foo["foo" + i]);
        }
      }

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test shapes
////////////////////////////////////////////////////////////////////////////////

    testShapes1 : function () {
      var cn = "example";
      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      var i;

      // prefill with "trash"
      for (i = 0; i < 1000; ++i) {
        c1.save({ _key: "test" + i });
      }

      // this accesses all documents, and creates shape accessors for all of them
      c1.toArray();

      c1.truncate(); 
      testHelper.rotate(c1);

      // create lots of different shapes
      for (i = 0; i < 100; ++i) { 
        var doc = { _key: "test" + i }; 
        doc["number" + i] = i; 
        doc["string" + i] = "test" + i; 
        doc["bool" + i] = (i % 2 == 0); 
        c1.save(doc); 
      } 

      // make sure compaction moves the shapes
      testHelper.rotate(c1);
      c1.truncate(); 
      internal.wait(5); 
      
      for (i = 0; i < 100; ++i) { 
        var doc = { _key: "test" + i }; 
        doc["number" + i] = i + 1; 
        doc["string" + i] = "test" + (i + 1); 
        doc["bool" + i] = (i % 2 != 0);
        c1.save(doc); 
      } 
     
      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertTrue(doc.hasOwnProperty("number" + i));
        assertTrue(doc.hasOwnProperty("string" + i));
        assertTrue(doc.hasOwnProperty("bool" + i));

        assertEqual(i + 1, doc["number" + i]);
        assertEqual("test" + (i + 1), doc["string" + i]);
        assertEqual(i % 2 != 0, doc["bool" + i]);
      } 

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test shapes
////////////////////////////////////////////////////////////////////////////////

    testShapes2 : function () {
      var cn = "example";
      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      var i;
      // prefill with "trash"
      for (i = 0; i < 1000; ++i) {
        c1.save({ _key: "test" + i });
      }
      c1.truncate(); 
      testHelper.rotate(c1);

      // create lots of different shapes
      for (i = 0; i < 100; ++i) { 
        var doc = { _key: "test" + i }; 
        doc["number" + i] = i; 
        doc["string" + i] = "test" + i; 
        doc["bool" + i] = (i % 2 == 0); 
        c1.save(doc); 
      } 
      
      c1.save({ _key: "foo", name: { first: "foo", last: "bar" } });
      c1.save({ _key: "bar", name: { first: "bar", last: "baz", middle: "foo" }, age: 22 });

      // remove most of the shapes
      for (i = 0; i < 100; ++i) { 
        c1.remove("test" + i);
      }

      // make sure compaction moves the shapes
      testHelper.rotate(c1);
      
      var doc = c1.document("foo");
      assertTrue(doc.hasOwnProperty("name"));
      assertFalse(doc.hasOwnProperty("age"));
      assertEqual({ first: "foo", last: "bar" }, doc.name);

      doc = c1.document("bar");
      assertTrue(doc.hasOwnProperty("name"));
      assertTrue(doc.hasOwnProperty("age"));
      assertEqual({ first: "bar", last: "baz", middle: "foo" }, doc.name);
      assertEqual(22, doc.age);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test shapes
////////////////////////////////////////////////////////////////////////////////

    testShapesUnloadReload : function () {
      var cn = "example";
      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      var i;
      
      // create lots of different shapes
      for (i = 0; i < 100; ++i) { 
        var doc = { _key: "test" + i }; 
        doc["number" + i] = i; 
        doc["string" + i] = "test" + i; 
        doc["bool" + i] = (i % 2 == 0); 
        c1.save(doc); 
      } 
      
      c1.save({ _key: "foo", name: { first: "foo", last: "bar" } });
      c1.save({ _key: "bar", name: { first: "bar", last: "baz", middle: "foo" }, age: 22 });
      
      // this accesses all documents, and creates shape accessors for all of them
      c1.toArray();

      // remove most of the shapes
      for (i = 0; i < 100; ++i) { 
        c1.remove("test" + i);
      }

      // make sure compaction moves the shapes
      testHelper.rotate(c1);
      // unload the collection
      
      testHelper.waitUnload(c1);

      c1 = internal.db._collection(cn);
     
      // check if documents are still there 
      var doc = c1.document("foo");
      assertTrue(doc.hasOwnProperty("name"));
      assertFalse(doc.hasOwnProperty("age"));
      assertEqual({ first: "foo", last: "bar" }, doc.name);

      doc = c1.document("bar");
      assertTrue(doc.hasOwnProperty("name"));
      assertTrue(doc.hasOwnProperty("age"));
      assertEqual({ first: "bar", last: "baz", middle: "foo" }, doc.name);
      assertEqual(22, doc.age);
      
      // create docs with already existing shapes
      for (i = 0; i < 100; ++i) { 
        var doc = { _key: "test" + i }; 
        doc["number" + i] = i; 
        doc["string" + i] = "test" + i; 
        doc["bool" + i] = (i % 2 == 0); 
        c1.save(doc); 
      } 
     
      // check if the documents work 
      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertTrue(doc.hasOwnProperty("number" + i));
        assertTrue(doc.hasOwnProperty("string" + i));
        assertTrue(doc.hasOwnProperty("bool" + i));

        assertEqual(i, doc["number" + i]);
        assertEqual("test" + i, doc["string" + i]);
        assertEqual(i % 2 == 0, doc["bool" + i]);
      } 

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test journals
////////////////////////////////////////////////////////////////////////////////

    testJournals : function () {
      var cn = "example";
      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      internal.wal.flush(true, true);

      // empty collection
      var fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(0, fig["journals"]["count"]);
      assertEqual(0, fig["datafiles"]["count"]);
      assertEqual(0, fig["compactors"]["count"]);

      c1.save({ "foo": "bar" });
      internal.wal.flush(true, true);

      fig = c1.figures();
      assertEqual(1, c1.count());
      assertEqual(1, fig["alive"]["count"]);
      assertTrue(0 < fig["alive"]["size"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertEqual(0, fig["datafiles"]["count"]);
      assertEqual(0, fig["compactors"]["count"]);
      
      testHelper.rotate(c1);
      
      fig = c1.figures();
      assertEqual(1, c1.count());
      assertEqual(1, fig["alive"]["count"]);
      assertTrue(0 < fig["alive"]["size"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(0, fig["journals"]["count"]);
      assertEqual(1, fig["datafiles"]["count"]);
      assertEqual(0, fig["compactors"]["count"]);
      
      c1.save({ "bar": "baz" });

      internal.wal.flush(true, true);
      fig = c1.figures();
      var alive = fig["alive"]["size"];
      assertEqual(2, c1.count());
      assertEqual(2, fig["alive"]["count"]);
      assertTrue(0 < alive);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertEqual(1, fig["datafiles"]["count"]);
      assertEqual(0, fig["compactors"]["count"]);

      c1.properties({ doCompact: false });
      internal.wait(1, false);

      testHelper.rotate(c1);
    
      fig = c1.figures();
      assertEqual(2, c1.count());
      assertEqual(2, fig["alive"]["count"]);
      assertEqual(alive, fig["alive"]["size"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(0, fig["journals"]["count"]);
      assertTrue(1 <= fig["datafiles"]["count"]);
      assertEqual(0, fig["compactors"]["count"]);
      
      c1.properties({ doCompact: true });
      internal.wait(1, false);

      c1.truncate();
      testHelper.rotate(c1);
     
      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["alive"]["size"]);

      c1.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test figures after truncate and rotate
////////////////////////////////////////////////////////////////////////////////

    testFiguresTruncate : function () {
      var maxWait;
      var waited;
      var cn = "example";
      var n = 400;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox. boom bang.";

      for (var i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 } );

      for (var i = 0; i < n; ++i) {
        c1.save({ _key: "test" + i, value : i, payload : payload });
      }
     
      testHelper.waitUnload(c1);  

      var fig = c1.figures();
      c1.properties({ doCompact: false });
      assertEqual(n, c1.count());
      assertEqual(n, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      c1.truncate();
      fig = c1.figures();

      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertTrue(0 <= fig["dead"]["count"]);
      assertTrue(0 <= fig["dead"]["size"]);
      assertTrue(0 <= fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 <= fig["datafiles"]["count"]);
      
      internal.wal.flush(true, true);
      c1.rotate();
      
      c1.properties({ doCompact: true });

      // wait for compactor to run
      require("console").log("waiting for compactor to run");

      // set max wait time
      if (internal.valgrind) {
        maxWait = 750;
      }
      else {
        maxWait = 90;
      }

      waited = 0;

      while (waited < maxWait) {
        internal.wait(2);
        waited += 2;
      
        fig = c1.figures();
        if (fig["dead"]["deletion"] == 0 && fig["dead"]["count"] == 0) {
          break;
        }
      }
            
      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["alive"]["size"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test figures after truncate and rotate, with compaction disabled
////////////////////////////////////////////////////////////////////////////////

    testFiguresNoCompact : function () {
      var maxWait;
      var cn = "example";
      var n = 400;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox. boom bang.";

      for (var i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576, "doCompact" : false });

      for (var i = 0; i < n; ++i) {
        c1.save({ _key: "test" + i, value : i, payload : payload });
      }
     
      testHelper.waitUnload(c1);  

      var fig = c1.figures();
      assertEqual(n, c1.count());
      assertEqual(n, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      c1.truncate();
      internal.wal.flush(true, true);
      c1.rotate();

      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["alive"]["size"]);
      assertEqual(n, fig["dead"]["count"]);
      assertTrue(0 < fig["dead"]["size"]);
      assertEqual(n, fig["dead"]["deletion"]);
      assertEqual(0, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);
      
      // wait for compactor to run
      require("console").log("waiting for compactor to run");

      // set max wait time
      if (internal.valgrind) {
        maxWait = 120;
      }
      else {
        maxWait = 15;
      }

      internal.wait(maxWait);

            
      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["alive"]["size"]);
      assertEqual(n, fig["dead"]["count"]);
      assertTrue(0 < fig["dead"]["size"]);
      assertEqual(n, fig["dead"]["deletion"]);
      assertEqual(0, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      c1.save({ "some data": true });
      fig = c1.figures();
      assertEqual(0, fig["journals"]["count"]);
      
      internal.wal.flush(true, true);

      fig = c1.figures();
      assertEqual(1, fig["journals"]["count"]);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document presence after compaction
////////////////////////////////////////////////////////////////////////////////

    testDocumentPresence : function () {
      var maxWait;
      var waited;
      var cn = "example";
      var n = 400;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox";

      for (var i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      for (var i = 0; i < n; ++i) {
        c1.save({ _key: "test" + i, value : i, payload : payload });
      }
      
      for (var i = 0; i < n; i += 2) {
        c1.remove("test" + i);
      }
     
      internal.wal.flush(true, true);
      // this will create a barrier that will block compaction
      var doc = c1.document("test1"); 

      c1.rotate();
      
      var fig = c1.figures();
      assertEqual(n / 2, c1.count());
      assertEqual(n / 2, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertTrue(200, fig["dead"]["deletion"]);
      assertTrue(0 <= fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      // trigger GC
      doc = null;
      internal.wait(0);

      // wait for compactor to run
      require("console").log("waiting for compactor to run");

      // set max wait time
      if (internal.valgrind) {
        maxWait = 750;
      }
      else {
        maxWait = 90;
      }

      waited = 0;
      var lastValue = fig["dead"]["deletion"];

      while (waited < maxWait) {
        internal.wait(5);
        waited += 5;
      
        fig = c1.figures();
        if (fig["dead"]["deletion"] == lastValue) {
          break;
        }
        lastValue = fig["dead"]["deletion"];
      }

      var doc;
      for (var i = 0; i < n; i++) {

        // will throw if document does not exist
        if (i % 2 == 0) {
          try {
            doc = c1.document("test" + i);
            fail();
          }
          catch (err) {
          }
        }
        else {
          doc = c1.document("test" + i);
        }
      }

      // trigger GC
      doc = null;
      internal.wait(0);

      c1.truncate();
      internal.wal.flush(true, true);
      c1.rotate();

      waited = 0;

      while (waited < maxWait) {
        internal.wait(2);
        waited += 2;
      
        fig = c1.figures();
        if (fig["dead"]["deletion"] == 0) {
          break;
        }
      }
      
      var fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(0, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creates documents, rotates the journal and truncates all documents
/// 
/// this will fully compact the 1st datafile (with the data), but will leave
/// the journal with the delete markers untouched
////////////////////////////////////////////////////////////////////////////////

    testCompactAfterTruncate : function () {
      var maxWait;
      var waited;
      var cn = "example";
      var n = 400;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox";

      for (var i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      for (var i = 0; i < n; ++i) {
        c1.save({ value : i, payload : payload });
      }

      internal.wal.flush(true, true);
      var fig = c1.figures();

      assertEqual(n, c1.count());
      assertEqual(n, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);
      
      // truncation will go fully into the journal...
      internal.wal.flush(true, true);
      c1.rotate();

      c1.truncate();
        
      internal.wal.flush(true, true);
      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(n, fig["dead"]["deletion"]);
      
      // wait for compactor to run
      require("console").log("waiting for compactor to run");

      // set max wait time
      if (internal.valgrind) {
        maxWait = 750;
      }
      else {
        maxWait = 90;
      }

      waited = 0;

      while (waited < maxWait) {
        internal.wait(2);
        waited += 2;
      
        fig = c1.figures();
        if (fig["dead"]["count"] == 0) {
          break;
        }
      }
            
      assertEqual(0, c1.count());
      // all alive & dead markers should be gone
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      // we should still have all the deletion markers
      assertTrue(n >= fig["dead"]["deletion"]);

      internal.db._drop(cn);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CompactionSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

