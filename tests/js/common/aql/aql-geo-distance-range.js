/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print, fail */

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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Alexey Bakharew
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

let analyzers = require("@arangodb/analyzers");

function GeoDistanceRange() {
  return {
    setUpAll: function () {
      db._create("geo");
      analyzers.save("geo_json", "geojson", {}, []);
      db._createView("geo_view", "arangosearch", {
       links: {
          geo: {
            fields: {
              location: { analyzers: ["geo_json"] }
            }
          }
        }
      });
  
      let lat = 6.537;
      let long = 50.332;
      let points = [];
      for (let x = 0; x < 100; ++x) {
        for (let y = 0; y < 100; ++y) {
          points.push({ location: { type: "Point", coordinates: [lat + x/1000, long + y/1000] } });
        }
      }
      db.geo.save(points);
      points = [];

      db.geo.save({ location: { "type": "Polygon", "coordinates": [
        [[ 37.614323, 55.705898 ],
          [ 37.615825, 55.705898 ],
          [ 37.615825, 55.70652  ],
          [ 37.614323, 55.70652  ],
          [ 37.614323, 55.705898 ]]
      ]}});
      db.geo.save({ location: {"type": "LineString", "coordinates": [
        [ 6.537, 50.332 ], [ 6.537, 50.376 ]]
      }});
      db.geo.save({ location: { "type": "MultiLineString", "coordinates": [
        [[ 6.537, 50.332 ], [ 6.537, 50.376 ]],
        [[ 6.621, 50.332 ], [ 6.621, 50.376 ]]
      ]}});
      db.geo.save({ location: { "type": "MultiPoint", "coordinates": [
        [ 6.537, 50.332 ], [ 6.537, 50.376 ],
        [ 6.621, 50.332 ], [ 6.621, 50.376 ]
      ]}});
      db.geo.save({ location: { "type": "MultiPolygon", "coordinates": [
        [[[ 37.614323, 55.705898 ],
          [ 37.615825, 55.705898 ],
          [ 37.615825, 55.70652  ],
          [ 37.614323, 55.70652  ],
          [ 37.614323, 55.705898 ]]],
        [[[ 37.614, 55.7050 ],
          [ 37.615, 55.7050 ],
          [ 37.615, 55.7058 ],
          [ 37.614, 55.7058 ],
          [ 37.614, 55.7050 ]]]
      ]}});

      // sync view
      db._query(`
        LET lines = GEO_MULTILINESTRING([
          [[ 6.537, 50.332 ], [ 6.537, 50.376 ]],
          [[ 6.621, 50.332 ], [ 6.621, 50.376 ]]
        ])
        FOR doc IN geo_view SEARCH
          ANALYZER(GEO_DISTANCE(doc.location, lines) < 100, "geo_json") OPTIONS {'waitForSync': true} return doc`);
    },

    tearDownAll: function () {
      db._dropView("geo_view");
      db._drop("geo");
      analyzers.remove("geo_json", true);
    },

    test: function() {
      const queries = [
        // BTS-470
        [`
          LET lines = GEO_MULTILINESTRING([
            [[ 6.537, 50.332 ], [ 6.537, 50.376 ]],
            [[ 6.621, 50.332 ], [ 6.621, 50.376 ]]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_DISTANCE(doc.location, lines) < 100, "geo_json")
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines )})
          `, 5
        ],
        [`
          LET lines = GEO_MULTILINESTRING([
            [[ 6.537, 50.332 ], [ 6.537, 50.376 ]],
            [[ 6.621, 50.332 ], [ 6.621, 50.376 ]]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_DISTANCE(doc.location, lines) < 3000, "geo_json")
            FILTER doc.location.type != "Point"
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines )})
          `, 3
        ],
        [
          `
          LET lines = GEO_MULTILINESTRING([
            [[ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ]],
            [[ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ]]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_DISTANCE(doc.location, lines) < 100, "geo_json")
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines)})
          `, 2
        ],
        [
          `
          LET lines = GEO_MULTIPOINT([
            [ 6.537, 50.332 ], [ 6.537, 50.376 ],
            [ 6.621, 50.332 ], [ 6.621, 50.376 ]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_DISTANCE(doc.location, lines) < 100, "geo_json")
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines )})
          `, 5
        ],
        [
          `
          LET lines = GEO_MULTIPOINT([
            [ 6.537, 50.332 ], [ 6.537, 50.376 ],
            [ 6.621, 50.332 ], [ 6.621, 50.376 ]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_DISTANCE(doc.location, lines ) < 3000, "geo_json")
            FILTER doc.location.type != "Point"
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines )})
          `, 3
        ],
        [
          `
          LET lines = GEO_MULTIPOINT([
            [ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ],
            [ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_DISTANCE(doc.location, lines) < 100, "geo_json")
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines)})  
          `, 2
        ],
        // BTS-471
        [
          `
          LET lines = GEO_MULTILINESTRING([
            [[ 6.537, 50.332 ], [ 6.537, 50.376 ]],
            [[ 6.621, 50.332 ], [ 6.621, 50.376 ]]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_IN_RANGE(doc.location, lines, 0, 100), "geo_json")
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines )})
          `, 5
        ],
        [
          `
          LET lines = GEO_MULTILINESTRING([
            [[ 6.537, 50.332 ], [ 6.537, 50.376 ]],
            [[ 6.621, 50.332 ], [ 6.621, 50.376 ]]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_IN_RANGE(doc.location, lines, 0, 3000), "geo_json")
            FILTER doc.location.type != "Point"
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines )})
          `, 3
        ],
        [
          `
          LET lines = GEO_MULTILINESTRING([
            [[ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ]],
            [[ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ]]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_IN_RANGE(doc.location, lines, 0, 100), "geo_json")
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines)})
          `, 2
        ],
        [
          `
          LET lines = GEO_MULTIPOINT([
            [ 6.537, 50.332 ], [ 6.537, 50.376 ],
            [ 6.621, 50.332 ], [ 6.621, 50.376 ]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_IN_RANGE(doc.location, lines, 0, 100), "geo_json")
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines )})
          `, 5
        ],
        [
          `
          LET lines = GEO_MULTIPOINT([
            [ 6.537, 50.332 ], [ 6.537, 50.376 ],
            [ 6.621, 50.332 ], [ 6.621, 50.376 ]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_IN_RANGE(doc.location, lines, 0, 3000), "geo_json")
            FILTER doc.location.type != "Point"
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines )})
          `, 3
        ],
        [
          `
          LET lines = GEO_MULTIPOINT([
            [ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ],
            [ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ]
          ])
          FOR doc IN geo_view
            SEARCH ANALYZER(GEO_IN_RANGE(doc.location, lines, 0, 100), "geo_json")
            RETURN MERGE(doc, { distance: GEO_DISTANCE(doc.location, lines)})
          `, 2
        ]
      ];

      queries.forEach( function (query_tuple, index) {
        let [query, expected] = query_tuple;
        let actual;
        try {
          actual = db._query(query).toArray().length;
          assertEqual(actual, expected);
        } catch (err) {
          print(`Actual: ${actual}, Expected: ${expected}, Index: ${index}`);
          print(query);
          fail(err);
        }
      }); 
    }
  };
}

function GeoDistanceRangeSuite() {
  let suite = {};
  deriveTestSuite(
    GeoDistanceRange(),
    suite,
    "GeoDistanceRangeSuite"
  );
  return suite;
}  

jsunity.run(GeoDistanceRangeSuite);
return jsunity.done();
