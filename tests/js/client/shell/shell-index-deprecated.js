/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author ArangoDB GmbH
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: deprecated index types
////////////////////////////////////////////////////////////////////////////////

function deprecatedIndexesSuite() {
  'use strict';
  var cn = "UnitTestsCollectionDeprecatedIdx";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        collection.drop();
      } catch (err) {
      }
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: hash index is deprecated
////////////////////////////////////////////////////////////////////////////////

    testHashIndexDeprecated : function () {
      try {
        collection.ensureIndex({ type: "hash", fields: ["a"] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        assertTrue(err.errorMessage.includes("deprecated"));
        assertTrue(err.errorMessage.includes("persistent"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: skiplist index is deprecated
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndexDeprecated : function () {
      try {
        collection.ensureIndex({ type: "skiplist", fields: ["a"] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        assertTrue(err.errorMessage.includes("deprecated"));
        assertTrue(err.errorMessage.includes("persistent"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: fulltext index is deprecated
////////////////////////////////////////////////////////////////////////////////

    testFulltextIndexDeprecated : function () {
      try {
        collection.ensureIndex({ type: "fulltext", fields: ["text"], minLength: 3 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        assertTrue(err.errorMessage.includes("deprecated"));
        assertTrue(err.errorMessage.includes("inverted"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: geo1 index is deprecated
////////////////////////////////////////////////////////////////////////////////

    testGeo1IndexDeprecated : function () {
      try {
        collection.ensureIndex({ type: "geo1", fields: ["pos"] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        assertTrue(err.errorMessage.includes("deprecated"));
        assertTrue(err.errorMessage.includes("geo"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: geo2 index is deprecated
////////////////////////////////////////////////////////////////////////////////

    testGeo2IndexDeprecated : function () {
      try {
        collection.ensureIndex({ type: "geo2", fields: ["lat", "lon"] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        assertTrue(err.errorMessage.includes("deprecated"));
        assertTrue(err.errorMessage.includes("geo"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: persistent index still works
////////////////////////////////////////////////////////////////////////////////

    testPersistentIndexWorks : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: ["a"] });
      assertEqual("persistent", idx.type);
      assertEqual(["a"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: geo index still works
////////////////////////////////////////////////////////////////////////////////

    testGeoIndexWorks : function () {
      var idx = collection.ensureIndex({ type: "geo", fields: ["pos"], geoJson: true });
      assertEqual("geo", idx.type);
      assertEqual(["pos"], idx.fields);
    },

  };
}

jsunity.run(deprecatedIndexesSuite);

return jsunity.done();
