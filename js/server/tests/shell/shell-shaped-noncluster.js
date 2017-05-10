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

jsunity.run(GeoShapedJsonSuite);

return jsunity.done();

