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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlGeoTestSuite () {
  var locations = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = AHUACATL_RUN(query, undefined);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query) {
    var result = executeQuery(query).getRows();
    var results = [ ];

    for (var i in result) {
      if (!result.hasOwnProperty(i)) {
        continue;
      }

      var row = result[i];
      var keys = [ ];
      for (var k in row) {
        if (row.hasOwnProperty(k) && k != '_id' && k != '_rev') {
          keys.push(k);
        }
      }

      keys.sort();
      var resultRow = { };
      for (var k in keys) {
        if (keys.hasOwnProperty(k)) {
          resultRow[keys[k]] = row[keys[k]];
        }

      }
      results.push(resultRow);
    }

    return results;
  }


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      locations = internal.db.UnitTestsAhuacatlLocations;

      if (locations.count() == 0) {
        for (var lat = -40; lat <= 40; ++lat) {
          for (var lon = -40; lon <= 40; ++lon) {
            locations.save({"latitude" : lat, "longitude" : lon });
          }
        }

        locations.ensureGeoIndex("latitude", "longitude");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near function
////////////////////////////////////////////////////////////////////////////////

    testNear1 : function () {
      var expected = [
        { "distance" : 111194.92664455873, "latitude" : -1, "longitude" : 0 }, 
        { "distance" : 111194.92664455873, "latitude" : 0, "longitude" : -1 }, 
        { "distance" : 111194.92664455873, "latitude" : 0, "longitude" : 1 }, 
        { "distance" : 111194.92664455873, "latitude" : 1, "longitude" : 0 }, 
        { "distance" : 0, "latitude" : 0, "longitude" : 0 }
      ];

      var actual = getQueryResults("FOR x IN NEAR(" + locations.name() + ", 0, 0, 5, \"distance\") SORT x.distance DESC, x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near function
////////////////////////////////////////////////////////////////////////////////

    testNear2 : function () {
      var expected = [
        { "latitude" : -10, "longitude" : 24 }, 
        { "latitude" : -10, "longitude" : 25 }, 
        { "latitude" : -10, "longitude" : 26 }
      ];

      var actual = getQueryResults("FOR x IN NEAR(" + locations.name() + ", -10, 25, 3) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near3 function
////////////////////////////////////////////////////////////////////////////////

    testNear3 : function () {
      var expected = [
        { "latitude" : -40, "longitude" : 38 }, 
        { "latitude" : -40, "longitude" : 39 }, 
        { "latitude" : -40, "longitude" : 40 }
      ];

      var actual = getQueryResults("FOR x IN NEAR(" + locations.name() + ", -90, 90, 3) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin1 : function () {
      var expected = [ { "distance" : 0, "latitude" : 0, "longitude" : 0 } ];
      var actual = getQueryResults("FOR x IN WITHIN(" + locations.name() + ", 0, 0, 10000, \"distance\") SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin2 : function () {
      var expected = [
        { "distance" : 111194.92664455873, "latitude" : -1, "longitude" : 0 }, 
        { "distance" : 111194.92664455873, "latitude" : 0, "longitude" : -1 }, 
        { "distance" : 0, "latitude" : 0, "longitude" : 0 }, 
        { "distance" : 111194.92664455873, "latitude" : 0, "longitude" : 1 }, 
        { "distance" : 111194.92664455873, "latitude" : 1, "longitude" : 0 }
      ];
     
      var actual = getQueryResults("FOR x IN WITHIN(" + locations.name() + ", 0, 0, 150000, \"distance\") SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin3 : function () {
      var expected = [ { "latitude" : -10, "longitude" : 25 } ];
      var actual = getQueryResults("FOR x IN WITHIN(" + locations.name() + ", -10, 25, 10000) SORT x.latitude, x.longitude RETURN x");
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

      var actual = getQueryResults("FOR x IN WITHIN(" + locations.name() + ", -10, 25, 150000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test within function
////////////////////////////////////////////////////////////////////////////////

    testWithin5 : function () {
      var expected = [
      ];

      var actual = getQueryResults("FOR x IN WITHIN(" + locations.name() + ", -90, 90, 10000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlGeoTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
