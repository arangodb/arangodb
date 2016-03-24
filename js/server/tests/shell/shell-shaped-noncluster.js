/*jshint globalstrict:false, strict:false */
/*global fail, assertFalse, assertTrue, assertEqual, assertUndefined */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the shaped json behavior
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

var arangodb = require("@arangodb");
var db = arangodb.db;
var internal = require("internal");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function DocumentShapedJsonSuite () {
  'use strict';
  var cn = "UnitTestsCollectionShaped";
  var c;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);

      for (var i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i,
                 value: i,
                 text: "Test" + i,
                 values: [ i ],
                 one: { two: { three: [ 1 ] } } });
      }

      // wait until the documents are actually shaped json
      internal.wal.flush(true, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief save a date object
////////////////////////////////////////////////////////////////////////////////

    testDate : function () {
      var dt = new Date();
      c.save({ _key: "date", value: dt });
      var doc = c.document("date");
      assertTrue(doc.hasOwnProperty("value"));
      assertEqual(dt.toJSON(), doc.value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief save a regexp object
////////////////////////////////////////////////////////////////////////////////

    testRegexp : function () {
      try {
        c.save({ _key: "date", regexp : /foobar/ });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief save a function object
////////////////////////////////////////////////////////////////////////////////

    testFunction : function () {
      try {
        c.save({ _key: "date", func : function () { } });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check getting keys
////////////////////////////////////////////////////////////////////////////////

    testGet : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));
        assertTrue(doc.hasOwnProperty("one"));

        assertEqual(cn + "/test" + i, doc._id);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value);
        assertEqual("Test" + i, doc.text);
        assertEqual([ i ], doc.values);
        assertEqual({ two: { three: [ 1 ] } }, doc.one);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check getting keys
////////////////////////////////////////////////////////////////////////////////

    testGetKeys : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        var keys = Object.keys(doc).sort();
        assertEqual([ "_id", "_key", "_rev", "one", "text", "value", "values" ], keys);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check updating of keys in shaped json
////////////////////////////////////////////////////////////////////////////////

    testUpdatePseudo : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        assertEqual(cn + "/test" + i, doc._id);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value);
        assertEqual("Test" + i, doc.text);
        assertEqual([ i ], doc.values);

        doc._id = "foobarbaz";
        doc._key = "meow";
        doc._rev = null;

        assertEqual("foobarbaz", doc._id);
        assertEqual("meow", doc._key);
        assertEqual(null, doc._rev);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check updating of keys in shaped json
////////////////////////////////////////////////////////////////////////////////

    testUpdateShaped1 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        doc.value = "Tester" + i;
        doc.text = 42 + i;
        doc.values.push(i);
        
        assertEqual(cn + "/test" + i, doc._id);
        assertEqual("test" + i, doc._key);
        assertEqual("Tester" + i, doc.value);
        assertEqual(42 + i, doc.text);
        assertEqual([ i, i ], doc.values);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check updating of keys in shaped json
////////////////////////////////////////////////////////////////////////////////

    testUpdateShaped2 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        assertEqual(i, doc.value);

        doc.value = 99;
        assertEqual(99, doc.value);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check updating of keys in shaped json
////////////////////////////////////////////////////////////////////////////////

    testUpdateShaped3 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        assertEqual([ i ], doc.values);

        doc.someValue = 1; // need to do this to trigger copying
        doc.values.push(42);
        assertEqual([ i, 42 ], doc.values);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check updating of keys in shaped json
////////////////////////////////////////////////////////////////////////////////

    testUpdateShapedNested1 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        assertEqual({ two: { three: [ 1 ] } }, doc.one);

        doc.one = "removing the nested structure";
        assertTrue(doc.hasOwnProperty("one"));
        assertEqual("removing the nested structure", doc.one);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check updating of keys in shaped json
////////////////////////////////////////////////////////////////////////////////

    testUpdateShapedNested2 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        assertEqual({ two: { three: [ 1 ] } }, doc.one);

        doc.someValue = 1; // need to do this to trigger copying
        doc.one.two.three = "removing the nested structure";
        assertTrue(doc.hasOwnProperty("one"));
        assertTrue(doc.one.hasOwnProperty("two"));
        assertTrue(doc.one.two.hasOwnProperty("three"));
        assertEqual("removing the nested structure", doc.one.two.three);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check updating of keys in shaped json
////////////////////////////////////////////////////////////////////////////////

    testUpdateShapedNested3 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        assertEqual({ two: { three: [ 1 ] } }, doc.one);
        doc.someValue = 1; // need to do this to trigger copying

        doc.one.two.four = 42;
        assertTrue(doc.hasOwnProperty("one"));
        assertTrue(doc.one.hasOwnProperty("two"));
        assertTrue(doc.one.two.hasOwnProperty("three"));
        assertTrue(doc.one.two.hasOwnProperty("four"));
        assertEqual([ 1 ], doc.one.two.three);
        assertEqual(42, doc.one.two.four);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check adding attributes in shaped json
////////////////////////////////////////////////////////////////////////////////

    testAddAttributes1 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        doc.thisIsAnAttribute = 99;

        assertTrue(doc.hasOwnProperty("thisIsAnAttribute"));
        assertEqual(99, doc.thisIsAnAttribute);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check adding attributes in shaped json
////////////////////////////////////////////////////////////////////////////////

    testAddAttributes2 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        doc["some attribute set now"] = "aha";

        assertTrue(doc.hasOwnProperty("some attribute set now"));
        assertEqual("aha", doc["some attribute set now"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check adding attributes in shaped json
////////////////////////////////////////////////////////////////////////////////

    testAddAttributesIndexed : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        doc[1] = "aha";

        assertTrue(doc.hasOwnProperty(1));
        assertTrue(doc.hasOwnProperty("1"));
        assertEqual("aha", doc[1]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check adding attributes in shaped json
////////////////////////////////////////////////////////////////////////////////

    testAddAttributesNested1 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        doc.someValue = 1; // need to do this to trigger copying
        doc.one.test = { foo: "bar" };
        assertTrue(doc.hasOwnProperty("one"));
        assertTrue(doc.one.hasOwnProperty("two"));
        assertTrue(doc.one.two.hasOwnProperty("three"));
        assertTrue(doc.one.hasOwnProperty("test"));
        assertEqual({ foo: "bar" }, doc.one.test);
        assertEqual({ three: [ 1 ] }, doc.one.two);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check adding attributes in shaped json
////////////////////////////////////////////////////////////////////////////////

    testAddAttributesNested2 : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        doc.something = { foo: "bar" };
        assertTrue(doc.hasOwnProperty("something"));
        assertTrue(doc.something.hasOwnProperty("foo"));
        assertEqual("bar", doc.something.foo);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionPseudoFirst : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));

        // delete pseudo-attributes first
        delete doc._key;
        assertFalse(doc.hasOwnProperty("_key"));
        
        delete doc._rev;
        assertFalse(doc.hasOwnProperty("_rev"));
        
        delete doc._id;
        assertFalse(doc.hasOwnProperty("_id"));
        
        delete doc.value;
        assertFalse(doc.hasOwnProperty("value"));
        
        delete doc.text;
        assertFalse(doc.hasOwnProperty("text"));
        
        delete doc.values;
        assertFalse(doc.hasOwnProperty("values"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of special attribute _id
////////////////////////////////////////////////////////////////////////////////

    testDeletionShapedKeyId : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));

        // delete special attribute _id
        delete doc._id;
        assertFalse(doc.hasOwnProperty("_id"));
        assertUndefined(doc._id);
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of special attributes from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionShapedKeyRev : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));

        // delete special attribute _key
        delete doc._key;
        assertFalse(doc.hasOwnProperty("_key"));
        assertUndefined(doc._key);
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));
        
        // delete special attribute _rev
        delete doc._rev;
        assertFalse(doc.hasOwnProperty("_rev"));
        assertFalse(doc.hasOwnProperty("_key"));
        assertUndefined(doc._rev);
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionShapedFirst : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));

        // delete shaped attributes first
        delete doc.value;
        assertFalse(doc.hasOwnProperty("value"));
        assertUndefined(doc.value);
        
        delete doc.text;
        assertFalse(doc.hasOwnProperty("text"));
        assertUndefined(doc.text);
        
        delete doc.values;
        assertFalse(doc.hasOwnProperty("values"));
        assertUndefined(doc.values);
        
        delete doc._key;
        assertFalse(doc.hasOwnProperty("_key"));
        assertUndefined(doc._key);
        
        delete doc._rev;
        assertFalse(doc.hasOwnProperty("_rev"));
        assertUndefined(doc._rev);
        
        delete doc._id;
        assertFalse(doc.hasOwnProperty("_id"));
        assertUndefined(doc._id);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion after deletion
////////////////////////////////////////////////////////////////////////////////

    testDeletionDeletion : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("one"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("values"));

        assertEqual([ "_id", "_key", "_rev", "one", "text", "value", "values" ], Object.keys(doc).sort());

        // delete _key
        delete doc._key;
        assertEqual([ "_id", "_rev", "one", "text", "value", "values" ], Object.keys(doc).sort());
        
        // delete text
        delete doc.text;
        assertEqual([ "_id", "_rev", "one", "value", "values" ], Object.keys(doc).sort());

        // delete _id
        delete doc._id;
        assertEqual([ "_rev", "one", "value", "values" ], Object.keys(doc).sort());

        // delete value
        delete doc.value;
        assertEqual([ "_rev", "one", "values" ], Object.keys(doc).sort());

        // delete _rev
        delete doc._rev;
        assertEqual([ "one", "values" ], Object.keys(doc).sort());

        // delete values
        delete doc.values;
        assertEqual([ "one" ], Object.keys(doc).sort());

        // delete one
        delete doc.one;
        assertEqual([ ], Object.keys(doc).sort());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionAfterUpdate : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));

        doc._key = "foobar";
        assertEqual("foobar", doc._key);
        doc._rev = 12345;
        assertEqual(12345, doc._rev);
        doc._id = "foo";
        assertEqual("foo", doc._id);

        delete doc._key;
        delete doc._rev;

        assertFalse(doc.hasOwnProperty("_rev"));
        assertFalse(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertEqual("foo", doc._id);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionSomeAttributes : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));

        delete doc._key;
        delete doc.value;

        assertFalse(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertFalse(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionIndexed : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        delete doc._key;
        doc[9] = "42!";

        assertFalse(doc.hasOwnProperty("_key"));
        assertEqual("42!", doc[9]);

        delete doc[9];
        assertFalse(doc.hasOwnProperty(9));
        assertFalse(doc.hasOwnProperty("9"));
        assertUndefined(doc[9]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionNested : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        delete doc.one.two.three;

        assertTrue(doc.hasOwnProperty("one"));
        assertTrue(doc.one.hasOwnProperty("two"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check access after deletion of documents
////////////////////////////////////////////////////////////////////////////////

    testAccessAfterDeletion : function () {
      var docs = [ ];
      for (var i = 0; i < 100; ++i) {
        docs[i] = c.document("test" + i);
      }

      c.truncate();
      c.rotate();

      internal.wait(5);
        
      for (i = 0; i < 100; ++i) {
        assertEqual(cn + "/test" + i, docs[i]._id);
        assertEqual("test" + i, docs[i]._key);
        assertEqual("Test" + i, docs[i].text);
        assertEqual([ i ], docs[i].values);
        assertEqual({ two: { three: [ 1 ] } }, docs[i].one);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check access after dropping collection
////////////////////////////////////////////////////////////////////////////////

    testAccessAfterDropping : function () {
      var docs = [ ];
      for (var i = 0; i < 100; ++i) {
        docs[i] = c.document("test" + i);
      }

      c.drop();

      internal.wait(5);
        
      for (i = 0; i < 100; ++i) {
        assertEqual(cn + "/test" + i, docs[i]._id);
        assertEqual("test" + i, docs[i]._key);
        assertEqual("Test" + i, docs[i].text);
        assertEqual([ i ], docs[i].values);
        assertEqual({ two: { three: [ 1 ] } }, docs[i].one);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function EdgeShapedJsonSuite () {
  'use strict';
  var cn = "UnitTestsCollectionShaped";
  var c;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._createEdgeCollection(cn);

      for (var i = 0; i < 100; ++i) {
        c.save(cn + "/from" + i,
               cn + "/to" + i,
               { _key: "test" + i,
                 value: i,
                 text: "Test" + i,
                 values: [ i ],
                 one: { two: { three: [ 1 ] } } });
      }

      // wait until the documents are actually shaped json
      internal.wal.flush(true, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check updating of keys in shaped json
////////////////////////////////////////////////////////////////////////////////

    testUpdatePseudo : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        assertEqual(cn + "/from" + i, doc._from);
        assertEqual(cn + "/to" + i, doc._to);

        doc._from = "foobarbaz";
        doc._to = "meow";

        assertEqual("foobarbaz", doc._from);
        assertEqual("meow", doc._to);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check adding attributes in shaped json
////////////////////////////////////////////////////////////////////////////////

    testAddAttribute : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        doc["some attribute set now"] = "aha";

        assertTrue(doc.hasOwnProperty("some attribute set now"));
        assertEqual("aha", doc["some attribute set now"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionPseudoFirst : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("_from"));
        assertTrue(doc.hasOwnProperty("_to"));

        // delete pseudo-attributes
        delete doc._from;
        assertFalse(doc.hasOwnProperty("_from"));
        
        delete doc._to;
        assertFalse(doc.hasOwnProperty("_to"));

        delete doc._key;
        assertFalse(doc.hasOwnProperty("_key"));
        
        delete doc._rev;
        assertFalse(doc.hasOwnProperty("_rev"));
        
        delete doc._id;
        assertFalse(doc.hasOwnProperty("_id"));
        
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionShapedFirst : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_from"));
        assertTrue(doc.hasOwnProperty("_to"));
        assertTrue(doc.hasOwnProperty("value"));

        // delete shaped attributes first
        delete doc.value;
        assertFalse(doc.hasOwnProperty("value"));
        assertUndefined(doc.value);
        
        delete doc._from;
        assertFalse(doc.hasOwnProperty("_from"));
        assertUndefined(doc._from);
        
        delete doc._to;
        assertFalse(doc.hasOwnProperty("_to"));
        assertUndefined(doc._to);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of special attributes from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionShapedKeyRev : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_from"));
        assertTrue(doc.hasOwnProperty("_to"));
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));

        // delete special attribute _key
        delete doc._key;
        assertFalse(doc.hasOwnProperty("_key"));
        assertUndefined(doc._key);
        assertTrue(doc.hasOwnProperty("_from"));
        assertTrue(doc.hasOwnProperty("_to"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));
        
        // delete special attribute _rev
        delete doc._rev;
        assertFalse(doc.hasOwnProperty("_rev"));
        assertFalse(doc.hasOwnProperty("_key"));
        assertUndefined(doc._rev);
        assertTrue(doc.hasOwnProperty("_from"));
        assertTrue(doc.hasOwnProperty("_to"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("values"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion of keys from shaped json
////////////////////////////////////////////////////////////////////////////////

    testDeletionAfterUpdate : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_from"));
        assertTrue(doc.hasOwnProperty("_to"));

        doc._from = "foobar";
        assertEqual("foobar", doc._from);
        doc._from = 12345;
        assertEqual(12345, doc._from);
        doc._to = "foo";
        assertEqual("foo", doc._to);

        delete doc._from;
        delete doc._to;

        assertFalse(doc.hasOwnProperty("_from"));
        assertFalse(doc.hasOwnProperty("_to"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check deletion after deletion
////////////////////////////////////////////////////////////////////////////////

    testDeletionDeletion : function () {
      for (var i = 0; i < 100; ++i) {
        var doc = c.document("test" + i);

        // initial state
        assertTrue(doc.hasOwnProperty("_from"));
        assertTrue(doc.hasOwnProperty("_to"));
        assertTrue(doc.hasOwnProperty("_key"));
        assertTrue(doc.hasOwnProperty("_rev"));
        assertTrue(doc.hasOwnProperty("_id"));
        assertTrue(doc.hasOwnProperty("one"));
        assertTrue(doc.hasOwnProperty("text"));
        assertTrue(doc.hasOwnProperty("value"));
        assertTrue(doc.hasOwnProperty("values"));

        var keys = Object.keys(doc).sort();
        assertEqual([ "_from", "_id", "_key", "_rev", "_to", "one", "text", "value", "values" ], keys);

        // delete _from
        delete doc._from;
        assertEqual([ "_id", "_key", "_rev", "_to", "one", "text", "value", "values" ], Object.keys(doc).sort());

        // delete _to
        delete doc._to;
        assertEqual([ "_id", "_key", "_rev", "one", "text", "value", "values" ], Object.keys(doc).sort());

        // delete _key
        delete doc._key;
        assertEqual([ "_id", "_rev", "one", "text", "value", "values" ], Object.keys(doc).sort());
        
        // delete text
        delete doc.text;
        assertEqual([ "_id", "_rev", "one", "value", "values" ], Object.keys(doc).sort());

        // delete _id
        delete doc._id;
        assertEqual([ "_rev", "one", "value", "values" ], Object.keys(doc).sort());

        // delete value
        delete doc.value;
        assertEqual([ "_rev", "one", "values" ], Object.keys(doc).sort());

        // delete _rev
        delete doc._rev;
        assertEqual([ "one", "values" ], Object.keys(doc).sort());

        // delete values
        delete doc.values;
        assertEqual([ "one" ], Object.keys(doc).sort());

        // delete one
        delete doc.one;
        assertEqual([ ], Object.keys(doc).sort());
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function GeoShapedJsonSuite () {
  'use strict';
  var cn = "UnitTestsCollectionShaped";
  var c;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
      c.ensureGeoIndex("lat", "lon");

      for (var i = -3; i < 3; ++i) {
        for (var j = -3; j < 3; ++j) {
          c.save({ distance: 0, lat: 40 + 0.01 * i, lon: 40 + 0.01 * j, something: "test" });
        }
      }


      // wait until the documents are actually shaped json
      internal.wal.flush(true, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief call within function with "distance" attribute
////////////////////////////////////////////////////////////////////////////////

    testDistance : function () {
      var result = db._query(
        "FOR u IN WITHIN(" + cn + ", 40.0, 40.0, 5000000, 'distance') " + 
          "SORT u.distance "+ 
          "RETURN { lat: u.lat, lon: u.lon, distance: u.distance }"
      ).toArray(); 

      // skip first result (which has a distance of 0)
      for (var i = 1; i < result.length; ++i) {
        var doc = result[i];

        assertTrue(doc.hasOwnProperty("lat"));
        assertTrue(doc.hasOwnProperty("lon"));
        assertTrue(doc.hasOwnProperty("distance"));
        assertTrue(doc.distance > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief call near function with "distance" attribute
////////////////////////////////////////////////////////////////////////////////

    testNear : function () {
      var result = db._query(
        "FOR u IN NEAR(" + cn + ", 40.0, 40.0, 5, 'something') SORT u.something " +
          "RETURN { lat: u.lat, lon: u.lon, distance: u.something }")
        .toArray(); 

      // skip first result (which has a distance of 0)
      for (var i = 1; i < result.length; ++i) {
        var doc = result[i];

        assertTrue(doc.hasOwnProperty("lat"));
        assertTrue(doc.hasOwnProperty("lon"));
        assertTrue(doc.hasOwnProperty("distance"));
        assertTrue(doc.distance >= 0);
      }
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(DocumentShapedJsonSuite);
jsunity.run(EdgeShapedJsonSuite);
jsunity.run(GeoShapedJsonSuite);

return jsunity.done();

