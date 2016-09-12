/*jshint globalstrict:false, strict:false, sub: true */
/*global fail, assertEqual, assertTrue, assertFalse */

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
var testHelper = require("@arangodb/test-helper").Helper;
var ArangoCollection = require("@arangodb/arango-collection").ArangoCollection;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CompactionSuite () {
  'use strict';
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief create movement of shapes
////////////////////////////////////////////////////////////////////////////////

    testShapesMovement : function () {
      var collectionName = "example";
      internal.db._drop(collectionName);

      var cn = internal.db._create(collectionName, { "journalSize" : 1048576 });
      var x, i, j, doc;

      for (i = 0; i < 1000; ++i) {
        doc = {
          _key: "old" + i,
          a: i,
          b: "test" + i,
          values: [ ],
          atts: { } };
        x = { };
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
        doc = cn.document("old" + i);

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

      internal.wait(7, false);

      assertEqual(1000, cn.count());

      // now access the "old" documents, which were probably moved
      for (i = 0; i < 1000; ++i) {
        doc = cn.document("old" + i);

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

      internal.db._drop(collectionName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test shapes
////////////////////////////////////////////////////////////////////////////////

    testShapes1 : function () {
      var cn = "example";
      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      var i, doc;

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
        doc = { _key: "test" + i }; 
        doc["number" + i] = i; 
        doc["string" + i] = "test" + i; 
        doc["bool" + i] = (i % 2 === 0); 
        c1.save(doc); 
      } 

      // make sure compaction moves the shapes
      testHelper.rotate(c1);
      c1.truncate(); 

      internal.wait(5, false); 
      
      for (i = 0; i < 100; ++i) { 
        doc = { _key: "test" + i }; 
        doc["number" + i] = i + 1; 
        doc["string" + i] = "test" + (i + 1); 
        doc["bool" + i] = (i % 2 !== 0);
        c1.save(doc); 
      } 
     
      for (i = 0; i < 100; ++i) {
        doc = c1.document("test" + i);
        assertTrue(doc.hasOwnProperty("number" + i));
        assertTrue(doc.hasOwnProperty("string" + i));
        assertTrue(doc.hasOwnProperty("bool" + i));

        assertEqual(i + 1, doc["number" + i]);
        assertEqual("test" + (i + 1), doc["string" + i]);
        assertEqual(i % 2 !== 0, doc["bool" + i]);
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

      var i, doc;
      // prefill with "trash"
      for (i = 0; i < 1000; ++i) {
        c1.save({ _key: "test" + i });
      }
      c1.truncate(); 
      testHelper.rotate(c1);

      // create lots of different shapes
      for (i = 0; i < 100; ++i) { 
        doc = { _key: "test" + i }; 
        doc["number" + i] = i; 
        doc["string" + i] = "test" + i; 
        doc["bool" + i] = (i % 2 === 0); 
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
      
      doc = c1.document("foo");
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

      var i, doc;
      
      // create lots of different shapes
      for (i = 0; i < 100; ++i) { 
        doc = { _key: "test" + i }; 
        doc["number" + i] = i; 
        doc["string" + i] = "test" + i; 
        doc["bool" + i] = (i % 2 === 0); 
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
      doc = c1.document("foo");
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
        doc = { _key: "test" + i }; 
        doc["number" + i] = i; 
        doc["string" + i] = "test" + i; 
        doc["bool" + i] = (i % 2 === 0); 
        c1.save(doc); 
      } 
     
      // check if the documents work 
      for (i = 0; i < 100; ++i) {
        doc = c1.document("test" + i);
        assertTrue(doc.hasOwnProperty("number" + i));
        assertTrue(doc.hasOwnProperty("string" + i));
        assertTrue(doc.hasOwnProperty("bool" + i));

        assertEqual(i, doc["number" + i]);
        assertEqual("test" + i, doc["string" + i]);
        assertEqual(i % 2 === 0, doc["bool" + i]);
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

      var tries = 0;
      while (++tries < 20) {
        fig = c1.figures();
        if (fig["alive"]["count"] === 1) { 
          break;
        }
        internal.wait(1, false);
      }

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

      tries = 0;
      while (++tries < 20) {
        fig = c1.figures();
      
        if (fig["alive"]["count"] === 2) {
          break;
        }
        internal.wait(1, false);
      }

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
        
      tries = 0;
      while (++tries < 20) {
        fig = c1.figures();
        if (fig["alive"]["count"] === 0) {
          break;
        }
        require("internal").wait(0.1);
      }
     
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["alive"]["size"]);

      c1.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test figures after truncate and rotate
////////////////////////////////////////////////////////////////////////////////

    testFiguresTruncate : function () {
      var i;
      var maxWait;
      var cn = "example";
      var n = 400;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox. boom bang.";

      for (i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 } );

      for (i = 0; i < n; ++i) {
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

      var tries = 0;
      while (++tries < maxWait) {
        fig = c1.figures();

        if (fig["dead"]["deletion"] === 0 && fig["dead"]["count"] === 0) {
          break;
        }

        internal.wait(1, false); 
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
      var i;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox. boom bang.";

      for ( i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576, "doCompact" : false });

      for (i = 0; i < n; ++i) {
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

      var tries = 0;
      while (++tries < 20) {
        fig = c1.figures();
        if (fig["dead"]["deletion"] === n && fig["alive"]["count"] === 0) {
          break;
        }
        internal.wait(1);
      }

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

      tries = 0;
      while (++tries < maxWait) {
        fig = c1.figures();
        if (fig["alive"]["count"] === 0) {
          break;
        }
        internal.wait(1, false);
      }
            
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

      tries = 0;
      while (++tries < maxWait) {
        fig = c1.figures();
        if (fig["journals"]["count"] === 1) {
          break;
        }
        internal.wait(1);
      }
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
      var i;
      var doc;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox";

      for (i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      for (i = 0; i < n; ++i) {
        c1.save({ _key: "test" + i, value : i, payload : payload });
      }
      
      for (i = 0; i < n; i += 2) {
        c1.remove("test" + i);
      }
     
      internal.wal.flush(true, true);
      // this will create a barrier that will block compaction
      doc = c1.document("test1"); 

      c1.rotate();

      var tries = 0, fig;

      while (++tries < 20) {
        fig = c1.figures();
        if (fig["alive"]["count"] === n / 2 && fig["dead"]["count"] === 0) {
          break;
        }
        internal.wait(1, false);
      }

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
        if (fig["dead"]["deletion"] === lastValue) {
          break;
        }
        lastValue = fig["dead"]["deletion"];
      }

      for (i = 0; i < n; i++) {

        // will throw if document does not exist
        if (i % 2 === 0) {
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
        if (fig["dead"]["deletion"] === 0 && fig["dead"]["count"] === 0) {
          break;
        }
      }
      
      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(0, fig["journals"]["count"]);
      assertEqual(0, fig["datafiles"]["count"]);

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
      var cn = "example";
      var n = 400;
      var i;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox";

      for (i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      for (i = 0; i < n; ++i) {
        c1.save({ value : i, payload : payload });
      }

      internal.wal.flush(true, true);

      var tries = 0, fig;
      while (++tries < 20) {
        fig = c1.figures();
        if (fig["alive"]["count"] === n) { 
          break;
        }
        internal.wait(1, false);
      }

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

      tries = 0;
      while (++tries < 20) {
        fig = c1.figures();
      
        if (fig["alive"]["count"] === 0 && fig["dead"]["deletion"] === n) {
          break;
        }
        internal.wait(1, false);
      }

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

      tries = 0;
      while (++tries < maxWait) {
        fig = c1.figures();

        if (fig["dead"]["count"] === 0) {
          break;
        }

        internal.wait(1, false);
      }
            
      assertEqual(0, c1.count());
      // all alive & dead markers should be gone
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      // we should still have all the deletion markers
      assertTrue(n >= fig["dead"]["deletion"]);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document presence after compaction
////////////////////////////////////////////////////////////////////////////////

    testCompactionAfterUpdateSingleDatafile : function () {
      var cn = "example";
      var n = 2000;
      var i;
      var fig;
      var payload = "the quick brown fox jumped over the lazy dog.";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      for (i = 0; i < n; ++i) {
        c1.save({ _key: "test" + i, value : i, payload : payload });
      }
      
      fig = c1.figures();
      assertEqual(0, fig.dead.count);
      
      internal.wal.flush(true, true);
      // wait for the above docs to get into the collection journal
      internal.wal.waitForCollector(cn);
      internal.wait(0.5);
      
      // update all documents so the existing revisions become irrelevant
      for (i = 0; i < n; ++i) {
        c1.update("test" + i, { payload: payload + ", isn't that nice?", updated: true });
      }
      
      fig = c1.figures();
      assertTrue(fig.dead.count > 0);

      internal.wal.flush(true, true);
      internal.wal.waitForCollector(cn);
    
      // wait for the compactor. it shouldn't compact now, but give it a chance to run
      internal.wait(5); 

      // unload collection
      c1.unload();
      c1 = null;

      while (internal.db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.wait(1, false);
      }

      // now reload and check documents
      c1 = internal.db._collection(cn);
      c1.load();

      for (i = 0; i < n; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(payload + ", isn't that nice?", doc.payload);
        assertTrue(doc.updated);
      }

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document presence after compaction
////////////////////////////////////////////////////////////////////////////////

    testCompactionAfterUpdateMultipleDatafiles : function () {
      var cn = "example";
      var n = 2000;
      var i;
      var fig;
      var payload = "the quick brown fox jumped over the lazy dog.";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 });

      for (i = 0; i < n; ++i) {
        c1.save({ _key: "test" + i, value : i, payload : payload });
      }
      
      fig = c1.figures();
      assertEqual(0, fig.dead.count);
      
      internal.wal.flush(true, true);
      // wait for the above docs to get into the collection journal
      internal.wal.waitForCollector(cn);
      internal.wait(0.5);
      
      // update all documents so the existing revisions become irrelevant
      for (i = 0; i < n; ++i) {
        c1.update("test" + i, { payload: payload + ", isn't that nice?", updated: true });
      }
      
      fig = c1.figures();
      assertTrue(fig.dead.count > 0);

      // create new datafile
      c1.rotate();

      internal.wal.flush(true, true);
      internal.wal.waitForCollector(cn);
    
      // wait for the compactor. it should compact now
      var tries = 0;
      while (tries++ < 20) {
        fig = c1.figures();
        if (fig.dead.count === 0) {
          break;
        }
        internal.wait(1);
      }

      assertEqual(0, fig.dead.count);

      // unload collection
      c1.unload();
      c1 = null;

      while (internal.db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.wait(1, false);
      }

      // now reload and check documents
      c1 = internal.db._collection(cn);
      c1.load();

      for (i = 0; i < n; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(payload + ", isn't that nice?", doc.payload);
        assertTrue(doc.updated);
      }
      
      assertEqual(0, c1.figures().dead.count);

      internal.db._drop(cn);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CompactionSuite);

return jsunity.done();

