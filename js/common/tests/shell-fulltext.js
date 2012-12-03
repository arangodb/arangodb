////////////////////////////////////////////////////////////////////////////////
/// @brief tests for fulltext indexes
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function fulltextTestSuite () {
  var cn = "UnitTestsFulltext";
  var c = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      c = internal.db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexSimple : function () {
      var idx = c.ensureFulltextIndex("text");

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type != "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "text" ], index.fields);
        assertEqual(false, index.indexSubstrings);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexExisting : function () {
      var idx = c.ensureFulltextIndex("textattr", false);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type != "fulltext") {
          continue;
        }
      
        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "textattr" ], index.fields);
        assertEqual(false, index.indexSubstrings);
       
        assertEqual(idx.id, c.ensureFulltextIndex("textattr", false).id);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexSubstrings : function () {
      var idx = c.ensureFulltextIndex("iam-an-indexed-ATTRIBUTE", true);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type != "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "iam-an-indexed-ATTRIBUTE" ], index.fields);
        assertEqual(true, index.indexSubstrings);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexSubstringsExsiting : function () {
      var idx = c.ensureFulltextIndex("iam-an-indexed-ATTRIBUTE", true);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type != "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "iam-an-indexed-ATTRIBUTE" ], index.fields);
        assertEqual(true, index.indexSubstrings);
      
        assertEqual(idx.id, c.ensureFulltextIndex("iam-an-indexed-ATTRIBUTE", true).id);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateMultipleIndexes : function () {
      var idx1 = c.ensureFulltextIndex("attr1", false);
      var idx2 = c.ensureFulltextIndex("attr1", true);
      var idx3 = c.ensureFulltextIndex("attr2", true);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type != "fulltext") {
          continue;
        }

        if (index.id == idx1.id) {
          assertEqual("fulltext", index.type);
          assertEqual([ "attr1" ], index.fields);
          assertEqual(false, index.indexSubstrings);
        }
        else if (index.id == idx2.id) {
          assertEqual("fulltext", index.type);
          assertEqual([ "attr1" ], index.fields);
          assertEqual(true, index.indexSubstrings);
        }
        else if (index.id == idx3.id) {
          assertEqual("fulltext", index.type);
          assertEqual([ "attr2" ], index.fields);
          assertEqual(true, index.indexSubstrings);
        }
        else {
          fail();
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks dropping indexes
////////////////////////////////////////////////////////////////////////////////

    testDropIndexes : function () {
      var idx1 = c.ensureFulltextIndex("attr1", false);
      var idx2 = c.ensureFulltextIndex("attr1", true);
      var idx3 = c.ensureFulltextIndex("attr2", true);

      assertTrue(c.dropIndex(idx1));
      assertTrue(c.dropIndex(idx2));
      assertTrue(c.dropIndex(idx3));
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(fulltextTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
