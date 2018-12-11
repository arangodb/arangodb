/*jshint globalstrict:false, strict:false, maxlen: 50000 */
/*global assertTrue, assertFalse, assertEqual, fail */

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

function fulltextCreateSuite () {
  'use strict';
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
/// @brief insert doc if index attribute is not present in it
////////////////////////////////////////////////////////////////////////////////

    testInsertDocumentWithoutAttribute : function () {
      var idx = c.ensureIndex({ type: "fulltext", fields: ["foo"] });
      c.insert({ _key: "test", value1: 1 });
      var doc = c.document("test");
      assertEqual(1, doc.value1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update doc if index attribute is not present in it
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocumentWithoutAttribute : function () {
      c.insert({ _key: "test", value1: 1 });
      var idx = c.ensureIndex({ type: "fulltext", fields: ["foo"] });
      c.update("test", { value2: "test" });
      var doc = c.document("test");
      assertEqual("test", doc.value2);
      assertEqual(1, doc.value1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace doc if index attribute is not present in it
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocumentWithoutAttribute : function () {
      c.insert({ _key: "test", value1: 1 });
      var idx = c.ensureIndex({ type: "fulltext", fields: ["foo"] });
      c.replace("test", { value2: "test" });
      var doc = c.document("test");
      assertEqual("test", doc.value2);
      assertFalse(doc.hasOwnProperty("value1"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove doc if index attribute is not present in it
////////////////////////////////////////////////////////////////////////////////

    testRemoveDocumentWithoutAttribute : function () {
      c.insert({ _key: "test", value1: 1 });
      var idx = c.ensureIndex({ type: "fulltext", fields: ["foo"] });
      c.remove("test");
      assertFalse(c.exists("test"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with multiple fields
////////////////////////////////////////////////////////////////////////////////

    testCreateMultipleFields: function () {
      try {
        c.ensureFulltextIndex("a", "b");
        fail();
      }
      catch (e) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexSimple : function () {
      var idx = c.ensureFulltextIndex("text");

      // don't inspect the result of the following query but make sure it does not fail
      var result = internal.db._query("RETURN FULLTEXT(" + c.name() + ", 'text', 'foo')").toArray()[0];
      assertEqual(0, result.length);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type !== "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "text" ], index.fields);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexExisting : function () {
      var idx = c.ensureFulltextIndex("textattr");

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type !== "fulltext") {
          continue;
        }
      
        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "textattr" ], index.fields);
       
        assertEqual(idx.id, c.ensureFulltextIndex("textattr").id);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexSubstrings : function () {
      var idx = c.ensureFulltextIndex("iam-an-indexed-ATTRIBUTE");
      
      // don't inspect the result of the following query but make sure it does not fail
      var result = internal.db._query("RETURN FULLTEXT(" + c.name() + ", 'iam-an-indexed-ATTRIBUTE', 'foo')").toArray()[0];
      assertEqual(0, result.length);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type !== "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "iam-an-indexed-ATTRIBUTE" ], index.fields);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexSubstringsExisting : function () {
      var idx = c.ensureFulltextIndex("iam-an-indexed-ATTRIBUTE");

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type !== "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "iam-an-indexed-ATTRIBUTE" ], index.fields);
      
        assertEqual(idx.id, c.ensureFulltextIndex("iam-an-indexed-ATTRIBUTE").id);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexSubattribute : function () {
      var idx = c.ensureFulltextIndex("a.b.c");
      
      // don't inspect the result of the following query but make sure it does not fail
      var result = internal.db._query("RETURN FULLTEXT(" + c.name() + ", 'a.b.c', 'foo')").toArray()[0];
      assertEqual(0, result.length);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type !== "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "a.b.c" ], index.fields);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateMultipleIndexes : function () {
      var idx1 = c.ensureFulltextIndex("attr1");
      var idx2 = c.ensureFulltextIndex("attr1");
      var idx3 = c.ensureFulltextIndex("attr2");

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type !== "fulltext") {
          continue;
        }

        if (index.id === idx1.id) {
          assertEqual("fulltext", index.type);
          assertEqual([ "attr1" ], index.fields);
        }
        else if (index.id === idx2.id) {
          assertEqual("fulltext", index.type);
          assertEqual([ "attr1" ], index.fields);
        }
        else if (index.id === idx3.id) {
          assertEqual("fulltext", index.type);
          assertEqual([ "attr2" ], index.fields);
        }
        else {
          fail();
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexMinLength1 : function () {
      var idx = c.ensureFulltextIndex("test", 5);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type !== "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "test" ], index.fields);
        assertEqual(5, index.minLength);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the index creation
////////////////////////////////////////////////////////////////////////////////

    testCreateIndexMinLength2 : function () {
      var idx = c.ensureFulltextIndex("test", 1);

      var indexes = c.getIndexes();
      for (var i = 0; i < indexes.length; ++i) {
        var index = indexes[i];
        if (index.type !== "fulltext") {
          continue;
        }

        assertEqual(idx.id, index.id);
        assertEqual("fulltext", index.type);
        assertEqual([ "test" ], index.fields);
        assertEqual(1, index.minLength);
        return;
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks dropping indexes
////////////////////////////////////////////////////////////////////////////////

    testDropIndexes : function () {
      var idx1 = c.ensureFulltextIndex("attr1");
      var idx2 = c.ensureFulltextIndex("attr1");
      var idx3 = c.ensureFulltextIndex("attr2");

      assertTrue(c.dropIndex(idx1));
      assertFalse(c.dropIndex(idx2)); // already deleted
      assertTrue(c.dropIndex(idx3));
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext queries
////////////////////////////////////////////////////////////////////////////////

function fulltextQuerySuite () {
  'use strict';
  var cn = "UnitTestsFulltext";
  var collection = null;
  var idx = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      idx = collection.ensureFulltextIndex("text").id;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////
    
    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief empty queries
////////////////////////////////////////////////////////////////////////////////

    testEmptyQueries: function () {
      collection.save({ text: "hello world"});

      var queries = [
        "",
        " ",
        "   ",
        "                             ",
        "\0",
        "\t  ",
        "\r\n"
      ];

      for (var i = 0; i < queries.length; ++i) {
        try {
          assertEqual(0, collection.fulltext("text", queries[i], idx).toArray().length);
          fail();
        }
        catch (e) {
          assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, e.errorNum);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief whitespace
////////////////////////////////////////////////////////////////////////////////

    testWhitespace: function () {
      var texts = [
        "some rubbish text",
        "More rubbish test data. The index should be able to handle all this.",
        "even MORE rubbish. Nevertheless this should be handled well, too."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "   some   ", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "   rubbish   ", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "\t\t\t\r\nrubbish\r\n\t", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "\rdata\r\nrubbish \rindex\t ", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "rubbish,test,data", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", " rubbish , test , data ", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", " rubbish, test, data", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "rubbish ,test ,data ", idx).toArray().length);

      assertEqual(0, collection.fulltext("text", "\rdata\r\nfuxxbau", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "  fuxxbau", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "never theless", idx).toArray().length);
    }, 
      
////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
////////////////////////////////////////////////////////////////////////////////

    testSimple: function () {
      var texts = [
        "some rubbish text",
        "More rubbish test data. The index should be able to handle all this.",
        "even MORE rubbish. Nevertheless this should be handled well, too."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "some", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "rubbish", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "text", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "More", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "test", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "data", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "The", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "index", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "should", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "be", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "able", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "to", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "handle", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "all", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "this", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "even", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "Nevertheless", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "handled", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "well", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "too", idx).toArray().length);
      
      assertEqual(0, collection.fulltext("text", "not", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "foobar", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "it", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "BANANA", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "noncontained", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "notpresent", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "Invisible", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "unAvailaBLE", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "Neverthelessy", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "dindex", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "grubbish", idx).toArray().length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief object attributes
////////////////////////////////////////////////////////////////////////////////

    testObjectAttributes: function () {
      var texts = [
        { de: "das ist müll", en: "this is rubbish" },
        { de: "das ist abfall", en: "this is trash" },
        { whatever: "this is a sentence, with whatever meaning", foobar: "readers should ignore this text" },
        { one: "value1", two: "value2" },
        { a: "duplicate", b: "duplicate", c: "duplicate" },
        { value: [ "this is ignored" ] },
        { value: { en: "rubbish", de: "müll" } },
        "foobarbaz",
        "müllmann",
        "rubbish bin"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(2, collection.fulltext("text", "rubbish", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "trash", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "müll", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:müll", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "foobarbaz", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "ignore", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "duplicate", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "sentence", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "ignored", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "pardauz", idx).toArray().length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief array attributes
////////////////////////////////////////////////////////////////////////////////

    testArrayAttributes: function () {
      var texts = [
        [ "das ist müll", "this is rubbish" ],
        [ "das ist abfall", "this is trash" ],
        [ ],
        [ "this is a sentence, with whatever meaning", "readers should ignore this text" ],
        [ "value1", "value2" ],
        [ "duplicate", "duplicate", "duplicate" ],
        [ { value: [ "this is ignored" ] } ],
        [ { value: { en: "rubbish", de: "müll" } } ],
        [ "foobarbaz" ],
        [ "müllmann" ],
        [ "rubbish bin" ]
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(2, collection.fulltext("text", "rubbish", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "trash", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "müll", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:müll", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "foobarbaz", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "ignore", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "duplicate", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "sentence", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "ignored", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "pardauz", idx).toArray().length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief logical operators
////////////////////////////////////////////////////////////////////////////////

    testLogicalOperators: function () {
      for (var i = 0; i < 100; ++i) {
        collection.save({ text: "prefix" + i });
      }

      assertEqual(1, collection.fulltext("text", "prefix1", idx).toArray().length);
      assertEqual(11, collection.fulltext("text", "prefix:prefix1", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix1,prefix2", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix1,prefix2,prefix3", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "prefix1,|prefix2,|prefix3", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix1,|prefix2,prefix3", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix1,prefix2,|prefix3", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix1,|prefix2,prefix:prefix3", idx).toArray().length);
      assertEqual(13, collection.fulltext("text", "prefix1,|prefix2,|prefix:prefix3", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:prefix1,prefix:prefix2", idx).toArray().length);
      assertEqual(22, collection.fulltext("text", "prefix:prefix1,|prefix:prefix2", idx).toArray().length);
      assertEqual(21, collection.fulltext("text", "prefix:prefix1,|prefix:prefix2,-prefix2", idx).toArray().length);
      assertEqual(20, collection.fulltext("text", "prefix:prefix1,|prefix:prefix2,-prefix2,-prefix1", idx).toArray().length);
      assertEqual(11, collection.fulltext("text", "prefix:prefix1,|prefix:prefix2,-prefix:prefix1", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:prefix1,|prefix:prefix2,-prefix:prefix", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test updates
////////////////////////////////////////////////////////////////////////////////

    testUpdates: function () {
      var d1 = collection.save({ text: "Some people like bananas, but others like tomatoes" });
      var d2 = collection.save({ text: "Several people hate oranges, but many like them" });

      assertEqual(1, collection.fulltext("text", "like,bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "like,tomatoes", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "but", idx).toArray().length);

      collection.update(d1, { text: "bananas are delicious" });
      assertEqual(1, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "delicious", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "like,bananas", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "like,tomatoes", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "but", idx).toArray().length);
      
      collection.update(d2, { text: "seems that some where still left!" });
      assertEqual(1, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "delicious", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "bananas,delicious", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "bananas,like", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "seems,left", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "oranges", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "people", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test updates
////////////////////////////////////////////////////////////////////////////////

    testUpdates2: function () {
      var d1 = collection.save({ text: "Some people like bananas, but others like tomatoes" });
      var d2 = collection.save({ text: "Several people hate oranges, but many like them" });

      assertEqual(1, collection.fulltext("text", "like,bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "like,tomatoes", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "but", idx).toArray().length);

      var d3 = collection.update(d1, { text: "bananas are delicious" });
      assertEqual(1, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "delicious", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "like,bananas", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "like,tomatoes", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "but", idx).toArray().length);
      
      var d4 = collection.update(d2, { text: "seems that some where still left!" });
      assertEqual(1, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "delicious", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "bananas,delicious", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "bananas,like", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "seems,left", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "oranges", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "people", idx).toArray().length);
      
      collection.remove(d3);
      assertEqual(0, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "delicious", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "seems,left", idx).toArray().length);
      
      collection.remove(d4);
      assertEqual(0, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "delicious", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "seems,left", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deletes
////////////////////////////////////////////////////////////////////////////////
 
    testDeletes: function () {
      var d1 = collection.save({ text: "Some people like bananas, but others like tomatoes" });
      var d2 = collection.save({ text: "Several people hate oranges, but many like them" });
      var d3 = collection.save({ text: "A totally unrelated text is inserted into the index, too" });

      assertEqual(1, collection.fulltext("text", "like,bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "like,tomatoes", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "but", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "totally,unrelated", idx).toArray().length);

      collection.remove(d1);
      assertEqual(0, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "like,bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "like", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "unrelated,text,index", idx).toArray().length);
      
      collection.remove(d3);
      assertEqual(0, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "like", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "unrelated,text,index", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "unrelated", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "text", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "index", idx).toArray().length);
      
      collection.remove(d2);
      assertEqual(0, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "like", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "oranges", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "hat", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "unrelated", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "text", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "index", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deletes
////////////////////////////////////////////////////////////////////////////////
    
    testDeletesWithCompact: function () {
      var d1 = collection.save({ text: "Some people like bananas, but others like tomatoes" });
      var d2 = collection.save({ text: "Several people hate oranges, but many like them" });
      var d3 = collection.save({ text: "A totally unrelated text is inserted into the index, too" });

      collection.remove(d1);
      assertEqual(0, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "some", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "tomatoes", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "others", idx).toArray().length);

      assertEqual(0, collection.fulltext("text", "bananas", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "some", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "tomatoes", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "others", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "several", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "index", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "unrelated,text,index", idx).toArray().length);

      collection.remove(d2);
      
      assertEqual(0, collection.fulltext("text", "several", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "oranges,hate", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "people", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "unrelated,text,index", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "index", idx).toArray().length);
      
      collection.remove(d3);
      
      assertEqual(0, collection.fulltext("text", "unrelated,text,index", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "index", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief case sensivity
////////////////////////////////////////////////////////////////////////////////

    testCaseSensitivity: function () {
      var texts = [
        "sOMe rubbiSH texT",
        "MoRe rubbish test Data. The indeX should be able to handle alL this.",
        "eveN MORE RUBbish. neverThELeSs ThIs should be hanDLEd well, ToO."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "sOmE", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "RUBBISH", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "texT", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "MORE", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "tEsT", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "Data", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "tHE", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "inDex", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "shoULd", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "bE", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "ablE", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "TO", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "hAnDlE", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "AlL", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "THis", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "eVen", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "nEvertheless", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "HandleD", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "welL", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "ToO", idx).toArray().length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief long words
////////////////////////////////////////////////////////////////////////////////

    testLongWords: function () {
      var texts = [
        // German is ideal for this
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattinsfreundinnenbesucheranlassversammlungsortausschilderungsherstellungsfabrikationsanlagenbetreiberliebhaberliebhaber",
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin",
        "autotuerendellenentfernungsfirmenmitarbeiterverguetungsbewerter",
        "Reliefpfeiler feilen reihenweise Pfeilreifen"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "AUTOTUERENDELLENentfernungsfirmenmitarbeiterverguetungsbewerter", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin", idx).toArray().length); // significance!
      assertEqual(1, collection.fulltext("text", "reliefpfeiler", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "feilen", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "reihenweise", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "pfeilreifen", idx).toArray().length);
      
      assertEqual(0, collection.fulltext("text", "pfeil", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "feile", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "feiler", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "reliefpfeilen", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "foo", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "bar", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "baz", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "pfeil", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "auto", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "it", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "not", idx).toArray().length);
      
      assertEqual(0, collection.fulltext("text", "somethingisreallywrongwiththislongwordsyouknowbetternotputthemintheindexyouneverknowwhathappensiftheresenoughmemoryforalltheindividualcharactersinthemletssee", idx).toArray().length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief search for multiple words
////////////////////////////////////////////////////////////////////////////////

    testMultipleWords: function () {
      var texts = [
        "sOMe rubbiSH texT",
        "MoRe rubbish test Data. The indeX should be able to handle alL this.",
        "eveN MORE RUBbish. neverThELeSs ThIs should be hanDLEd well, ToO."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "some,rubbish,text", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "some,rubbish", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "SOME,TEXT,RUBBISH", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "ruBBisH,TEXT,some", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "rubbish,text", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "some,text", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "more,rubbish,test,data,the,index,should,be,able,to,handle,all,this", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "even,more,rubbish,nevertheless,this,should,be,handled,well,too", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "even,too", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "even,rubbish,should,be,handled", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "more,well", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "well,this,rubbish,more", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "nevertheless,well", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "should,this,be", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "should,this", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "should,be", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "this,be", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "this,be,should", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "THIS,should", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "THIS,should,BE", idx).toArray().length);

      assertEqual(0, collection.fulltext("text", "some,data", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "nevertheless,text", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "some,rubbish,data", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "more,nevertheless,index", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "tomato,text", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "banana,too", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "apple", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesHorizontal: function () {
      collection.save({ text: "the the the the the the the their theirs them them themselves" });
      collection.save({ text: "apple banana tomato peach" });
     
      assertEqual(1, collection.fulltext("text", "the", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "them", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "themselves", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "the,their,theirs,them,themselves", idx).toArray().length);
      
      assertEqual(0, collection.fulltext("text", "she", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "thus", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "these,those", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesHorizontalMany: function () {
      var text = "";
      for (var i = 0; i < 1000; ++i) {
        text += "some random text,";
      }

      collection.save({ text: text });
      collection.save({ text: text });
     
      assertEqual(2, collection.fulltext("text", "text,random", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:rando", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "banana", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesQuery: function () {
      collection.save({ text: "the quick brown dog jumped over the lazy fox" });
      collection.save({ text: "apple banana tomato peach" });
      
      assertEqual(1, collection.fulltext("text", "the,the,the,the,the,the,the,the", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "apple,apple,apple,apple,apple,apple,apple,apple,APPLE,aPpLE", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "apple,apple,apple,apple,apple,apple,apple,apple,APPLE,aPpLE,peach", idx).toArray().length);

      assertEqual(0, collection.fulltext("text", "apple,apple,apple,apple,apple,apple,apple,apple,APPLE,aPpLE,peach,fox", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesVertical: function () {
      var rubbish = "the quick brown fox jumped over the lazy dog";
      
      for (var i = 0; i < 100; ++i) {
        collection.save({ text: rubbish });
      }
      
      assertEqual(100, collection.fulltext("text", "the", idx).toArray().length);
      assertEqual(100, collection.fulltext("text", "the,dog", idx).toArray().length);
      assertEqual(100, collection.fulltext("text", "quick,brown,dog", idx).toArray().length);
      assertEqual(100, collection.fulltext("text", "the,quick,brown,fox,jumped,over,the,lazy,dog", idx).toArray().length);
      assertEqual(100, collection.fulltext("text", "dog,lazy,the,over,jumped,fox,brown,quick,the", idx).toArray().length);
      assertEqual(100, collection.fulltext("text", "fox,over,dog", idx).toArray().length);

      assertEqual(0, collection.fulltext("text", "the,frog", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "no,cats,allowed", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "banana", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesMatrix: function () {
      var text = "";
      for (var i = 0; i < 1000; ++i) {
        text += "some random text,";
      }

      for (i = 0; i < 1000; ++i) {
        collection.save({ text: text });
      }
     
      assertEqual(1000, collection.fulltext("text", "text,random", idx).toArray().length);
      assertEqual(1000, collection.fulltext("text", "prefix:rando", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "banana", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesDocuments: function () {
      var text1 = "this is a short document text";
      var text2 = "Some longer document text is put in here just to validate whats going on";
      
      for (var i = 0; i < 2500; ++i) {
        collection.save({ text: text1 });
        collection.save({ text: text2 });
      }

      assertEqual(5000, collection.fulltext("text", "document", idx).toArray().length);
      assertEqual(5000, collection.fulltext("text", "text", idx).toArray().length);
      assertEqual(2500, collection.fulltext("text", "this", idx).toArray().length);
      assertEqual(2500, collection.fulltext("text", "some", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "banana", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate entries
////////////////////////////////////////////////////////////////////////////////
    
    testDuplicatesDocuments2: function () {
      var text1 = "this is a short document text";
      var text2 = "Some longer document text is put in here just to validate whats going on";

      var docs = [ ];
      
      for (var i = 0; i < 500; ++i) {
        docs[i] = collection.save({ text: text1 });
        collection.save({ text: text2 });
      }

      assertEqual(1000, collection.fulltext("text", "document", idx).toArray().length);
      assertEqual(1000, collection.fulltext("text", "text", idx).toArray().length);
      assertEqual(500, collection.fulltext("text", "this", idx).toArray().length);
      assertEqual(500, collection.fulltext("text", "some", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "banana", idx).toArray().length);

      for (i = 0; i < 250; ++i) {
        collection.remove(docs[i]);
      }

      assertEqual(750, collection.fulltext("text", "document", idx).toArray().length);
      assertEqual(750, collection.fulltext("text", "text", idx).toArray().length);
      assertEqual(250, collection.fulltext("text", "this", idx).toArray().length);
      assertEqual(500, collection.fulltext("text", "some", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "banana", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test all different entries
////////////////////////////////////////////////////////////////////////////////
    
    testDifferentDocuments: function () {
      var prefix = "prefix";

      for (var i = 0; i < 2500; ++i) {
        var word = prefix + i;
        collection.save({ text: word });
      }
     
      assertEqual(1, collection.fulltext("text", "prefix0", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix1", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix10", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix100", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix1000", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix2499", idx).toArray().length);

      assertEqual(0, collection.fulltext("text", "prefix00", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix2500", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix2501", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lengths
////////////////////////////////////////////////////////////////////////////////
    
    testLengths: function () {
      collection.save({ text: "somerandomstringaaaaaaaaaaaaaaaaaaaaaab"});
      collection.save({ text: "somerandomstringaaaaaaaaaaaaaaaaaaaaaabd"});
      collection.save({ text: "somerandomstringaaaaaaaaaaaaaaaaaaaaaabf"});
      collection.save({ text: "somerandomstringaaaaaaaaaaaaaaaaaaaaaabfd"});
      collection.save({ text: "somerandomstringaaaaaaaaaaaaaaaaaaaaaabc"});
      collection.save({ text: "somerandomstringaaaaaaaaaaaaaaaaaaaaaabcd"});
      collection.save({ text: "somerandomstringaaaaaaaaaaaaaaaaaaaaaabcde"});
    
      assertEqual(1, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaab", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaabd", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaabf", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaabfd", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaabc", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaabcd", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaabcde", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test similar entries
////////////////////////////////////////////////////////////////////////////////
    
    testSimilar: function () {
      var suffix = "a";
      for (var i = 0; i < 100; ++i) {
        collection.save({ text: "somerandomstring" + suffix });
        suffix += "a";
      }
    
      assertEqual(1, collection.fulltext("text", "somerandomstringa", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "somerandomstringaa", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "somerandomstringaaa", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "somerandomstringaaaa", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaa", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaa", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaa", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaaa", idx).toArray().length);
      assertEqual(77, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaaaaaa", idx).toArray().length);

      assertEqual(0, collection.fulltext("text", "foo", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "somerandomstring", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "somerandomstringb", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "somerandomstringbbbba", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "somerandomstringaaaaab", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "somerandomstringaaaaabaaaa", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "somerandomstringaaaaaaaaaaaaaaaaaaaab", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "somerandomstring0aaaa", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "somerandomstringaaaaa0", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test German Umlauts
////////////////////////////////////////////////////////////////////////////////
    
    testUmlauts: function () {
      var texts = [
        "DER MÜLLER GING IN DEN WALD UND aß den HÜHNERBÖREKBÄRENmensch",
        "der peter mag den bÖRGER",
        "der müller ging in den wald un aß den hahn",
        "der hans mag den pilz",
        "DER MÜLLER GING IN DEN WAL UND aß den HÜHNERBÖREKBÄRENmensch" 
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "der,peter", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "der,müller", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "börger", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "BÖRGER", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "bÖRGER", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "der,müller,ging,in,den,wald", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,müller,ging,in,den,wald,und,aß,den", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,müller,ging,in,den,wald,und,aß,den,hühnerbörekbärenmensch", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "der,müller,aß,den,hühnerbörekbärenmensch", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,HANS,mag,den,PILZ", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,PILZ,hans,den,MAG", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,PILZ,hans,den,MAG", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,peter,mag,den", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,peter,mag,den,bÖRGER", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,peter,mag,bÖRGER", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,peter,bÖRGER", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "der,bÖRGER", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "bÖRGER", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple languages
////////////////////////////////////////////////////////////////////////////////

    testUnicode: function () {
      var texts = [
        "big. Really big. He moment. Magrathea! - insisted Arthur, - I do you can sense no further because it doesn't fit properly. In my the denies faith, and the atmosphere beneath You are not cheap He was was his satchel. He throughout Magrathea. - He pushed a tore the ecstatic crowd. Trillian sat down the time, the existence is it? And he said, - What they don't want this airtight hatchway. - it's we you shooting people would represent their Poet Master Grunthos is in his mind.",
        "מהן יטע המקצועות חצר הַלֶּחֶם תחת זכר שבר יַד לַאֲשֶׁר בְּדָבָר ירה והנשגבים חשבונותי. קר רה עת זז. נראו מאוד לשבת מודד מעול. קם הרמות נח לא לי מעליו מספוא בז זז כר שב. . ההן ביצים מעט מלב קצת לשכרה שנסכה קלונו מנוחה רכב ונס. עבודתנו בעבודתו נִבְרֹא. שבר פרי רסן הַשְּׁטוּת וּגְבוּרָה שְׁאֵלַנִי בְּאוֹתָהּ לִי קַלִּילִים יִכָּנְסוּ מעש. חי הַ פה עם =יחידת עד אם כי",
        "Ultimo cadere chi sedete uso chiuso voluto ora. Scotendosi portartela meraviglia ore eguagliare incessante allegrezza per. Pensava maestro pungeva un le tornano ah perduta. Fianco bearmi storia soffio prende udi poteva una. Cammino fascino elisire orecchi pollici mio cui sai sul. Chi egli sino sei dita ben. Audace agonie groppa afa vai ultima dentro scossa sii. Alcuni mia blocco cerchi eterno andare pagine poi. Ed migliore di sommesso oh ai angoscia vorresti.", 
        "Νέο βάθος όλα δομές της χάσει. Μέτωπο εγώ συνάμα τρόπος και ότι όσο εφόδιο κόσμου. Προτίμηση όλη διάφορους του όλο εύθραυστη συγγραφής. Στα άρα ένα μία οποία άλλων νόημα. Ένα αποβαίνει ρεαλισμού μελετητές θεόσταλτο την. Ποντιακών και rites κοριτσάκι παπούτσια παραμύθια πει κυρ.",
        "Mody laty mnie ludu pole rury Białopiotrowiczowi. Domy puer szczypię jemy pragnął zacność czytając ojca lasy Nowa wewnątrz klasztoru. Chce nóg mego wami. Zamku stał nogą imion ludzi ustaw Białopiotrowiczem. Kwiat Niesiołowskiemu nierostrzygniony Staje brał Nauka dachu dumę Zamku Kościuszkowskie zagon. Jakowaś zapytać dwie mój sama polu uszakach obyczaje Mój. Niesiołowski książkowéj zimny mały dotychczasowa Stryj przestraszone Stolnikównie wdał śmiertelnego. Stanisława charty kapeluszach mięty bratem każda brząknął rydwan.",
        "Мелких против летают хижину тмится. Чудесам возьмет звездна Взжигай. . Податель сельские мучитель сверкает очищаясь пламенем. Увы имя меч Мое сия. Устранюсь воздушных Им от До мысленные потушатся Ко Ея терпеньем.", 
        "dotyku. Výdech spalin bude položen záplavový detekční kabely 1x UPS Newave Conceptpower DPA 5x 40kVA bude ukončen v samostatné strojovně. Samotné servery mají pouze lokalita Ústí nad zdvojenou podlahou budou zakončené GateWayí HiroLink - Monitoring rozvaděče RTN na jednotlivých záplavových zón na soustrojí resp. technologie jsou označeny SA-MKx.y. Jejich výstupem je zajištěn přestupem dat z jejich provoz. Na dveřích vylepené výstražné tabulky. Kabeláž z okruhů zálohovaných obvodů v R.MON-I. Monitoring EZS, EPS, ... možno zajistit funkčností FireWallů na strukturovanou kabeláží vedenou v měrných jímkách zapuštěných v každém racku budou zakončeny v R.MON-NrNN. Monitoring motorgenerátorů: řídící systém bude zakončena v modulu",
        "ramien mu zrejme vôbec niekto je už presne čo mám tendenciu prispôsobiť dych jej páčil, čo chce. Hmm... Včera sa mi pozdava, len dočkali, ale keďže som na uz boli u jej nezavrela. Hlava jej to ve městě nepotká, hodně mi to tí vedci pri hre, keď je tu pre Designiu. Pokiaľ viete o odbornejšie texty. Prvým z tmavých uličiek, každý to niekedy, zrovnávať krok s obrovským batohom na okraj vane a temné úmysly, tak rozmýšľam, aký som si hromady mailov, čo chcem a neraz sa pokúšal o filmovém klubu v budúcnosti rozhodne uniesť mladú maliarku (Linda Rybová), ktorú so",
        " 復讐者」. 復讐者」. 伯母さん 復讐者」. 復讐者」. 復讐者」. 復讐者」. 第九章 第五章 第六章 第七章 第八章. 復讐者」 伯母さん. 復讐者」 伯母さん. 第十一章 第十九章 第十四章 第十八章 第十三章 第十五章. 復讐者」 . 第十四章 第十一章 第十二章 第十五章 第十七章 手配書. 第十四章 手配書 第十八章 第十七章 第十六章 第十三章. 第十一章 第十三章 第十八章 第十四章 手配書. 復讐者」."
      ];

      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "נראו", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "נראו,עבודתנו", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "portartela,eterno", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "Взжигай,сверкает,терпеньем", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "Stanisława,Niesiołowski,przestraszone,szczypię", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "uniesť", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "uniesť,mladú", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test French text
////////////////////////////////////////////////////////////////////////////////

    testFrench: function () {
      var texts = [
        "La nouvelle. Elle, ni le cas de cette dame payés lire Invitation amitié voyager manger tout le sur deux. Vous timide qui peine dépenses débattons hâta résolu. Moment, toujours poli sur l'esprit est la chaleur qu'il cœurs. Downs ceux qui sont encore pleines d'esprit une sorte boules chef ainsi. Moment d'une petite demeurent non non jusqu'à animée. Way affaire peut hors de notre pays régulière pour adapter applaudi.",
        "Lui invitation bruyante avait dépêché connexion habitant de projection. D'un commun accord un danger mr grenier edward un. Détournée, comme ailleurs strictement aucun effort disposition par Stanhill. Cette femme appel le faire soupirer porte pas sentir. Vous et l'ordre demeure malgré vous. Proxénétisme bien appartenant notre nous-mêmes et certainement propre continuelle perpétuelle. Il d'ailleurs parfois d'ou ma certitude. Lain pas que cinq ou moins élevé. Tout voyager régler la littérature de la loi comment.",
        "Est au porte-monnaie essayé blagues Chine prête une carie. Petite son chemin boisé timide avait des bas de puissance. Pour indiquant parlant admis d'apprentissage de mon exercice afin volets po procuré mr il sentiments. Pour ou trois maison offre commence à prendre am. Comme dissuader joyeux surmonté sorte d'amicale il se livrait déballé. Connexion à l'altération de façon me recueillir. Difficile dans une vaste livré à l'allocation direction. Diminution utilisation altération peut mettre considérée sentiments discrétion intéressée. Un voyant faiblement escaliers suis revenu me branche pas.",
        "Contenta d'obtenir la certitude que dis-je méfie sont le jambon franchise caché. Sur la résolution affecté sur considérés comme des. Aucune pensée me mari ou colonel effets de formage. Fin montrant assis qui a vu d'ailleurs musicale adaptée fils. Contraste piano intéressés altération manger sympathiser été. Il croyait familles si aucun intérêt élégance surprendre un. Il demeura miles mauvaises une plaque afin de retard. Elle a survécu propre relation peut mettre éliminés.",
        "En complément raisonnable est favorable connexion expédiés dans résilié. Faire l'objet estime que nous avons appelé excuse père supprimer. So real cher sur plus comme celui-ci. Rire pour deux familles frais de surprendre l'ajout. Si la sincérité il à la curiosité l'organisation. En savoir prendre termes aussi. A peine mrs produit trop enlever nouvelle ancienne.",
        "Il aggravée par un de miles civile. Manière vivante avant tout mr suis en effet s'attendre. Parmi tous les joyeux son n'a pas encore elle. Vous maîtresse amener les enfants hors Dashwood. Dont le mérite se marier en vertu remplies. Dans celui-ci continue consulté personne ne les écoute. Devonshire monsieur le sexe immobile voyage Six eux-mêmes. Alors que le colonel grandement montrant s'observer honte. Demandes minutes vous régulier pour nuire est.",
        "Situation en admettant la promotion au niveau ou à être perçu. M. acuité que nous avons jusqu'à la jouissance estimable. Un lieu à la fin du feutre savoir. En savoir ne permettent solide à la tombe. Âge soupçon Middleton son attention. Principalement lit plusieurs son souhaitent. Est tellement moments de chambre de mal à. Douteux encore répondu adéquatement à l'humanité ainsi son désir. Minuter croyons que le service civil est arrivé ajouter tous. Allocation acuité à un favori empressement à vous exquise vaste.",
        "Débattre de me renvoyer un élevage-il. Gâtez événement était les paroles de son arrêt causent pas. Femme larmes qui n'est pas du monde miles ligneuse. J'aurais bien voulu être faites mutuelle sauf en réponse vigueur. Avait soigneusement cultivé l'amitié tumultueuse de connexion imprudence fils. Windows parce que le sexe de son inquiétude. Loi permettent sauvé vues collines de dix jours. Examiner attendant son passage Jour Soir procéder.",
        "Spot de venir à jamais la main comme dame rencontrer. Mépris délicate reçu deux encore avancé. Gentleman comme appartenant, il commandait l'abattement de croire en d'. En aucun poulet suis d'enroulement de manière sage. Son plaisir préservé sexe manière un nouveau comportement. Lui encore devonshire célébré en particulier. Insensible une disposition sont petitesse ressemblait repoussante.",
        "Folly veuve mots d'un des bas âge peu tous les sept ans. Si une partie raté de fait, il parc, juste à montrer. Découvert avaient-elles considérées projection qui favorable. Connaissances nécessaires jusqu'à ce assez. Refusant l'éducation départ est Dashwoods être soit un fichier. Utilisez hors la loi curiosité agréable monsieur ne veut pas déficient instantanément. Facile la vie fait l'esprit de voir a porté dix. Paroisse toute bavard peut Elinor directe de l'ancien. Jusqu'à comme signifié veuve au moins égale une action."
      ];

      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }
     
      assertEqual(1, collection.fulltext("text", "complément", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "expédiés", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "résilié", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "sincérité", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "curiosité", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:curiosité", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:sincé", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:sincé,prefix:complé", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "complément,expédiés,résilié,sincérité,curiosité", idx).toArray().length);

      assertEqual(1, collection.fulltext("text", "Dépêché", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "Détournée", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "Proxénétisme", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "perpétuelle", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:perpétuelle", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:perpé", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:Dépêché", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:Dépê", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:Dépenses", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:Départ", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "prefix:Dép", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "dépêché,Détournée,Proxénétisme,perpétuelle", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test Spanish text
////////////////////////////////////////////////////////////////////////////////

    testSpanish: function () {
      var texts = [
        "Nuevo. Ella ni caso a esa señora pagó leer Invitación amistad viajando comer todo lo que el a dos. Shy ustedes que apenas gastos debatiendo apresuró resuelto. Siempre educado momento en que es espíritu calor a los corazones. Downs esos ingeniosos aún un jefe bolas tan así. Momento un poco hasta quedarse sin ninguna animado. Camino de mayo trajo a nuestro país periódicas para adaptarse vitorearon.",
        "Él había enviado invitación bullicioso conexión habitar proyección. Por mutuo un peligro desván mr edward una. Desviado como adición esfuerzo estrictamente ninguna disposición por Stanhill. Esta mujer llamada lo hacen suspirar puerta no sentía. Usted y el orden morada pesar conseguirlo. La adquisición de lejos nuestra pertenencia a nosotros mismos y ciertamente propio perpetuo continuo. Es otra parte de mi a veces o certeza. Lain no como cinco o menos alto. Todo viajar establecer cómo la literatura ley.",
        "Se trató de chistes en bolsa china decaimiento listo un archivo. Pequeño su timidez tenía leñosa poder downs. Para que denota habla admitió aprendiendo mi ejercicio para Adquiridos pulg persianas mr lo sentimientos. Para o tres casa oferta tomado am a comenzar. Como disuadir alegre superó así de amable se entregaba sin envasar. Alteración de conexión así como me coleccionismo. Difícil entregado en extenso en subsidio dirección. Alteración poner disminución uso puede considerarse sentimientos discreción interesado. Un viendo débilmente escaleras soy yo sin ingresos rama.",
        "Contento obtener certeza desconfía más aún son jamón franqueza oculta. En la resolución no afecta a considerar de. No me pareció marido o coronel efectos formando. Shewing End sentado que vio además de musical adaptado hijo. En contraste interesados comer pianoforte alteración simpatizar fue. Él cree que si las familias no sorprender a un interés elegancia. Reposó millas equivocadas una placa tan demora. Ella puso propia relación sobrevivió podrá eliminarse."
      ];

      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }
     
      assertEqual(1, collection.fulltext("text", "disminución", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "Él", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "Invitación", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "disposición", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "había", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "señora", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "pagó", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "espíritu", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "leñosa", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "leñosa,decaimiento,china,amable", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "Él,por,lejos,ley", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "Él,un", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:pod", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:via", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "prefix:per", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:per,prefix:todo", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:señora", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:señ", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "prefix:sent", idx).toArray().length);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief prefixes
////////////////////////////////////////////////////////////////////////////////

    testPrefixes1: function () {
      var texts = [
        "Ego sum fidus. Canis sum.",
        "Ibi est Aurelia amica. Aurelia est puelle XI annos nata. Filia est.",
        "Claudia mater est.",
        "Anna est ancilla. Liberta est",
        "Flavus Germanus est servus. Coquus est.",
        "Ibi Quintus amicus est. Quintus est X annos natus.",
        "Gaius est frater magnus. Est XVIII annos natus et Bonnae miles.",
        "Aurelius pater est. Est mercator."
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "prefix:aurelia", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:aurelius", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:aureli", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:aurel", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:aure", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:AURE", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:anna", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "prefix:anno", idx).toArray().length);
      assertEqual(4, collection.fulltext("text", "prefix:ann", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:ANCILLA", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:Ancilla", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:AncillA", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:ancill", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:ancil", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:anci", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:anc", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:ma", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:fid", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:fil", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:fi", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:amic", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:amica", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:amicus", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:nata", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:natus", idx).toArray().length);
      assertEqual(3, collection.fulltext("text", "prefix:nat", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:MERCATOR", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:MERCATO", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:MERCAT", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:MERCA", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:MERC", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:MER", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:ME", idx).toArray().length);
      
      assertEqual(0, collection.fulltext("text", "prefix:pansen", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:banana", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:banan", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:bana", idx).toArray().length);
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief prefixes
////////////////////////////////////////////////////////////////////////////////

    testPrefixes2: function () {
      var texts = [
        "Flötenkröten tröten böse wörter nörgelnd",
        "Krötenbrote grölen stoßen GROßE Römermöter",
        "Löwenschützer möchten mächtige Müller ködern",
        "Differenzenquotienten goutieren gourmante Querulanten, quasi quergestreift",
        "Warum winken wichtige Winzer weinenden Wichten watschelnd winterher?",
        "Warum möchten böse wichte wenig müßige müller meiern?",
        "Loewenschuetzer moechten maechtige Mueller koedern",
        "Moechten boese wichte wenig mueller melken?"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      // single prefix
      assertEqual(1, collection.fulltext("text", "prefix:flötenkröten", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:flötenkröte", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:flöte", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:gour", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:gou", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:warum", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:müll", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:Müller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:müller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:mül", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:mü", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:müß", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:groß", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:große", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:GROßE", idx).toArray().length);
     
      // multiple search words
      assertEqual(2, collection.fulltext("text", "moechten,mueller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:moechten,mueller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:moechte,mueller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:muell,moechten", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:mueller,moechten", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "moechten,prefix:muell", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "moechten,prefix:mueller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:moechten,prefix:mueller", idx).toArray().length);

      assertEqual(2, collection.fulltext("text", "möchten,müller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:möchten,müller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "möchten,prefix:müller", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:möchten,prefix:müller", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:flöten,böse", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:müll,müßige", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:war,prefix:wichtig", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:war,prefix:wichte", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:war,prefix:wichten", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:warum,prefix:wicht", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:war,prefix:wicht", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:war,prefix:wi", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:flöte,prefix:nörgel", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:flöte,böse,wörter,prefix:nörgel", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:flöte,prefix:tröt,prefix:bös", idx).toArray().length);
      
      // prefix and non-existing words
      assertEqual(0, collection.fulltext("text", "prefix:flöte,wichtig", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:flöte,wichte", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:flöte,prefix:wicht", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:flöte,prefix:tröt,prefix:bös,GROßE", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "quasi,prefix:quer,präfix:differenz,müller", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "quasi,prefix:quer,präfix:differenz,prefix:müller", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief long prefixes
////////////////////////////////////////////////////////////////////////////////

    testLongPrefixes: function () {
      var texts = [
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattinsfreundinnenbesucheranlassversammlungsortausschilderungsherstellungsfabrikationsanlagenbetreiberliebhaberliebhaber",
        "Donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstandsvorsitzenderehegattin",
        "autotuerendellenentfernungsfirmenmitarbeiterverguetungsbewerter",
        "Dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiter",
        "Dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiterinsignifikant"
      ];
      
      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(2, collection.fulltext("text", "prefix:donau", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:donaudampfschifffahrtskapitaensmuetze", idx).toArray().length); // significance
      assertEqual(2, collection.fulltext("text", "prefix:donaudampfschifffahrtskapitaensmuetzentraeger", idx).toArray().length); // significance
      assertEqual(2, collection.fulltext("text", "prefix:donaudampfschifffahrtskapitaensmuetzentraegervereinsvorstand", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:dampf", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:dampfmaschinenfahrzeugsinspektion", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:dampfmaschinenfahrzeugsinspektionsverwaltungsstellenmitarbeiterinsignifikant", idx).toArray().length); // !!! only 40 chars are significant
      assertEqual(1, collection.fulltext("text", "prefix:autotuerendellenentfernungsfirmenmitarbeiter", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:autotuer", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:auto", idx).toArray().length);
      
      assertEqual(0, collection.fulltext("text", "prefix:somethingisreallywrongwiththislongwordsyouknowbetternotputthemintheindexyouneverknowwhathappensiftheresenoughmemoryforalltheindividualcharactersinthemletssee", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief lipsum
////////////////////////////////////////////////////////////////////////////////

    testLipsum: function () {
      collection.save({ text: "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet." });
      collection.save({ text: "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet." });

      assertEqual(2, collection.fulltext("text", "Lorem,ipsum,dolor,sit,amet", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "Lorem,aliquyam", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "gubergren,labore", idx).toArray().length);

      assertEqual(0, collection.fulltext("text", "gubergren,laborum", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "dolor,consetetur,prefix:invi", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "dolor,consetetur,prefix:invi,eirmod", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "dolor,consetetur,prefix:invi,prefix:eirmo", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:sanct,voluptua", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:accus,prefix:takima", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:accus,prefix:takeshi", idx).toArray().length);
      assertEqual(0, collection.fulltext("text", "prefix:accus,takeshi", idx).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief substrings
////////////////////////////////////////////////////////////////////////////////

    testSubstrings: function () {
      try {
        assertEqual(0, collection.fulltext("text", "substring:fi", idx).toArray().length);
        fail();
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief 4 byte sequences
////////////////////////////////////////////////////////////////////////////////

    testUnicodeSearches: function () {
      idx = collection.ensureFulltextIndex("text", 1).id;

      var texts = [
        "\u30e3\u30f3\u30da\u30fc\u30f3\u304a\u3088\u3073\u8ffd\u52a0\u60c5\u5831 \u3010\u697d\u5668\u6b73\u672b\u30bb\u30fc\u30eb\u3011\uff1a\u304a\u597d\u304d\u306aCD\u30fbDVD\u3068\u540c\u6642\u8cfc\u5165\u3067\u304a\u5f97\u306a\u30ad\u30e3\u30f3\u30da\u30fc\u30f3 \u3001 \u7279\u5178\u4ed8\u30a8\u30ec\u30ad\u30ae\u30bf\u30fc \u307b\u304b\u3001\u4eca\u5e74\u6700\u5f8c\u306e\u304a\u8cb7\u3044\u5f97\u30ad\u30e3\u30f3\u30da\u30fc\u30f3\uff0112\/16\u307e\u3067\u3002\u306a\u304a\u3001CD\u30fbDVD\u540c\u6642\u8cfc\u5165\u30ad\u30e3\u30f3\u30da\u30fc\u30f3\u306fAmazon\u30de\u30fc\u30b1\u30c3\u30c8\u30d7\u30ec\u30a4\u30b9\u306e\u5546\u54c1\u306f\u30ad\u30e3\u30f3\u30da\u30fc\u30f3\u5bfe\u8c61\u5916\u3067\u3059\u3002 \u300c\u697d\u5668\u6b73\u672b\u30bb\u30fc\u30eb\u300d\u3078 \u2605\u2606\u3010\u304a\u3059\u3059\u3081\u60c5\u5831\u3011\u30a6\u30a3\u30f3\u30bf\u30fc\u30bb\u30fc\u30eb\u30002013\/1\/4\u307e\u3067\u958b\u50ac\u4e2d\u2606\u2605\u2606 \u30bb\u30fc\u30eb\u5bfe\u8c61\u5546\u54c1\u306f\u5404\u30da\u30fc\u30b8\u304b\u3089\u30c1\u30a7\u30c3\u30af\uff1a \u6700\u592770\uff05OFF\uff01\u56fd\u5185\u76e4\u30d0\u30fc\u30b2\u30f3\uff5c \u3010ALL1,000\u5186\u3011\u4eba\u6c17\u8f38\u5165\u76e4\u30ed\u30c3\u30af\u30fb\u30dd\u30c3\u30d7\u30b9\uff5c \u3010\u8f38\u5165\u76e4\u671f\u9593\u9650\u5b9a1,000\u5186\u3011\u30af\u30ea\u30b9\u30de\u30b9CD\u30bb\u30fc\u30eb\uff5c \u3010\u4eba\u6c17\u306e\u8f38\u5165\u76e41,200\u5186\u3011\u7652\u3057\u306e\u97f3\u697d\uff5c \u3010ALL991\u5186\u3011\u58f2\u308c\u7b4b\u8f38\u5165\u76e4\uff5c",
        "\u30103\u679a\u7d44900\u5186\u307b\u304b\u3011NOT NOW MUSIC\u30bb\u30fc\u30eb\uff5c \u30102013\/1\/7\u307e\u3067\u3011\u30df\u30e5\u30fc\u30b8\u30c3\u30afDVD 2\u679a\u4ee5\u4e0a\u30675%OFF\uff01\uff5c \u3000\u203b\u30de\u30fc\u30b1\u30c3\u30c8\u30d7\u30ec\u30a4\u30b9\u306e\u5546\u54c1\u306f\u30bb\u30fc\u30eb\u5bfe\u8c61\u5916\u3068\u306a\u308a\u307e\u3059\u3002\u3010\u9650\u5b9a\u7248\/\u521d\u56de\u7248\u30fb\u7279\u5178\u306b\u3064\u3044\u3066\u3011Amazon\u30de\u30fc\u30b1\u30c3\u30c8\u30d7\u30ec\u30a4\u30b9\u306e\u51fa\u54c1\u8005\u304c\u8ca9\u58f2\u3001\u767a\u9001\u3059\u308b\u5546\u54c1\u306e\u5834\u5408\u306f\u3001\u51fa\u54c1\u8005\u306e\u30b3\u30e1\u30f3\u30c8\u3092\u3054\u78ba\u8a8d\u3044\u305f\u3060\u304f\u304b\u3001\u51fa\u54c1\u8005\u306b\u304a\u554f\u3044\u5408\u308f\u305b\u306e\u4e0a\u3067\u3054\u6ce8\u6587\u304f\u3060\u3055\u3044\u3002",
        "\u56fe\u4e66\u7b80\u4ecb \u4e9a\u9a6c\u900a\u56fe\u4e66\uff0c\u4e2d\u56fd\u6700\u5927\u7684\u7f51\u4e0a\u4e66\u5e97\u3002\u62e5\u6709\u6587\u5b66\uff0c\u7ecf\u6d4e\u7ba1\u7406\uff0c\u5c11\u513f\uff0c\u4eba\u6587\u793e\u79d1\uff0c\u751f\u6d3b\uff0c\u827a\u672f\uff0c\u79d1\u6280\uff0c\u8fdb\u53e3\u539f\u7248\uff0c\u671f\u520a\u6742\u5fd7\u7b49\u5927\u7c7b\uff0c\u6559\u6750\u6559\u8f85\u8003\u8bd5\uff0c\u5386\u53f2\uff0c\u56fd\u5b66\u53e4\u7c4d\uff0c\u6cd5\u5f8b\uff0c\u519b\u4e8b\uff0c\u5b97\u6559\uff0c\u5fc3\u7406\u5b66\uff0c\u54f2\u5b66\uff0c\u5065\u5eb7\u4e0e\u517b\u751f\uff0c\u65c5\u6e38\u4e0e\u5730\u56fe\uff0c\u5a31\u4e50\uff0c\u4e24\u6027\u5a5a\u604b\uff0c\u65f6\u5c1a\uff0c\u5bb6\u5c45\u4f11\u95f2\uff0c\u5b55\u4ea7\u80b2\u513f\uff0c\u6587\u5b66\uff0c\u5c0f\u8bf4\uff0c\u4f20\u8bb0\uff0c\u9752\u6625\u4e0e\u52a8\u6f2b\u7ed8\u672c\uff0c\u5bb6\u5ead\u767e\u79d1\uff0c\u5916\u8bed\uff0c\u5de5\u5177\u4e66\uff0c\u6559\u80b2\uff0c\u5fc3\u7406\u52b1\u5fd7\uff0c\u5fc3\u7075\u8bfb\u7269\uff0c\u5efa\u7b51\uff0c\u8ba1\u7b97\u673a\u4e0e\u7f51\u7edc\uff0c\u79d1\u5b66\u4e0e\u81ea\u7136\u7b49\u6570\u5341\u5c0f\u7c7b\u5171\u8ba1300\u591a\u4e07\u79cd\u4e2d\u5916\u56fe\u4e66\u3002",
        "\u0635\u0648\u0631\u0629 \u0648\u0641\u064a\u062f\u064a\u0648 : \u0645\u0648\u0631\u064a\u0646\u064a\u0648 \u064a\u0645\u0646\u0639 \u0627\u062f\u0627\u0646 \u0645\u0646 \u0627\u0631\u062a\u062f\u0627\u0621 \u0634\u0627\u0631\u0629 \u0627\u0644\u0642\u064a\u0627\u062f\u0629 \u0648\u064a\u0645\u0646\u062d\u0647\u0627 \u0644\u0628\u064a\u0628\u064a\n\u0641\u064a \u0644\u064a\u0644\u0629 \u0647\u0627\u062f\u0626\u0629 \u0641\u0627\u0632 \u0631\u064a\u0627\u0644 \u0645\u062f\u0631\u064a\u062f \u0628\u0633\u0647\u0648\u0644\u0629 \u0639\u0644\u0649 \u0623\u064a\u0627\u0643\u0633 \u0623\u0645\u0633\u062a\u0631\u062f\u0627\u0645 \u0648\u0633\u062c\u0644 \u0631\u0628\u0627\u0639\u064a\u0629 \u0646\u062c\u0648\u0645\u0647\u0627 \u0643\u0631\u064a\u0633\u062a\u064a\u0627\u0646\u0648 \u0631\u0648\u0646\u0627\u0644\u062f\u0648 \u060c \u0643\u0627\u0643\u0627 \u060c \u0643\u0627\u0644\u064a\u062e\u0648\u0646 \u060c \u0648\u0634\u0647\u062f\u062a \u0627\u0644\u0645\u0628\u0627\u0631\u0627\u0629 \u0627\u0631\u062a\u062f\u0627\u0621 \u0646\u062c\u0645 \u0645\u064a\u0644\u0627\u0646 \u0627\u0644\u0633\u0627\u0628\u0642 \u0631\u064a\u0643\u0627\u0631\u062f\u0648 \u0643\u0627\u0643\u0627 \u0634\u0627\u0631\u0629 \u0642\u064a\u0627\u062f\u0629 \u0627\u0644\u0641\u0631\u064a\u0642 . \u0648\u0628\u0639\u062f \u062e\u0631\u0648\u062c \u0643\u0627\u0643\u0627 \u0645\u0646 \u0627\u0644\u0645\u0628\u0627\u0631\u0627\u0629 \u0623\u0639\u0637\u0649 \u0627\u0644\u0644\u0627\u0639\u0628 \u0627\u0644\u0628\u0631\u0627\u0632\u064a\u0644\u064a \u0627\u0644\u0634\u0627\u0631\u0629 \u0644\u0643\u0627\u0631\u0641\u0627\u0644\u064a\u0648 \u0648\u0644\u0643\u0646 \u0643\u0627\u0631\u0641\u0627\u0644\u064a\u0648 \u062a\u0646\u0627\u0632\u0644 \u0639\u0646\u0647\u0627",
        "\u0627\u0644\u0645\u0632\u064a\u062f \u2190\n\u0641\u064a\u062f\u064a\u0648: \u0632\u064a\u0646\u062a \u064a\u0646\u062a\u0635\u0631 \u0639\u0644\u0649 \u0627\u0644\u0645\u064a\u0644\u0627\u0646 \u0648\u064a\u0634\u0627\u0631\u0643 \u0641\u064a \u0627\u0644\u062f\u0648\u0631\u064a \u0627\u0644\u0627\u0648\u0631\u0628\u064a\n\u062a\u0639\u0631\u0636 \u0645\u064a\u0644\u0627\u0646 \u0627\u0644\u0649 \u0627\u0644\u0647\u0632\u064a\u0645\u0629 \u0627\u0644\u062b\u0627\u0646\u064a\u0629 \u0641\u064a \u0627\u0644\u0645\u0633\u0627\u0628\u0642\u0629 \u0639\u0646\u062f\u0645\u0627 \u0627\u0633\u062a\u0642\u0628\u0644 \u0646\u0627\u062f\u064a \u0632\u064a\u0646\u062a \u0633\u0627\u0646 \u0628\u0637\u0631\u0633\u0628\u0648\u0631\u063a (1-0)\u0645\u0633\u0627\u0621 \u0627\u0644\u062b\u0644\u0627\u062b\u0627\u0621 \u0639\u0644\u0649 \u0645\u0644\u0639\u0628 \u201c\u0627\u0644\u0633\u0627\u0646 \u0633\u064a\u0631\u0648\u201d \u0636\u0645\u0646 \u0627\u0644\u062c\u0648\u0644\u0629 \u0627\u0644\u062e\u062a\u0627\u0645\u064a\u0629 \u0645\u0646 \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0629 \u0627\u0644\u062b\u0627\u0644\u062b\u0629 \u0645\u0646 \u062f\u0648\u0631\u064a \u0627\u0628\u0637\u0627\u0644",
        "\u0627\u0648\u0631\u0648\u0628\u0627 \u0644\u0643\u0631\u0629 \u0627\u0644\u0642\u062f\u0645.\u0648\u064a\u062f\u064a\u0646 \u0632\u064a\u0646\u062a \u0628\u0647\u0630\u0627 \u0627\u0644\u0641\u0648\u0632 \u0627\u0644\u0649 \u0627\u0644\u0628\u0631\u062a\u063a\u0627\u0644\u064a \u062f\u0627\u0646\u064a \u0627\u0644\u0641\u064a\u0634 \u063a\u0648\u0645\u064a\u0632 \u0627\u0644\u0630\u064a \u0633\u062c\u0644 \u0647\u062f\u0641 \u0627\u0644\u0627\u0646\u062a\u0635\u0627\u0631 \u0641\u064a ... \u0627\u0644\u0645\u0632\u064a\u062f \u2190\n\u0641\u064a\u062f\u064a\u0648: \u0631\u064a\u0627\u0644 \u0645\u062f\u0631\u064a\u062f \u064a\u0636\u0631\u0628 \u0634\u0628\u0627\u0643 \u0627\u064a\u0627\u0643\u0633 \u0628\u0631\u0628\u0627\u0639\u064a\u0629\n\u0623\u062d\u0633\u0646 \u0631\u064a\u0627\u0644 \u0645\u062f\u0631\u064a\u062f \u0636\u064a\u0627\u0641\u0629 \u0627\u064a\u0627\u0643\u0633 \u0627\u0645\u0633\u062a\u0631\u062f\u0627\u0645 \u0628\u0641\u0648\u0632\u0647 \u0639\u0644\u064a\u0647 (4-1) \u0645\u0633\u0627\u0621 \u0627\u0644\u062b\u0644\u0627\u062b\u0627\u0621 \u0639\u0644\u0649 \u0645\u0644\u0639\u0628 \u201c\u0633\u0627\u0646\u062a\u064a\u0627\u063a\u0648 \u0628\u0631\u0646\u0627\u0628\u064a\u0647\u201d \u0636\u0645\u0646 \u0627\u0644\u0645\u0631\u062d\u0644\u0629 \u0627\u0644\u0633\u0627\u062f\u0633\u0629 \u0648\u0627\u0644\u0627\u062e\u064a\u0631\u0629 \u0645\u0646 \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0629 \u0627\u0644\u0631\u0627\u0628\u0639\u0629 \u0645\u0646 \u0645\u0633\u0627\u0628\u0642\u0629",
        "\u062f\u0648\u0631\u064a \u0623\u0628\u0637\u0627\u0644 \u0627\u0648\u0631\u0648\u0628\u0627 \u0644\u0643\u0631\u0629 \u0627\u0644\u0642\u062f\u0645.\u0627\u0641\u062a\u062a\u062d \u0623\u0635\u062d\u0627\u0628 \u0627\u0644\u0623\u0631\u0636 \u0627\u0644\u062a\u0633\u062c\u064a\u0644 \u0628\u0639\u062f \u0645\u0646\u0638\u0648\u0645\u0629 \u062c\u0645\u0627\u0639\u064a\u0629 \u062e\u062a\u0645\u0647\u0627 \u0643\u0631\u064a\u0633\u062a\u064a\u0627\u0646\u0648 \u0631\u0648\u0646\u0627\u0644\u062f\u0648 \u0641\u064a \u0627\u0644\u0634\u0628\u0627\u0643 \u0639\u0646\u062f \u0627\u0644\u062f\u0642\u064a\u0642\u0629 ... \u0627\u0644\u0645\u0632\u064a\u062f \u2190\n\u0641\u064a\u062f\u064a\u0648 \u0648\u0635\u0648\u0631 \u2013 \u0627\u0644\u0633\u064a\u062a\u0649 \u064a\u0648\u062f\u0639 \u0627\u0644\u0645\u0648\u0633\u0645 \u0627\u0644\u0627\u0648\u0631\u0648\u0628\u064a \u0628\u062e\u0633\u0627\u0631\u0629 \u0645\u0646 \u062f\u0648\u0631\u062a\u0645\u0648\u0646\u062f\n  \u062e\u0633\u0631 \u0645\u0627\u0646 \u0633\u064a\u062a\u064a \u0627\u0644\u064a\u0648\u0645 \u0627\u0644\u062b\u0644\u0627\u062b\u0627\u0621 \u0627\u0645\u0627\u0645 \u0645\u0636\u064a\u0641\u0647 \u0628\u0631\u0648\u0633\u064a\u0627 \u062f\u0648\u0631\u062a\u0645\u0648\u0646\u062f \u0627\u0644\u0627\u0644\u0645\u0627\u0646\u064a \u0628\u0647\u062f\u0641 \u062f\u0648\u0646 \u0631\u062f\u060c \u0636\u0645\u0646 \u0645\u0646\u0627\u0641\u0633\u0627\u062a \u0627\u0644\u062c\u0648\u0644\u0629 \u0627\u0644\u0627\u062e\u064a\u0631\u0629 \u0645\u0646 \u062f\u0648\u0631\u064a \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0627\u062a \u0644\u0645\u0633\u0627\u0628\u0642\u0629 \u0627\u0644\u062a\u0634\u0627\u0645\u0628\u064a\u0648\u0646\u0632\u0644\u064a\u062c\u060c \u0648\u062a\u0630\u064a\u0644 \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0647 \u0628\u0631\u0635\u064a\u062f \u062b\u0644\u0627\u062b \u0646\u0642\u0627\u0637\u060c \u0648\u0641\u0642\u062f \u0641\u0631\u0635\u0629 \u0627\u0644\u062a\u0623\u0647\u0644 \u0644\u0644\u062f\u0648\u0631\u064a \u0627\u0644\u0627\u0648\u0631\u0648\u0628\u064a. \u0627\u062d\u0631\u0632 \u062f\u0648\u0631\u062a\u0645\u0648\u0646\u062f \u0647\u062f\u0641\u0647",
        "\u0627\u0644\u0648\u062d\u064a\u062f \u0641\u0649 \u0627\u0644\u062f\u0642\u064a\u0642\u0629 57 \u0639\u0646 \u0637\u0631\u064a\u0642 \u062c\u0648\u0644\u064a\u0627\u0646 \u0634\u064a\u0628\u0631\u060c ... \u0627\u0644\u0645\u0632\u064a\u062f \u2190\n  \u062a\u0634\u0643\u064a\u0644\u0629:\u0627\u0644\u064a\u0648\u0641\u064a \u064a\u062d\u0644 \u0636\u064a\u0641\u0627\u064b \u0639\u0644\u0649 \u0634\u062e\u062a\u0627\u0631 \u0645\u0646 \u0627\u062c\u0644 \u0646\u0642\u0637\u0629 \u0627\u0644\u062a\u0627\u0647\u0644\n    \u062a\u062a\u0651\u062c\u0647 \u0627\u0644\u0623\u0646\u0638\u0627\u0631 \u0625\u0644\u0649 \u0645\u0644\u0639\u0628 \u201c\u062f\u0648\u0646\u0628\u0627\u0633 \u0623\u0631\u064a\u0646\u0627 \u201d \u0645\u0633\u0627\u0621 \u0627\u0644\u0627\u0631\u0628\u0639\u0627\u0621 \u0641\u064a \u062a\u0645\u0627\u0645 \u0627\u0644\u0633\u0627\u0639\u0629 \u0627\u0644\u062d\u0627\u062f\u064a\u0629 \u0639\u0634\u0631\u0629 \u0627\u0644\u0627 \u0631\u0628\u0639 \u0628\u062a\u0648\u0642\u064a\u062a \u0645\u0643\u0629 \u0627\u0644\u0645\u0643\u0631\u0645\u0629 \u0627\u0644\u0630\u064a \u0633\u064a\u0643\u0648\u0646 \u0645\u0633\u0631\u062d\u0627\u064b \u0644\u0642\u0645\u0651\u0629 \u0627\u0644\u0645\u062c\u0645\u0648\u0639\u0629 \u0627\u0644\u062e\u0627\u0645\u0633\u0629 \u0645\u0646 \u062f\u0648\u0631\u064a \u0627\u0628\u0637\u0627\u0644 \u0627\u0648\u0631\u0648\u0628\u0627 \u0644\u0643\u0631\u0629 \u0627\u0644\u0642\u062f\u0645.\u0639\u0646\u062f\u0645\u0627 \u064a\u062d\u0644 \u0646\u0627\u062f\u064a \u0627\u0644\u064a\u0648\u0641\u0646\u062a\u0648\u0633 \u0627\u0644\u0627\u064a\u0637\u0627\u0644\u064a \u0636\u064a\u0641\u0627\u064b \u062b\u0642\u064a\u0644\u0627\u064b \u0639\u0644\u0649 \u0634\u062e\u062a\u0627\u0631 \u0627\u0644\u0623\u0648\u0643\u0631\u0627\u0646\u064a .\u064a\u062f\u062e\u0644 \u0627\u0644\u064a\u0648\u0641\u064a \u0627\u0644\u0644\u0642\u0627\u0621",
        "\ud0c0\uc774\ub294 \ubd88\uad50\uc758 \ub098\ub77c\uc774\uc790 \uc0ac\uc6d0\uc758 \ub098\ub77c\uc774\ub2e4. \uc8fc\ubbfc\uc758 95% \uc774\uc0c1\uc774 \ubd88\uad50\uc2e0\uc790\uc774\ub2e4. [1]\uc18c\uc2b9\ubd88\uad50\ub294 \ud604\ub300 \ud0c0\uc774\uc758 \ub3d9\uc77c\uc131\uacfc \uc2e0\ub150\uc758 \uc911\uc2ec\ub41c \uc704\uce58\ub97c \ucc28\uc9c0\ud558\ub294 \ud0c0\uc774\uc758 \uad6d\uad50\uc774\ub2e4. \ub2ec\ub825, \ud48d\uc2b5 \ub4f1\ub3c4 \ubd88\uad50\uc801\uc778 \uac83\uc774 \ub9ce\ub2e4. \uc2e4\uc81c\ub85c, \ud0c0\uc774\uc758 \ubd88\uad50\ub294 \uc11c\uc11c\ud788 \ubc1c\uc804\ud558\uc5ec \ub3d9\ubb3c\uc22d\ubc30\ub098 \uc870\uc0c1\uc22d\ubc30\ub85c\ubd80\ud130 \uae30\uc6d0\ub41c \uc9c0\uc5ed \uc885\uad50\ub4e4\uc744 \ud3ec\uc12d\ud558\uc600\ub2e4. \uad6d\uc655\uc744 \ube44\ub86f\ud558\uc5ec \ub0a8\uc790\ub77c\uba74 \uc77c\uc0dd\uc5d0 \uc801\uc5b4\ub3c4 \ud55c\ubc88\uc740 \uc808\uc5d0 \ub4e4\uc5b4\uac00 \uc0ad\ubc1c\ud558\uba70 3\uac1c\uc6d4 \uc815\ub3c4\uc758 \uc218\ub3c4\uacfc\uc815\uc744 \uc9c0\ub0b4\uace0 \uc624\ub294 \uac83\ub3c4 \uc758\ubb34\uc801\uc774\ub2e4. \uc774\ub978 \uc0c8\ubcbd\uc774\uba74 \ub204\ub7f0 \ubc95\ubcf5\uc744 \uac78\uce5c \ud0c1\ubc1c \uc2b9\ub824\ub4e4\uc774 \uc0ac\uc6d0\uc744 \ub098\uc11c \ud589\ub82c\uc744 \uc2dc\uc791\ud558\uace0 \uc2e0\ub3c4\ub4e4\uc740 \uc815\uc131\uc2a4\ub7fd\uac8c \uc774\ub4e4\uc5d0\uac8c \uacf5\uc591\uc744 \ubc14\uce5c\ub2e4. \ud0c0\uc774\uc2b9\ub824\ub4e4\uc740 \uc5b4\ub290 \ub098\ub77c\uc5d0\uc11c\ubcf4\ub2e4\ub3c4 \uc0ac\ud68c\uc801 \uc9c0\uc704\uac00 \ub192\ub2e4. \ud0c0\uc774\uc5d0\ub294 \uc544\ub984\ub2e4\uc6b4 \uc655\uad81\uacfc \ub9ce\uc740 \uc0ac\uc6d0\ub4e4\uc774 \uc788\ub294\ub370, \ucc28\ud06c\ub9ac \uc655\uc870\uc758 \uc218\ud638\uc0ac\uc6d0\uc73c\ub85c\uc11c \uc5d0\uba54\ub784\ub4dc \uc0ac\uc6d0\uacfc \uc218\ucf54\ud0c0\uc774 \uc911\uc2ec\ubd80\uc5d0 \uc788\ub294 \ucd5c\ub300\uc0ac\uc6d0\uc778 \uc653 \ub9c8\ud558\ud0d3, \uc720\uc11c\uae4a\uc740 \uc808 \uc653 \uc544\ub8ec \ub4f1 3\ub9cc\uc5ec\uac1c\uc5d0 \ub2ec\ud558\ub294 \ud06c\uace0 \uc791\uc740 \uc0ac\uc6d0\uc740 \uc544\ub984\ub2f5\uae30 \uadf8\uc9c0\uc5c6\ub2e4.",
        "C\u00e3i v\u1ea3 hay th\u1ec3 hi\u1ec7n s\u1ef1 t\u1ee9c gi\u1eadn l\u00e0 m\u1ed9t \u0111i\u1ec1u ki\u00eang c\u1eef trong v\u0103n h\u00f3a Th\u00e1i, v\u00e0, c\u0169ng nh\u01b0 c\u00e1c n\u1ec1n v\u0103n h\u00f3a ch\u00e2u \u00c1 kh\u00e1c, c\u1ea3m x\u00fac tr\u00ean khu\u00f4n m\u1eb7t l\u00e0 c\u1ef1c k\u1ef3 quan tr\u1ecdng. V\u00ec l\u00fd do n\u00e0y, du kh\u00e1ch c\u1ea7n \u0111\u1eb7c bi\u1ec7t ch\u00fa \u00fd tr\u00e1nh t\u1ea1o ra c\u00e1c xung \u0111\u1ed9t, th\u1ec3 hi\u1ec7n s\u1ef1 gi\u1eadn d\u1eef hay khi\u1ebfn cho m\u1ed9t ng\u01b0\u1eddi Th\u00e1i \u0111\u1ed5i n\u00e9t m\u1eb7t. S\u1ef1 kh\u00f4ng \u0111\u1ed3ng t\u00ecnh ho\u1eb7c c\u00e1c cu\u1ed9c tranh ch\u1ea5p n\u00ean \u0111\u01b0\u1ee3c gi\u1ea3i quy\u1ebft b\u1eb1ng n\u1ee5 c\u01b0\u1eddi v\u00e0 kh\u00f4ng n\u00ean c\u1ed1 tr\u00e1ch m\u1eafng \u0111\u1ed1i ph\u01b0\u01a1ng.\n\nTh\u01b0\u1eddng th\u00ec, ng\u01b0\u1eddi Th\u00e1i gi\u1ea3i quy\u1ebft s\u1ef1 b\u1ea5t \u0111\u1ed3ng, c\u00e1c l\u1ed7i nh\u1ecf hay s\u1ef1 xui x\u1ebbo b\u1eb1ng c\u00e1ch n\u00f3i Vi\u1ec7c s\u1eed d\u1ee5ng ph\u1ed5 bi\u1ebfn th\u00e0nh ng\u1eef n\u00e0y \u1edf Th\u00e1i Lan th\u1ec3 hi\u1ec7n t\u00ednh h\u1eefu \u00edch c\u1ee7a n\u00f3 v\u1edbi vai tr\u00f2 m\u1ed9t c\u00e1ch th\u1ee9c gi\u1ea3m thi\u1ec3u c\u00e1c xung \u0111\u1ed9t, c\u00e1c m\u1ed1i b\u1ea5t h\u00f2a v\u00e0 than phi\u1ec1n; khi m\u1ed9t ng\u01b0\u1eddi n\u00f3i mai pen rai th\u00ec h\u1ea7u nh\u01b0 c\u00f3 ngh\u0129a l\u00e0 s\u1ef1 vi\u1ec7c kh\u00f4ng h\u1ec1 quan tr\u1ecdng, v\u00e0 do",
        "\u0111\u00f3, c\u00f3 th\u1ec3 coi l\u00e0 kh\u00f4ng c\u00f3 s\u1ef1 va ch\u1ea1m n\u00e0o v\u00e0 kh\u00f4ng l\u00e0m ai \u0111\u1ed5i n\u00e9t m\u1eb7t c\u1ea3.\nAra Wilson th\u1ea3o lu\u1eadn v\u1ec1 phong t\u1ee5c Th\u00e1i, g\u1ed3m c\u00f3 bun khun trong cu\u1ed1n s\u00e1ch N\u1ec1n kinh t\u1ebf d\u1ef1a tr\u00ean s\u1ef1 quen bi\u1ebft \u1edf B\u0103ng C\u1ed1c\n\nM\u1ed9t phong t\u1ee5c Th\u00e1i kh\u00e1c l\u00e0 bun khun, l\u00e0 s\u1ef1 mang \u01a1n c\u00e1c \u0111\u1ea5ng sinh th\u00e0nh, c\u0169ng nh\u01b0 l\u00e0 nh\u1eefng ng\u01b0\u1eddi gi\u00e1m h\u1ed9, th\u1ea7y c\u00f4 gi\u00e1o v\u00e0 nh\u1eefng ng\u01b0\u1eddi c\u00f3 c\u00f4ng d\u01b0\u1ee1ng d\u1ee5c ch\u0103m s\u00f3c m\u00ecnh. Phong t\u1ee5c n\u00e0y g\u1ed3m nh\u1eefng t\u00ecnh c\u1ea3m v\u00e0 h\u00e0nh \u0111\u1ed9ng trong c\u00e1c m\u1ed1i quan h\u1ec7 c\u00f3 qua c\u00f3 l\u1ea1i.[5].\n\nNgo\u00e0i ra, gi\u1eabm l\u00ean \u0111\u1ed3ng b\u1ea1t Th\u00e1i c\u0169ng l\u00e0 c\u1ef1c k\u1ef3 v\u00f4 l\u1ec5 v\u00ec h\u00ecnh \u1ea3nh \u0111\u1ea7u c\u1ee7a qu\u1ed1c V\u01b0\u01a1ng c\u00f3 xu\u1ea5t hi\u1ec7n tr\u00ean ti\u1ec1n xu Th\u00e1i. Khi ng\u1ed3i trong c\u00e1c ng\u00f4i \u0111\u1ec1n ch\u00f9a, m\u1ecdi ng\u01b0\u1eddi n\u00ean tr\u00e1nh ch\u0129a ch\u00e2n v\u00e0o c\u00e1c tranh \u1ea3nh, t\u01b0\u1ee3ng \u0111\u1ee9c Ph\u1eadt. C\u00e1c mi\u1ebfu th\u1edd trong n\u01a1i \u1edf c\u1ee7a ng\u01b0\u1eddi Th\u00e1i \u0111\u01b0\u1ee3c x\u00e2y sao cho ch\u00e2n kh\u00f4ng ch\u0129a th\u1eb3ng v\u00e0o c\u00e1c bi\u1ec3u t\u01b0\u1ee3ng th\u1edd t\u1ef1- v\u00ed d\u1ee5 nh\u01b0 kh\u00f4ng \u0111\u1eb7t mi\u1ebfu th\u1edd \u0111\u1ed1i v\u1edbi gi\u01b0\u1eddng ng\u1ee7 n\u1ebfu nh\u00e0 qu\u00e1 nh\u1ecf, kh\u00f4ng c\u00f3 ch\u1ed7 kh\u00e1c \u0111\u1ec3 \u0111\u1eb7t mi\u1ebfu.",
        "Ta\u00f0 er ikki ney\u00f0ugt at vera innrita\u00f0ur fyri at taka lut, men hetta gevur t\u00e6r fleiri m\u00f8guleikar, og vit vilja \u00f3gvuliga fegin vita, hv\u00f8r t\u00fa ert.\n\nHygg eisini at ofta settum spurningum.\n\nT\u00e1 t\u00fa r\u00e6ttar eina s\u00ed\u00f0u, eru ymsar wiki kotur, sum gera ta\u00f0 l\u00e6tt hj\u00e1 t\u00e6r millum anna\u00f0 at gera undirpartar til eina grein og seta myndir inn. Hvussu hetta ver\u00f0ur gj\u00f8rt, kanst t\u00fa lesa um \u00e1 Hvussu ritstj\u00f3rni eg eina s\u00ed\u00f0u.",
        "\u10d5\u10d8\u10d9\u10d8\u10de\u10d4\u10d3\u10d8\u10d8\u10e1 \u10d2\u10d0\u10d6\u10d4\u10d7\u10d8 \u10db\u10d7\u10d0\u10d5\u10d0\u10e0\u10d8 \u10d0\u10d3\u10d2\u10d8\u10da\u10d8\u10d0 \u10e1\u10d0\u10d3\u10d0\u10ea \u10e8\u10d4\u10d2\u10d8\u10eb\u10da\u10d8\u10d0\u10d7 \u10e8\u10d4\u10d8\u10e2\u10e7\u10dd\u10d7 \u10d7\u10e3 \u10e0\u10d0 \u10ee\u10d3\u10d4\u10d1\u10d0 \u10d5\u10d8\u10d9\u10d8\u10de\u10d4\u10d3\u10d8\u10d0\u10e8\u10d8, \u10d2\u10d0\u10d8\u10d2\u10dd\u10d7 \u10e0\u10d0 \u10d0\u10e0\u10d8\u10e1 \u10d2\u10d0\u10e1\u10d0\u10d9\u10d4\u10d7\u10d4\u10d1\u10d4\u10da\u10d8, \u10e0\u10dd\u10db\u10d4\u10da\u10d8 \u10de\u10e0\u10dd\u10d4\u10e5\u10e2\u10d8\u10e1 \u10ef\u10d2\u10e3\u10e4\u10e1 \u10e8\u10d4\u10e3\u10d4\u10e0\u10d7\u10d3\u10d4\u10d7, \u10e8\u10d4\u10d8\u10e2\u10e7\u10dd\u10d7 \u10d0\u10dc \u10d0\u10ea\u10dc\u10dd\u10d1\u10dd\u10d7 \u10e1\u10ee\u10d5\u10d4\u10d1\u10e1 \u10e1\u10d8\u10d0\u10ee\u10da\u10d4\u10d4\u10d1\u10d8\u10e1\u10d0 \u10d3\u10d0 \u10db\u10d8\u10db\u10d3\u10d8\u10dc\u10d0\u10e0\u10d4 \u10db\u10dd\u10d5\u10da\u10d4\u10dc\u10d4\u10d1\u10d8\u10e1 \u10e8\u10d4\u10e1\u10d0\u10ee\u10d4\u10d1.",
        "\u10d3\u10d0\u10ee\u10db\u10d0\u10e0\u10d4\u10d1\u10d8\u10e1\u10d7\u10d5\u10d8\u10e1 \u10d2\u10d0\u10d3\u10d0\u10ee\u10d4\u10d3\u10d4\u10d7 \u10d8\u10dc\u10e1\u10e2\u10e0\u10e3\u10e5\u10ea\u10d8\u10d4\u10d1\u10d8\u10e1 \u10d2\u10d5\u10d4\u10e0\u10d3\u10e1, \u10d0\u10dc \u10d3\u10d0\u10e1\u10d5\u10d8\u10d7 \u10d9\u10d8\u10d7\u10ee\u10d5\u10d0 \u10e0\u10d4\u10d3\u10d0\u10e5\u10e2\u10dd\u10e0\u10d4\u10d1\u10d8\u10e1\u10d7\u10d5\u10d8\u10e1.\n\u10e4\u10dd\u10e0\u10e3\u10db\u10d8 \u10d5\u10d8\u10d9\u10d8\u10de\u10d4\u10d3\u10d8\u10d8\u10e1 \u10db\u10d7\u10d0\u10d5\u10d0\u10e0\u10d8 \u10d2\u10d0\u10dc\u10ee\u10d8\u10da\u10d5\u10d8\u10e1 \u10d0\u10d3\u10d2\u10d8\u10da\u10d8\u10d0, \u10e0\u10dd\u10db\u10d4\u10da\u10e8\u10d8\u10ea \u10e0\u10d0\u10db\u10d3\u10d4\u10dc\u10d8\u10db\u10d4 \u10d2\u10d0\u10dc\u10e7\u10dd\u10e4\u10d8\u10da\u10d4\u10d1\u10d0\u10d0: \u10e1\u10d8\u10d0\u10ee\u10da\u10d4\u10d4\u10d1\u10d8, \u10e2\u10d4\u10e5\u10dc\u10d8\u10d9\u10e3\u10e0 \u10e1\u10d0\u10d9\u10e3\u10d7\u10ee\u10d4\u10d1\u10d8, \u10ec\u10d8\u10dc\u10d0\u10d3\u10d0\u10d3\u10d4\u10d1\u10d4\u10d1\u10d8, \u10e3\u10d7\u10d0\u10dc\u10ee\u10db\u10dd\u10d4\u10d1\u10d0, \u10d5\u10d8\u10d9\u10d8\u10e8\u10d4\u10ee\u10d5\u10d4\u10d3\u10e0\u10d4\u10d1\u10d8, \u10d2\u10e0\u10d0\u10e4\u10d8\u10d9\u10e3\u10da \u10e1\u10d0\u10d0\u10db\u10e5\u10e0\u10dd, \u10d3\u10d0\u10ee\u10db\u10d0\u10e0\u10d4\u10d1\u10d0 \u10d3\u10d0 \u10db\u10e0\u10d0\u10d5\u10d0\u10da\u10d4\u10dc\u10dd\u10d5\u10d0\u10dc\u10d8."
      ];

      for (var i = 0; i < texts.length; ++i) {
        collection.save({ text: texts[i] });
      }

      assertEqual(1, collection.fulltext("text", "đầu", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "thẳng", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "đổi,Nền,Vương", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "Phật,prefix:tượn", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "დახმარებისთვის", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "ინსტრუქციების", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "მთავარი", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "რედაქტორებისთვის", idx).toArray().length);
      assertEqual(2, collection.fulltext("text", "prefix:მთა", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:რედაქტორები", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "조상숭배로부터", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "타이승려들은,수호사원으로서", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:타이승려,prefix:수호사원으", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:조상숭배로", idx).toArray().length);
      // TODO FIXME: re-activate this test!
      // assertEqual(1, collection.fulltext("text", "教", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:教", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "ógvuliga", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "møguleikar", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "síðu,rættar,ritstjórni", idx).toArray().length);
      assertEqual(1, collection.fulltext("text", "prefix:læt", idx).toArray().length);
    },

    testQueryingAfterDeletion: function () {
      for (let i = 0; i < 4000; ++i) {
        collection.save({ _key: "test" + i, text: "test" + i });
      }

      for (let i = 2436; i < 3473; ++i) {
        collection.remove("test" + i);
      }

      for (let i = 0; i < 4000; ++i) {
        assertEqual((i >= 2436 && i < 3473) ? 0 : 1, collection.fulltext("text", "test" + i, idx).toArray().length);
      }
    } 
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(fulltextCreateSuite);
jsunity.run(fulltextQuerySuite);

return jsunity.done();

