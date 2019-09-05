/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertEqual, assertNull, assertTypeOf, assertNotEqual, fail */

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
    tearDown : function () {
      try {
        db._dropView("abc");
      } catch (_) {}
      try {
        db._dropView("def");
      } catch (_) {}
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (empty)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadNameEmpty : function () {
      try {
        db._createView("", "arangosearch", {});
        fail();
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (duplicate)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadNameDuplicate : function () {
      try {
        db._createView("abc", "arangosearch", {});
        db._createView("abc", "arangosearch", {});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
      } finally {
        var abc = db._view("abc");
        abc.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (conflict with collection)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadNameDuplicateOfCollection : function () {
      db._dropView("abc");
      db._drop("abc");
      try {
        db._create("abc");
        db._createView("abc", "arangosearch", {});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
      } finally {
        var abc = db._collection("abc");
        abc.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief get non-existent
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingGetMissing : function () {
      db._dropView("abc");
      var abc = db._view("abc");
      assertEqual(abc, null);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief modify with unacceptable properties
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingModifyUnacceptable : function () {
      db._dropView("abc");
      var abc = db._createView("abc", "arangosearch", { "consolidationIntervalMsec": 17 });
      try {
        assertEqual(abc.name(), "abc");
        assertEqual(abc.properties().consolidationIntervalMsec, 17);
        abc.properties({ "bogus": "junk", "consolidationIntervalMsec": 7 });
        assertEqual(abc.properties().consolidationIntervalMsec, 7);
      } finally {
        abc.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create a couple views and then drop them
    ////////////////////////////////////////////////////////////////////////////
    testAddDrop : function () {
      db._dropView("abc");
      db._dropView("def");
      db._createView("abc", "arangosearch", { "consolidationIntervalMsec": 10 });
      var abc = db._view("abc");
      try {
        db._createView("def", "arangosearch", { "consolidationIntervalMsec": 3 });
        var def = db._view("def");
        try {
          var propA = abc.properties();
          var propD = def.properties();
          assertEqual(abc.name(), "abc");
          assertEqual(abc.type(), "arangosearch");
          assertEqual(propA.consolidationIntervalMsec, 10);
          assertEqual(def.name(), "def");
          assertEqual(def.type(), "arangosearch");
          assertEqual(propD.consolidationIntervalMsec, 3);
        } finally {
          def.drop();
          assertNull(db._view("def"));
        }
      } finally {
        abc.drop();
        assertNull(db._view("abc"));
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief retrieve short list of views
    ////////////////////////////////////////////////////////////////////////////
    testShortList : function () {
      db._dropView("abc");
      db._dropView("def");
      var abc = db._createView("abc", "arangosearch", {});
      try {
        assertEqual(abc.name(), "abc");
        var def = db._createView("def", "arangosearch", {});
        try {
          assertEqual(def.name(), "def");
          var views = db._views();

          let expectedViews = new Set();
          expectedViews.add(abc.name());
          expectedViews.add(def.name());

          assertTrue(Array.isArray(views));
          assertTrue(views.length >= expectedViews.size);
          for (var i = 0; i < views.length; i++) {
            expectedViews.delete(views[i].name());
          }
          assertEqual(0, expectedViews.size);
        } finally {
          def.drop();
        }
      } finally {
        abc.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief modify properties
    ////////////////////////////////////////////////////////////////////////////
    testModifyProperties : function () {
      db._dropView("abc");
      var abc = db._createView("abc", "arangosearch", { "consolidationIntervalMsec": 10 });
      try {
        var props = abc.properties();

        assertEqual(abc.name(), "abc");
        assertEqual(abc.type(), "arangosearch");
        assertEqual(props.consolidationIntervalMsec, 10);

        abc.properties({ "consolidationIntervalMsec": 7 });
        abc = db._view("abc");
        props = abc.properties();

        assertEqual(abc.name(), "abc");
        assertEqual(abc.type(), "arangosearch");
        assertEqual(props.consolidationIntervalMsec, 7);
      } finally {
        abc.drop();
      }
    },

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ViewSuite);

return jsunity.done();
