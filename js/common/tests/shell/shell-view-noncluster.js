/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertEqual, assertTypeOf, assertNotEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the view interface
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
/// @author Daniel H. Larkin
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var ArangoView = arangodb.ArangoView;
var testHelper = require("@arangodb/test-helper").Helper;
var db = arangodb.db;
var ERRORS = arangodb.errors;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: view
////////////////////////////////////////////////////////////////////////////////

function ViewSuite () {
  'use strict';
  return {

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (empty)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadNameEmpty : function () {
      try {
        db._createView("", "logger", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad type (none)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadTypeNone : function () {
      try {
        db._createView("abc");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad type (empty)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadTypeEmpty : function () {
      try {
        db._createView("abc", "", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad type (non-existent)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadTypeMissing : function () {
      try {
        db._createView("abc", "bogus", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (duplicate)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadNameDuplicate : function () {
      try {
        db._createView("abc", "logger", {});
        db._createView("abc", "logger", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
        var abc = db._view("abc");
        abc.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief rename (duplicate)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingRenameDuplicate : function () {
      try {
        db._createView("abc", "logger", {});
        var v = db._createView("def", "logger", {});
        v.rename("abc");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
        var abc = db._view("abc");
        abc.drop();
        var def = db._view("def");
        def.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief rename (illegal)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingRenameIllegal : function () {
      try {
        var v = db._createView("abc", "logger", {});
        v.rename("@bc!");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        var abc = db._view("abc");
        abc.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief get non-existent
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingGetMissing : function () {
      var abc = db._view("abc");
      assertEqual(abc, null);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief modify with unacceptable properties
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingModifyUnacceptable : function () {
      var abc = db._createView("abc", "logger", {});
      assertEqual(abc.name(), "abc");
      try {
        abc.properties({"bogus": "junk"});
        fail();
      }
      catch (err) {
        assertEqual(10, err.errorNum);
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, 10);
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        abc.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create a couple views and then drop them
    ////////////////////////////////////////////////////////////////////////////
    testAddDrop : function () {
      db._createView("abc", "logger", {"level": "WARN"});
      db._createView("def", "logger", {"level": "DEBUG"});
      var abc = db._view("abc");
      var def = db._view("def");
      var propA = abc.properties();
      var propD = def.properties();
      assertEqual(abc.name(), "abc");
      assertEqual(abc.type(), "logger");
      assertEqual(propA.level, "WARN");
      assertEqual(def.name(), "def");
      assertEqual(def.type(), "logger");
      assertEqual(propD.level, "DEBUG");
      abc.drop();
      def.drop();
      try {
        abc = db._view("abc");
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_VIEW_NOT_FOUND.code, err.errorNum);
      }
      try {
        def = db._view("def");
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_VIEW_NOT_FOUND.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief retrieve empty list of views
    ////////////////////////////////////////////////////////////////////////////
    testEmptyList : function () {
      var views = db._views();
      assertTrue(Array.isArray(views));
      assertEqual(views.length, 0);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief retrieve short list of views
    ////////////////////////////////////////////////////////////////////////////
    testShortList : function () {
      var abc = db._createView("abc", "logger", {"level": "WARN"});
      assertEqual(abc.name(), "abc");
      var def = db._createView("def", "logger", {"level": "DEBUG"});
      assertEqual(def.name(), "def");
      var views = db._views();
      assertTrue(Array.isArray(views));
      assertEqual(views.length, 2);
      assertEqual(abc.name(), views[0].name());
      assertEqual(def.name(), views[1].name());
      abc.drop();
      def.drop();
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief modify properties
    ////////////////////////////////////////////////////////////////////////////
    testModifyProperties : function () {
      var abc = db._createView("abc", "logger", {"level": "WARN"});
      var props = abc.properties();

      assertEqual(abc.name(), "abc");
      assertEqual(abc.type(), "logger");
      assertEqual(props.level, "WARN");

      abc.properties({"level": "TRACE"});
      abc = db._view("abc");
      props = abc.properties();

      assertEqual(abc.name(), "abc");
      assertEqual(abc.type(), "logger");
      assertEqual(props.level, "TRACE");

      abc.drop();
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief rename view
    ////////////////////////////////////////////////////////////////////////////
    testRename : function () {
      var abc = db._createView("abc", "logger", {"level": "WARN"});
      assertEqual(abc.name(), "abc");

      abc.rename("def");
      assertEqual(abc.name(), "def");

      abc.rename("def");
      assertEqual(abc.name(), "def");

      abc.rename("abc");
      assertEqual(abc.name(), "abc");

      abc.drop();
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ViewSuite);

return jsunity.done();
