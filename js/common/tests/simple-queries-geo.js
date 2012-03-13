////////////////////////////////////////////////////////////////////////////////
/// @brief tests for "aql.js"
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlGeoTestSuite () {
  var collection1 = null;
  var documents1 = null;
  var index1 = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    this.collection1 = db.UnitTestsGeoAttributes;

    if (this.collection1.count() == 0) {
      for (var lat = -90;  lat <= 90;  lat += 5) {
        for (var lon = -180;  lon <= 180;  lon += 5) {
          this.collection1.save({ name : "name/lat=" + lat + "/lon=" + lon, lat : lat, lon : lon });
        }
      }
    }

    this.documents1 = this.collection1.T_toArray();
    this.index1 = db.UnitTestsGeoAttributes.ensureGeoIndex("lat", "lon");
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  function tearDown () {
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <collection>.toArray()
////////////////////////////////////////////////////////////////////////////////

  function testCollection1ToArray () {
    assertEqual(2701, this.collection1.T_toArray().length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <collection>.within()
////////////////////////////////////////////////////////////////////////////////

  function testCollection1Within () {
    assertEqual(1, this.collection1.within(0, 0, 1).toArray().length);
    assertEqual("name/lat=0/lon=0", this.collection1.within(0, 0, 1).next().name);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(aqlGeoTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
