/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Florian Bartels
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db, indexId;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function withinRectangleSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop("geo");
      db._drop("geo2");

      var i, j ;
      db._create("geo");
      indexId = db.geo.ensureGeoIndex("lat", "lon");
      let geodocs = [];
      for (i = -40; i < 40; ++i) {
        for (j = -40; j < 40; ++j) {
          geodocs.push({ lat: i, lon: j });
        }
      }
      db.geo.insert(geodocs);
      geodocs = [];

      db._create("geo2");
      indexId = db.geo2.ensureGeoIndex("pos");

      for (i = -40; i < 40; ++i) {
        for (j = -40; j < 40; ++j) {
          geodocs.push({ pos : [i, j] });
        }
      }
      db.geo2.insert(geodocs);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop("geo");
      db._drop("geo2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test WITHIN_RECTANGLE as result
////////////////////////////////////////////////////////////////////////////////

    testWithinRectangleAsResult : function () {
      var actual = AQL_EXECUTE("RETURN WITHIN_RECTANGLE(geo, -1, -1, 1, 1)").json[0];
      assertEqual(actual.length , 9);
    },

    testWithinRectangleAsResultForSingleDocument : function () {
      var actual = AQL_EXECUTE("RETURN WITHIN_RECTANGLE(geo, -0.8, -1.2, -1.2, -0.8)").json[0];
      assertEqual(actual.length , 1);
    },

    testWithinRectangleAsResultForMissingDocument : function () {
      var actual = AQL_EXECUTE("RETURN WITHIN_RECTANGLE(geo, -41, -41, -41, -41)").json[0];
      assertEqual(actual.length , 0);
    },

    testWithinRectangleAsResultForUnknownCollection : function () {
      try  {
        AQL_EXECUTE("RETURN WITHIN_RECTANGLE(unknown, -41, -41, -41, -41)");
        fail();
      } catch (e) {
        assertTrue(e.errorNum === errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
    },

    testWithinRectangleAsResultForCollectionWithoutGeoIndex : function () {
      try  {
        AQL_EXECUTE("RETURN WITHIN_RECTANGLE(_graphs, -41, -41, -41, -41)");
        fail();
      } catch (e) {
        assertTrue(e.errorNum === errors.ERROR_QUERY_GEO_INDEX_MISSING.code);
      }
    },

    testWithinRectangleAsResultWithPositionBasedGeoIndex : function () {
      var actual =AQL_EXECUTE("RETURN WITHIN_RECTANGLE(geo2, -1, -1, 1, 1)").json[0];
      assertEqual(actual.length , 9);
    },

    testWithinRectangleAsResultForSingleDocumentWithPositionBasedGeoIndex : function () {
      var actual =AQL_EXECUTE("RETURN WITHIN_RECTANGLE(geo2, -0.8, -1.2, -1.2, -0.8)").json[0];
      assertEqual(actual.length , 1);
    },

    testWithinRectangleAsResultForMissingDocumentWithPositionBasedGeoIndex : function () {
      var actual =AQL_EXECUTE("RETURN WITHIN_RECTANGLE(geo2, -41, -41, -41, -41)").json[0];
      assertEqual(actual.length , 0);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(withinRectangleSuite);

return jsunity.done();

