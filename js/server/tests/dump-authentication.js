/*jshint globalstrict:false, strict:false, maxlen:4000 */
/*global assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for dump/reload
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

var internal = require("internal");
var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function dumpTestSuite () {
  'use strict';
  var db = internal.db;

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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the empty collection
////////////////////////////////////////////////////////////////////////////////
    
    testEmpty : function () {
      var c = db._collection("UnitTestsDumpEmpty");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertTrue(p.waitForSync);
      assertFalse(p.isVolatile);
      assertEqual(256, p.indexBuckets);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(0, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection with many documents
////////////////////////////////////////////////////////////////////////////////
    
    testMany : function () {
      var c = db._collection("UnitTestsDumpMany");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);
      assertFalse(p.isVolatile);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(100000, c.count());

      var doc;
      var i;

      // test all documents
      for (i = 0; i < 100000; ++i) {
        doc = c.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("this is a test", doc.value2);
        assertEqual("test" + i, doc.value3);
      }

      doc = c.first();
      assertEqual("test0", doc._key);
      assertEqual(0, doc.value1);
      assertEqual("this is a test", doc.value2);
      assertEqual("test0", doc.value3);
      
      doc = c.last();
      assertEqual("test99999", doc._key);
      assertEqual(99999, doc.value1);
      assertEqual("this is a test", doc.value2);
      assertEqual("test99999", doc.value3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the edges collection
////////////////////////////////////////////////////////////////////////////////
    
    testEdges : function () {
      var c = db._collection("UnitTestsDumpEdges");
      var p = c.properties();

      assertEqual(3, c.type()); // edges
      assertFalse(p.waitForSync);
      assertFalse(p.isVolatile);

      assertEqual(2, c.getIndexes().length); // primary index + edges index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual("edge", c.getIndexes()[1].type);
      assertEqual(10, c.count());

      var doc;
      var i;

      // test all documents
      for (i = 0; i < 10; ++i) {
        doc = c.document("test" + i);
        assertEqual("test" + i, doc._key);
        assertEqual("UnitTestsDumpMany/test" + i, doc._from);
        assertEqual("UnitTestsDumpMany/test" + (i + 1), doc._to);
        assertEqual(i + "->" + (i + 1), doc.what);
      }

      doc = c.first();
      assertEqual("test0", doc._key);
      assertEqual("UnitTestsDumpMany/test0", doc._from);
      assertEqual("UnitTestsDumpMany/test1", doc._to);
      assertEqual("0->1", doc.what);
      
      doc = c.last();
      assertEqual("test9", doc._key);
      assertEqual("UnitTestsDumpMany/test9", doc._from);
      assertEqual("UnitTestsDumpMany/test10", doc._to);
      assertEqual("9->10", doc.what);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the order of documents
////////////////////////////////////////////////////////////////////////////////
    
    testOrder : function () {
      var c = db._collection("UnitTestsDumpOrder");
      var p = c.properties();

      assertEqual(2, c.type()); // document
      assertFalse(p.waitForSync);
      assertFalse(p.isVolatile);
      assertEqual(16, p.indexBuckets);

      assertEqual(1, c.getIndexes().length); // just primary index
      assertEqual("primary", c.getIndexes()[0].type);
      assertEqual(3, c.count());

      var doc;

      doc = c.first();
      assertEqual("three", doc._key);
      assertEqual(3, doc.value);
      assertEqual(123, doc.value2);

      doc = c.first(2)[1];
      assertEqual("two", doc._key);
      assertEqual(2, doc.value);
      assertEqual(456, doc.value2);

      doc = c.first(3)[2];
      assertEqual("one", doc._key);
      assertEqual(1, doc.value);
      assertEqual(789, doc.value2);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(dumpTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
