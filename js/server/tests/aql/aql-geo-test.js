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
var internal = require("internal");
var db = require("@arangodb").db;
var errors = require("@arangodb").errors;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlGeoTestSuite () {
  var locations = null;

  function runQuery (query) {
    var result = getQueryResults(query);

    result = result.map(function(row) {
      var r;
      if (row !== null && typeof row === 'object') {
        for (r in row) {
          if (row.hasOwnProperty(r)) {
            var value = row[r];
            if (typeof(value) === "number") {
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

    setUp : function () {
      var lat, lon;

      var pre = internal.time();
      db._drop("UnitTestsAhuacatlLocations");
      var after = internal.time();
      internal.print("Drop: " + (after - pre) + " s");

      pre = internal.time();
      locations = db._create("UnitTestsAhuacatlLocations");
      after = internal.time();
      internal.print("Create: " + (after - pre) + " s");

      for (lat = -40; lat <= 40; ++lat) {
        for (lon = -40; lon <= 40; ++lon) {
          locations.save({"latitude" : lat, "longitude" : lon });
        }
      }

      pre = internal.time();
      locations.ensureGeoIndex("latitude", "longitude");
      after = internal.time();
      internal.print("Insert: " + (after - pre) + " s");

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsAhuacatlLocations");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near function
////////////////////////////////////////////////////////////////////////////////

    testNear1 : function () {
      var expected = [ { "distance" : "111194.92664", "latitude" : -1, "longitude" : 0 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : -1 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : 1 }, { "distance" : "111194.92664", "latitude" : 1, "longitude" : 0 }, { "distance" : 0, "latitude" : 0, "longitude" : 0 } ];
      var pre = internal.time();

      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", 0, 0, 5, \"distance\") SORT x.distance DESC, x.latitude, x.longitude RETURN x");

      
      var after = internal.time();
            internal.print("Cursor 1: " + (after - pre) + " s");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near function
////////////////////////////////////////////////////////////////////////////////

    testNear2 : function () {
      var expected = [ { "latitude" : -10, "longitude" : 24 }, { "latitude" : -10, "longitude" : 25 }, { "latitude" : -10, "longitude" : 26 } ];
            var pre = internal.time();

      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -10, 25, 3) SORT x.latitude, x.longitude RETURN x");
            var after = internal.time();
            internal.print("Cursor 2: " + (after - pre) + " s");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near function
////////////////////////////////////////////////////////////////////////////////

    testNear3 : function () {
      var expected = [ { "distance" : "14891044.54146", "latitude" : 40, "longitude" : -40 }, { "distance" : "14853029.30724", "latitude" : 40, "longitude" : -39 }, { "distance" : "14815001.47646", "latitude" : 40, "longitude" : -38 } ];
      var pre = internal.time();
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, 10000, \"distance\") SORT x.distance DESC LIMIT 3 RETURN x");
      var after = internal.time();
      internal.print("Cursor 3: " + (after - pre) + " s");
      assertEqual(expected, actual);
     
      expected = [ {"distance" : "4487652.12954", "latitude" : -37, "longitude" : 26 }, { "distance" : "4485565.93668", "latitude" : -39, "longitude" : 20 }, { "distance" : "4484371.86154" , "latitude" : -38, "longitude" : 23 } ]; 
      pre = internal.time();
      actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, null, \"distance\") SORT x.distance DESC LIMIT 3 RETURN x");
      after = internal.time();
      internal.print("Cursor 4: " + (after - pre) + " s");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near function
////////////////////////////////////////////////////////////////////////////////

    testNear4 : function () {
      var expected = [ {"latitude" : -40, "longitude" : 40 }, { "latitude" : -40, "longitude" : 39 }, { "latitude" : -40, "longitude" : 38 } ];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, null) SORT x.latitude, x.longitude DESC LIMIT 3 RETURN x");
      assertEqual(expected, actual);

      expected = [ { "latitude" : -40, "longitude" : 40 }, { "latitude" : -40, "longitude" : 39 } ];
      actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, 2) SORT x.latitude, x.longitude DESC LIMIT 3 RETURN x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin1 : function () {
      var expected = [ { "distance" : 0, "latitude" : 0, "longitude" : 0 } ];
      var pre = internal.time();
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", 0, 0, 10000, \"distance\") SORT x.latitude, x.longitude RETURN x");
      var after = internal.time();
      internal.print("Radius 1: " + (after - pre) + " s");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin2 : function () {
      var expected = [ { "distance" : "111194.92664", "latitude" : -1, "longitude" : 0 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : -1 }, { "distance" : 0, "latitude" : 0, "longitude" : 0 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : 1 }, { "distance" : "111194.92664", "latitude" : 1, "longitude" : 0 } ];
            var pre = internal.time();
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", 0, 0, 150000, \"distance\") SORT x.latitude, x.longitude RETURN x");
      var after = internal.time();
      internal.print("Radius 2: " + (after - pre) + " s");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin3 : function () {
      var expected = [ { "latitude" : -10, "longitude" : 25 } ];
                  var pre = internal.time();

      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -10, 25, 10000) SORT x.latitude, x.longitude RETURN x");
      var after = internal.time();
      internal.print("Radius 3: " + (after - pre) + " s");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin4 : function () {
      var expected = [ 
        { "latitude" : -11, "longitude" : 25 }, 
        { "latitude" : -10, "longitude" : 24 }, 
        { "latitude" : -10, "longitude" : 25 }, 
        { "latitude" : -10, "longitude" : 26 }, 
        { "latitude" : -9, "longitude" : 25 }
      ];

      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -10, 25, 150000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin5 : function () {
      var expected = [ ];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -90, 90, 10000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlGeoTestSuite);

return jsunity.done();

