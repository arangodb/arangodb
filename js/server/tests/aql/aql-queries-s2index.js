/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, geo queries
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
var db = require("@arangodb").db;
var errors = require("@arangodb").errors;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function legacyGeoTestSuite() {
  var locations = null;
  var locationsNon = null;

  function runQuery(query) {
    var result = getQueryResults(query);

    result = result.map(function (row) {
      var r;
      if (row !== null && typeof row === 'object') {
        for (r in row) {
          if (row.hasOwnProperty(r)) {
            var value = row[r];
            if (typeof (value) === "number") {
              if (value !== parseFloat(parseInt(value))) {
                row[r] = Number(value).toFixed(5);
              }
            }
          }
        }
      }

      return row;
    });

    return result;
  }

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var lat, lon;
      db._drop("UnitTestsAhuacatlLocations");
      db._drop("UnitTestsAhuacatlLocationsNon");

      locations = db._create("UnitTestsAhuacatlLocations");
      for (lat = -40; lat <= 40; ++lat) {
        for (lon = -40; lon <= 40; ++lon) {
          locations.save({ "latitude": lat, "longitude": lon });
        }
      }

      locations.ensureIndex({ type: "s2index", fields: ["latitude", "longitude"] });

      locationsNon = db._create("UnitTestsAhuacatlLocationsNon");

      for (lat = -40; lat <= 40; ++lat) {
        for (lon = -40; lon <= 40; ++lon) {
          locationsNon.save({ "latitude": lat, "longitude": lon });
        }
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop("UnitTestsAhuacatlLocations");
      db._drop("UnitTestsAhuacatlLocationsNon");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test near function
    ////////////////////////////////////////////////////////////////////////////////

    testNear1 : function () {
      var expected = [ { "distance" : "111194.92664", "latitude" : -1, "longitude" : 0 }, 
      { "distance" : "111194.92664", "latitude" : 0, "longitude" : -1 }, 
      { "distance" : "111194.92664", "latitude" : 0, "longitude" : 1 }, 
      { "distance" : "111194.92664", "latitude" : 1, "longitude" : 0 }, 
      { "distance" : 0, "latitude" : 0, "longitude" : 0 } ];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", 0, 0, 5) " + 
                            "LET d = DISTANCE(0,0,x.latitude,x.longitude) " + 
                            "SORT d DESC, x.latitude, x.longitude RETURN MERGE(x, {distance:d})");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test near function
    ////////////////////////////////////////////////////////////////////////////////

    testNear2: function () {
      var expected = [{ "latitude": -10, "longitude": 24 }, { "latitude": -10, "longitude": 25 }, { "latitude": -10, "longitude": 26 }];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -10, 25, 3) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test near function
    ////////////////////////////////////////////////////////////////////////////////

    testNear3 : function () {
      var expected = [ { "distance" : "14891044.54146", "latitude" : 40, "longitude" : -40 }, 
                       { "distance" : "14853029.30724", "latitude" : 40, "longitude" : -39 }, 
                       { "distance" : "14815001.47646", "latitude" : 40, "longitude" : -38 } ];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, 10000) " + 
                            "LET d = DISTANCE(-70,70,x.latitude,x.longitude) " + 
                            "SORT d DESC LIMIT 3 RETURN MERGE(x, {distance:d})");
      assertEqual(expected, actual);
     
      expected = [ {"distance" : "4487652.12954", "latitude" : -37, "longitude" : 26 }, 
      { "distance" : "4485565.93668", "latitude" : -39, "longitude" : 20 }, 
      { "distance" : "4484371.86154" , "latitude" : -38, "longitude" : 23 } ]; 
      actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, null) " + 
                        "LET d = DISTANCE(-70,70,x.latitude,x.longitude) " +
                        "SORT d DESC LIMIT 3 RETURN MERGE(x, {distance:d})");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test near function
    ////////////////////////////////////////////////////////////////////////////////

    testNear4: function () {
      var expected = [{ "latitude": -40, "longitude": 40 }, { "latitude": -40, "longitude": 39 }, { "latitude": -40, "longitude": 38 }];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, null) SORT x.latitude, x.longitude DESC LIMIT 3 RETURN x");
      assertEqual(expected, actual);

      expected = [{ "latitude": -40, "longitude": 40 }, { "latitude": -40, "longitude": 39 }];
      actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, 2) SORT x.latitude, x.longitude DESC LIMIT 3 RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    testWithin1 : function () {
      var expected = [ { "distance" : 0, "latitude" : 0, "longitude" : 0 } ];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", 0, 0, 10000) " + 
      "LET d = DISTANCE(0,0,x.latitude,x.longitude) " +
      "SORT x.latitude, x.longitude RETURN MERGE(x, {distance:d})");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    testWithin2 : function () {
      var expected = [ { "distance" : "111194.92664", "latitude" : -1, "longitude" : 0 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : -1 }, { "distance" : 0, "latitude" : 0, "longitude" : 0 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : 1 }, { "distance" : "111194.92664", "latitude" : 1, "longitude" : 0 } ];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", 0, 0, 150000) " + 
      "LET d = DISTANCE(0,0,x.latitude,x.longitude) " +
      "SORT x.latitude, x.longitude RETURN MERGE(x, {distance:d})");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    testWithin3: function () {
      var expected = [{ "latitude": -10, "longitude": 25 }];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -10, 25, 10000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    testWithin4: function () {
      var expected = [
        { "latitude": -11, "longitude": 25 },
        { "latitude": -10, "longitude": 24 },
        { "latitude": -10, "longitude": 25 },
        { "latitude": -10, "longitude": 26 },
        { "latitude": -9, "longitude": 25 }
      ];

      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -10, 25, 150000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    testWithin5: function () {
      var expected = [];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -90, 90, 10000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test without geo index available
    ////////////////////////////////////////////////////////////////////////////////

    testNonIndexed : function () {
      assertQueryError(errors.ERROR_QUERY_GEO_INDEX_MISSING.code, "RETURN NEAR(" + locationsNon.name() + ", 0, 0, 10)"); 
      assertQueryError(errors.ERROR_QUERY_GEO_INDEX_MISSING.code, "RETURN WITHIN(" + locationsNon.name() + ", 0, 0, 10)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid NEAR arguments count
////////////////////////////////////////////////////////////////////////////////

    testInvalidNearArgument : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NEAR(\"" + locationsNon.name() + "\", 0, 0, \"foo\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NEAR(\"" + locationsNon.name() + "\", 0, 0, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NEAR(\"" + locationsNon.name() + "\", 0, 0, 10, true)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid WITHIN arguments count
////////////////////////////////////////////////////////////////////////////////

    testInvalidWithinArgument : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(\"" + locationsNon.name() + "\", 0, 0, \"foo\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(\"" + locationsNon.name() + "\", 0, 0, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(\"" + locationsNon.name() + "\", 0, 0, 0, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(\"" + locationsNon.name() + "\", 0, 0, 0, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid collection parameter
////////////////////////////////////////////////////////////////////////////////

    testInvalidCollectionArgument : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(1234, 0, 0, 10)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(false, 0, 0, 10)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(true, 0, 0, 10)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN([ ], 0, 0, 10)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN({ }, 0, 0, 10)"); 
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "RETURN WITHIN(@name, 0, 0, 10)", { name: "foobarbazcoll" }); 
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "RETURN WITHIN(@name, 0, 0, 10)"); 
    }

  };
}

function pointsTestSuite() {

  // Test queries with index usage and without
  function runQuery(query) {
    var result1 = getQueryResults(query.string, query.bindVars || {}, false);
    var result2 = getQueryResults(query.string, query.bindVars || {}, false,
      { optimizer: { rules: ["-all"] } });
    assertEqual(query.expected, result1, query.string);
    assertEqual(query.expected, result2, query.string);
  }

  let locations;
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var lat, lon;
      db._drop("UnitTestsPointsTestSuite");

      locations = db._create("UnitTestsPointsTestSuite");
      for (lat = -40; lat <= 40; ++lat) {
        for (lon = -40; lon <= 40; ++lon) {
          locations.save({ "lat": lat, "lng": lon });
        }
      }

      locations.ensureIndex({ type: "s2index", fields: ["lat", "lng"] });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop("UnitTestsPointsTestSuite");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle1: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER DISTANCE(-10, 25, x.lat, x.lng) <= 10000 RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [{ "lat": -10, "lng": 25 }]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle2: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER DISTANCE(-10, 25, x.lat, x.lng) <= 150000 SORT x.lat, x.lng RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -10, "lng": 24 },
          { "lat": -10, "lng": 25 },
          { "lat": -10, "lng": 26 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle3: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER DISTANCE(-90, -90, x.lat, x.lng) <= 10000 SORT x.lat, x.lng RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: []
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle4: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) <= 10000 RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [{ "lat": -10, "lng": 25 }]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle5: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) <= 150000 SORT x.lat, x.lng RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -10, "lng": 24 },
          { "lat": -10, "lng": 25 },
          { "lat": -10, "lng": 26 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle6: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([-90, -90], [x.lng, x.lat]) <= 10000 SORT x.lat, x.lng RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: []
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulus1: function () {
      runQuery({
        string: `FOR x IN @@cc 
                   FILTER DISTANCE(-10, 25, x.lat, x.lng) <= 150000 
                   FILTER DISTANCE(-10, 25, x.lat, x.lng) > 108500 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -10, "lng": 24 },
          { "lat": -10, "lng": 26 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulus2: function () {
      runQuery({
        string: `FOR x IN @@cc 
                   FILTER DISTANCE(-10, 25, x.lat, x.lng) <= 150000 
                   FILTER DISTANCE(-10, 25, x.lat, x.lng) > 109545 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulus3: function () {
      runQuery({
        string: `FOR x IN @@cc 
                   FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) <= 150000 
                   FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) > 108500 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -10, "lng": 24 },
          { "lat": -10, "lng": 26 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulus4: function () {
      runQuery({
        string: `FOR x IN @@cc 
                   FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) <= 150000 
                   FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) > 109545 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsPolygon: function () {
      const polygon = {
        "type": "Polygon",
        "coordinates": [[[-11.5, 23.5], [-6, 26], [-10.5, 26.1], [-11.5, 23.5]]]
      };

      runQuery({
        string: `FOR x IN @@cc 
                   FILTER GEO_CONTAINS(@poly, [x.lng, x.lat]) 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
          "poly": polygon
        },
        expected: [
          { "lat": 24, "lng": -11 },
          { "lat": 25, "lng": -10 },
          { "lat": 25, "lng": -9 },
          { "lat": 26, "lng": -10 },
          { "lat": 26, "lng": -9 },
          { "lat": 26, "lng": -8 },
          { "lat": 26, "lng": -7 },
          { "lat": 26, "lng": -6 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testIntersectsPolygon: function () {
      const polygon = {
        "type": "Polygon",
        "coordinates": [[[-11.5, 23.5], [-6, 26], [-10.5, 26.1], [-11.5, 23.5]]]
      };

      runQuery({
        string: `FOR x IN @@cc 
                   FILTER GEO_INTERSECTS(@poly, [x.lng, x.lat]) 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
          "poly": polygon
        },
        expected: [
          { "lat": 24, "lng": -11 },
          { "lat": 25, "lng": -10 },
          { "lat": 25, "lng": -9 },
          { "lat": 26, "lng": -10 },
          { "lat": 26, "lng": -9 },
          { "lat": 26, "lng": -8 },
          { "lat": 26, "lng": -7 },
          { "lat": 26, "lng": -6 }
        ]
      });
    },

  };
}

const internal = require('internal');

////////////////////////////////////////////////////////////////////////////////
/// @brief Querys on a collection of GeoJson objects
////////////////////////////////////////////////////////////////////////////////
function geoJsonTestSuite() {
  let locations;

  // Test queries with index usage and without
  function runQuery(query) {
    var result1 = getQueryResults(query.string, query.bindVars || {}, false);
    var result2 = getQueryResults(query.string, query.bindVars || {}, false,
      { optimizer: { rules: ["-all"] } });
    let expected = query.expected.slice().sort();
    /*
    result1.forEach(k => internal.print("Res: ", locations.document(k)));
    expected.forEach(k => internal.print("Exp: ", locations.document(k)));//*/
    
    assertEqual(expected, result1.sort(), query.string);
    assertEqual(expected, result2.sort(), query.string);
  }

  // GeoJSON test data. https://gist.github.com/aaronlidman/7894176?short_path=2b56a92
  // Mostly from the spec: http://geojson.org/geojson-spec.html. 
  // stuff over Java island
  let indonesia = [
    { "type": "Polygon", "coordinates": [[[100.0, 0.0], [101.0, 0.0], [101.0, 1.0], [100.0, 1.0], [100.0, 0.0]]] },
    { "type": "LineString", "coordinates": [[102.0, 0.0], [103.0, 1.0], [104.0, 0.0], [105.0, 1.0]] },
    { "type": "Point", "coordinates": [102.0, 0.5] }];
  let indonesiaKeys = [];

  // EMEA region
  let emea = [ // TODO implement multi-polygon
    /*{ "type": "MultiPolygon", "coordinates": [ [[[40, 40], [20, 45], [45, 30], [40, 40]]],  
      [[[20, 35], [10, 30], [10, 10], [30, 5], [45, 20], [20, 35]],  [[30, 20], [20, 15], [20, 25], [30, 20]]]] }*/
      { "type": "Polygon",  "coordinates": [ [[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]], [[20, 30], [35, 35], [30, 20], [20, 30]]]},
      { "type": "LineString", "coordinates": [[25,10],[10,30],[25,40]] },
      { "type": "MultiLineString", "coordinates": [ [[10, 10], [20, 20], [10, 40]], [[40, 40], [30, 30], [40, 20], [30, 10]]] },
      { "type": "MultiPoint",  "coordinates": [ [10, 40], [40, 30], [20, 20], [30, 10] ] }
  ];
  let emeaKeys = [];
  let rectEmea1 = {"type":"Polygon","coordinates":[[[2,4],[26,4],[26,49],[2,49],[2,4]]]};

  let rectEmea3 = {"type":"Polygon","coordinates":[[[35,42],[49,42],[49,50],[35,50],[35,42]]]};

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var lat, lon;
      db._drop("UnitTestsGeoJsonTestSuite");

      locations = db._create("UnitTestsGeoJsonTestSuite");
      locations.ensureIndex({ type: "s2index", fields: ["geometry"], geoJson: true });

      indonesiaKeys = indonesia.map(doc => locations.save({ geometry: doc })._key);
      emeaKeys = emea.map(doc => locations.save({ geometry: doc })._key);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop("UnitTestsGeoJsonTestSuite");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle1: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([102, 0], x.geometry) <= 450000 RETURN x._key",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: indonesiaKeys
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere (without circle edge included)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle2: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([101, 0], x.geometry) < 283489.5838873064 RETURN x._key",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [indonesiaKeys[0], indonesiaKeys[2]]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle3: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([101, 0], x.geometry) <= 100000 RETURN x._key",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: indonesiaKeys.slice(0, 1)
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle4: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([101, 0], x.geometry) <= 100000 RETURN x._key",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: indonesiaKeys.slice(0, 1)
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////////

    testContainsPolygon1: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_CONTAINS(@poly, x.geometry) RETURN x._key",
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea1
        },
        expected: [emeaKeys[1]]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////////

    /*testContainsPolygon2: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_CONTAINS(@poly, x.geometry) RETURN x._key",
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea1
        },
        expected: [emeaKeys[1]]
      });
    },*/

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////////

    testIntersectsPolygon1: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_INTERSECTS(@poly, x.geometry) RETURN x._key",
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea1
        },
        expected: emeaKeys
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////////

    testIntersectsPolygon3: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_INTERSECTS(@poly, x.geometry) RETURN x._key",
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea3
        },
        expected: emeaKeys.slice(0,1)
      });
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(legacyGeoTestSuite);
jsunity.run(pointsTestSuite);
jsunity.run(geoJsonTestSuite);

return jsunity.done();

