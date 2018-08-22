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
var internal = require("internal");
var ArangoView = arangodb.ArangoView;
var testHelper = require("@arangodb/test-helper").Helper;
var db = arangodb.db;
var ERRORS = arangodb.errors;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: view
////////////////////////////////////////////////////////////////////////////////

function IResearchLinkSuite () {
  'use strict';
  var collection = null, collection2 = null;

  return {
    ////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////
    setUp : function () {
      internal.db._drop('testCollection');
      collection = internal.db._create('testCollection');
      internal.db._drop('testCollection2');
      collection2 = internal.db._create('testCollection2');
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // we need try...catch here because at least one test drops the collection itself!
      try {
        collection.unload();
        collection.drop();
        collection2.unload();
        collection2.drop();
      } catch (err) {
      }
      collection = null;
      internal.wait(0.0);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief should ignore links specified at creation
    ////////////////////////////////////////////////////////////////////////////
    testHandlingCreateWithLinks : function () {
      var meta = { links: { 'testCollection' : { includeAllFields: true } } };
      var view = db._createView("badView", "arangosearch", meta);
      var links = view.properties().links;
      assertNotEqual(links["testCollection"], undefined);
      view.drop();
    },

    testHandlingCreateWithMultipleLinks : function () {
      var meta = { links: { 'testCollection' : { includeAllFields: true },
        'testCollection2' : { includeAllFields: true } } };
      var view = db._createView("badView", "arangosearch", meta);
      var links = view.properties().links;
      assertNotEqual(links["testCollection"], undefined);
      assertNotEqual(links["testCollection2"], undefined);
      view.drop();
    },

    testHandlingCreateWithBadLinks : function () {
      var meta = { links: { 'nonExistingCollection' : { includeAllFields: true } } };

      try {
        var view = db._createView("badView", "arangosearch", meta);
      } catch(err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }

      try {
        var view = db._view('badView');
      } catch (err) {
        assertEqual(ERRORS.ERROR_VIEW_NOT_FOUND.code, err.errorNum);
      }
    },

    testHandlingCreateWithSomeBadLinks : function () {
      var meta = { links: {
        'testCollection' : { includeAllFields: true },
        'nonExistingCollection' : { includeAllFields: true },
        'testCollection2' : { includeAllFields: true }  } };

      try {
        var view = db._createView("badView", "arangosearch", meta);
      } catch(err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }

      try {
        var view = db._view("badView");
      } catch (err) {
        assertEqual(ERRORS.ERROR_VIEW_NOT_FOUND.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create a view and add/drop link
    ////////////////////////////////////////////////////////////////////////////
    testAddDrop : function () {
      var meta = { links: { 'testCollection' : { includeAllFields: true } } };
      var view = db._createView("testView", "arangosearch", {});
      view.properties(meta);

      var links = view.properties().links;
      assertNotEqual(links['testCollection'], undefined);
      assertTrue(links['testCollection'].includeAllFields);

      var newMeta = { links: { 'testCollection' : null } };
      view.properties(newMeta);
      links = view.properties().links;
      assertEqual(links['testCollection'], undefined);

      view.drop();
      try {
        view = db._view('testView');
      } catch (err) {
        assertEqual(ERRORS.ERROR_VIEW_NOT_FOUND.code, err.errorNum);
      }
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(IResearchLinkSuite);

return jsunity.done();
