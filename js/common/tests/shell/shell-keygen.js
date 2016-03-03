/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the traditional key generators
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

var arangodb = require("@arangodb");
var db = arangodb.db;
var ERRORS = arangodb.errors;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: traditional key gen
////////////////////////////////////////////////////////////////////////////////

function TraditionalSuite () {
  'use strict';
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
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
                   err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);
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


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(TraditionalSuite);

return jsunity.done();

