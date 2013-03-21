////////////////////////////////////////////////////////////////////////////////
/// @brief test the key generators
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var wait = require("internal").wait;
var console = require("console");
var arangodb = require("org/arangodb");

var ArangoCollection = arangodb.ArangoCollection;
var db = arangodb.db;
var ERRORS = arangodb.errors;

// -----------------------------------------------------------------------------
// --SECTION--                                      auto-increment key generator
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: auto-increment
////////////////////////////////////////////////////////////////////////////////

function AutoIncrementSuite () {
  var cn = "UnitTestsKeyGen";

  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid offset
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidOffset : function () {
      try {
        db._create(cn, { keyOptions: { type: "autoincrement", offset: -1 } });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement1 : function () {
      try {
        db._create(cn, { keyOptions: { type: "autoincrement", increment: 0 } });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement2 : function () {
      try {
        db._create(cn, { keyOptions: { type: "autoincrement", increment: -1 } });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement2 : function () {
      try {
        db._create(cn, { keyOptions: { type: "autoincrement", increment: 9999999999999 } });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidKey1 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", allowUserKeys: false } });

      try {
        c.save({ _key: "1234" }); // no user keys allowed
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidKey2 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", allowUserKeys: true } });

      try {
        c.save({ _key: "a1234" }); // invalid key
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk1 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", offset: 12345678901234567 } });

      var options = c.properties().keyOptions;
      assertEqual("autoincrement", options.type);
      assertEqual(true, options.allowUserKeys);
      assertEqual(1, options.increment);
      assertEqual(12345678901234567, options.offset);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk2 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement" } });

      var options = c.properties().keyOptions;
      assertEqual("autoincrement", options.type);
      assertEqual(true, options.allowUserKeys);
      assertEqual(1, options.increment);
      assertEqual(0, options.offset);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk3 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", allowUserKeys: false, offset: 83, increment: 156 } });

      var options = c.properties().keyOptions;
      assertEqual("autoincrement", options.type);
      assertEqual(false, options.allowUserKeys);
      assertEqual(156, options.increment);
      assertEqual(83, options.offset);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with user key
////////////////////////////////////////////////////////////////////////////////

    testCheckUserKey : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", allowUserKeys: true } });

      var d1 = c.save({ _key: "1234" });
      assertEqual("1234", d1._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValues1 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", offset: 2, increment: 3 } });

      var d1 = c.save({ });
      assertEqual("2", d1._key);

      var d2 = c.save({ });
      assertEqual("5", d2._key);
      
      var d3 = c.save({ });
      assertEqual("8", d3._key);
      
      var d4 = c.save({ });
      assertEqual("11", d4._key);
      
      var d5 = c.save({ });
      assertEqual("14", d5._key);
      
      // create a gap
      var d6 = c.save({ _key: "100" });
      assertEqual("100", d6._key);
      
      var d7 = c.save({ });
      assertEqual("101", d7._key);

      var d8 = c.save({ });
      assertEqual("104", d8._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValues2 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", offset: 19, increment: 1 } });

      var d1 = c.save({ });
      assertEqual("19", d1._key);

      var d2 = c.save({ });
      assertEqual("20", d2._key);
      
      var d3 = c.save({ });
      assertEqual("21", d3._key);
      
      var d4 = c.save({ });
      assertEqual("22", d4._key);
      
      var d5 = c.save({ });
      assertEqual("23", d5._key);
      
      // create a gap
      var d6 = c.save({ _key: "99" });
      assertEqual("99", d6._key);
      
      var d7 = c.save({  });
      assertEqual("100", d7._key);
      
      var d8 = c.save({  });
      assertEqual("101", d8._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values after unloading etc.
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValuesUnload1 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", offset: 1, increment: 4 } });

      var d1 = c.save({ });
      assertEqual("1", d1._key);

      var d2 = c.save({ });
      assertEqual("5", d2._key);
      
      var d3 = c.save({ });
      assertEqual("9", d3._key);
      
      var d4 = c.save({ });
      assertEqual("13", d4._key);
     
      c.unload();
      console.log("waiting for collection to unload"); 
      wait(5); 
      assertEqual(ArangoCollection.STATUS_UNLOADED, c.status()); 
     
      d1 = c.save({ });
      assertEqual("17", d1._key);

      d2 = c.save({ });
      assertEqual("21", d2._key);
      
      d3 = c.save({ });
      assertEqual("25", d3._key);
      
      d4 = c.save({ });
      assertEqual("29", d4._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values after unloading etc.
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValuesUnload2 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", offset: 0, increment: 2 } });

      var d1 = c.save({ });
      assertEqual("2", d1._key);

      var d2 = c.save({ });
      assertEqual("4", d2._key);
      
      var d3 = c.save({ });
      assertEqual("6", d3._key);
      
      var d4 = c.save({ });
      assertEqual("8", d4._key);
     
      c.unload();
      console.log("waiting for collection to unload"); 
      wait(5); 
      assertEqual(ArangoCollection.STATUS_UNLOADED, c.status()); 
     
      d1 = c.save({ });
      assertEqual("10", d1._key);

      // create a gap
      d2 = c.save({ _key: "39" });
      assertEqual("39", d2._key);
      
      d3 = c.save({ });
      assertEqual("40", d3._key);
      
      // create a gap
      d4 = c.save({ _key: "19567" });
      assertEqual("19567", d4._key);
      
      c.unload();
      console.log("waiting for collection to unload"); 
      wait(5); 
      assertEqual(ArangoCollection.STATUS_UNLOADED, c.status()); 
      
      d1 = c.save({  });
      assertEqual("19568", d1._key);
      
      d2 = c.save({  });
      assertEqual("19570", d2._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValuesMixedWithUserKeys : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", offset: 0, increment: 1 } });

      var d1 = c.save({ });
      assertEqual("1", d1._key);

      var d2 = c.save({ });
      assertEqual("2", d2._key);
      
      var d3 = c.save({ _key: "100" });
      assertEqual("100", d3._key);
      
      var d4 = c.save({ _key: "3" });
      assertEqual("3", d4._key);
      
      var d5 = c.save({ _key: "99" });
      assertEqual("99", d5._key);
      
      var d6 = c.save({ });
      assertEqual("101", d6._key);

      var d7 = c.save({ });
      assertEqual("102", d7._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValuesDuplicates : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", offset: 0, increment: 1 } });

      var d1 = c.save({ });
      assertEqual("1", d1._key);

      var d2 = c.save({ });
      assertEqual("2", d2._key);
      
      try {
        c.save({ _key: "1" });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test out of keys
////////////////////////////////////////////////////////////////////////////////

    testOutOfKeys1 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", allowUserKeys: true, offset: 0, increment: 1 } });

      var d1 = c.save({ _key: "18446744073709551615" }); // still valid
      assertEqual("18446744073709551615", d1._key);
      
      try {
        c.save({ }); // should raise out of keys
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_OUT_OF_KEYS.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test out of keys
////////////////////////////////////////////////////////////////////////////////

    testOutOfKeys2 : function () {
      var c = db._create(cn, { keyOptions: { type: "autoincrement", allowUserKeys: true, offset: 0, increment: 10 } });

      var d1 = c.save({ _key: "18446744073709551615" }); // still valid
      assertEqual("18446744073709551615", d1._key);
      
      try {
        c.save({ }); // should raise out of keys
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_OUT_OF_KEYS.code, err.errorNum);
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                         traditional key generator
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: traditional key gen
////////////////////////////////////////////////////////////////////////////////

function TraditionalSuite () {
  var cn = "UnitTestsKeyGen";

  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidKey1 : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional", allowUserKeys: false } });

      try {
        c.save({ _key: "1234" }); // no user keys allowed
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidKey2 : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional", allowUserKeys: true } });

      try {
        c.save({ _key: "öä .mds 3 -6" }); // invalid key
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk1 : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional" } });

      var options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk2 : function () {
      var c = db._create(cn, { keyOptions: { } });
      
      var options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk3 : function () {
      var c = db._create(cn, { keyOptions: { allowUserKeys: false } });
      
      var options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(false, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with user key
////////////////////////////////////////////////////////////////////////////////

    testCheckUserKey : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional", allowUserKeys: true } });
      
      var options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);

      var d1 = c.save({ _key: "1234" });
      assertEqual("1234", d1._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValues : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional" } });

      var d1 = parseFloat(c.save({ })._key);
      var d2 = parseFloat(c.save({ })._key);
      var d3 = parseFloat(c.save({ })._key);
      var d4 = parseFloat(c.save({ })._key);
      var d5 = parseFloat(c.save({ })._key);

      assertTrue(d1 < d2);
      assertTrue(d2 < d3);
      assertTrue(d3 < d4);
      assertTrue(d4 < d5);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AutoIncrementSuite);
jsunity.run(TraditionalSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

