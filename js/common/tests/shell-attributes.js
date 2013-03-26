////////////////////////////////////////////////////////////////////////////////
/// @brief test attribute naming
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

var db = arangodb.db;
var wait = require("internal").wait;

// -----------------------------------------------------------------------------
// --SECTION--                                                        attributes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function AttributesSuite () {
  var ERRORS = require("internal").errors;
  var cn = "UnitTestsCollectionAttributes";
  var c = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      c.unload();
      c.drop();
      c = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief no attributes
////////////////////////////////////////////////////////////////////////////////

    testNoAttributes : function () {
      var doc = { };
      
      var d1 = c.save(doc);
      var d2 = c.document(d1._id);
      delete d1.error;

      assertEqual(d1, d2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief empty attribute name
////////////////////////////////////////////////////////////////////////////////

    testEmptyAttribute : function () {
      var doc = { "" : "foo" };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);
      delete d1.error;

      var i;
      for (i in d2) {
        if (d2.hasOwnProperty(i)) {
          assertTrue(i !== "");
        }
      }

      assertEqual(d1, d2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief query empty attribute name
////////////////////////////////////////////////////////////////////////////////

    testQueryEmptyAttribute : function () {
      var doc = { "" : "foo" };
      c.save(doc);

      var docs = c.toArray();
      assertEqual(1, docs.length);
      var d = docs[0];

      var i;
      for (i in d) {
        if (d.hasOwnProperty(i)) {
          assertTrue(i !== "");
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief reserved attribute names
////////////////////////////////////////////////////////////////////////////////

    testReservedAttributes : function () {
      var doc = { "_id" : "foo", "_rev": "99", "_key" : "meow", "_from" : "33", "_to": "99", "_test" : false };

      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual("meow", d1._key);
      assertEqual("meow", d2._key);
      assertEqual(cn + "/meow", d1._id);
      assertEqual(cn + "/meow", d2._id);
      assertEqual(d1._rev, d2._rev);
      
      // user specified _rev value must have been ignored
      assertTrue(d1._rev !== "99");
      
      // test attributes
      var i;
      for (i in d2) {
        if (d2.hasOwnProperty(i)) {
          assertTrue(i !== "_from" && i !== "_to" && i !== "_test");
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief attribute name with special chars
////////////////////////////////////////////////////////////////////////////////

    testSpecialAttributes : function () {
      var doc = { "-meow-" : 1, "mötör" : 2, " " : 3, "\t" : 4, "\r" : 5, "\n" : 6 };
      
      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual(1, d2["-meow-"]);
      assertEqual(2, d2["mötör"]);
      assertEqual(3, d2[" "]);
      assertEqual(4, d2["\t"]);
      assertEqual(5, d2["\r"]);
      assertEqual(6, d2["\n"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shared attribute list
////////////////////////////////////////////////////////////////////////////////

    testSharedAttributesList : function () {
      var sub = { name: 1 };
      var doc = { a: [ sub, sub ] };
      
      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual(sub, d2["a"][0]);
      assertEqual(sub, d2["a"][1]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shared attribute object
////////////////////////////////////////////////////////////////////////////////

    testSharedAttributesObject : function () {
      var sub = { name: 1 };
      var doc = { a: sub, b: sub };
      
      var d1 = c.save(doc);
      var d2 = c.document(d1._id);

      assertEqual(sub, d2["a"]);
      assertEqual(sub, d2["b"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shared attribute list
////////////////////////////////////////////////////////////////////////////////

    testCyclicAttributesList : function () {
      var sub = {};
      var doc = { a: [ sub ] };

      sub.cycle = doc;
     
      try {
        c.save(doc);
        fail();
      }
      catch (err) {
        if (err && err.errorNum) {
          // we're on the server
          assertEqual(ERRORS.ERROR_ARANGO_SHAPER_FAILED.code, err.errorNum);
        }
        else {
          // we're on the client, and the JS engine just throws a generic type error
        }

      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AttributesSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
