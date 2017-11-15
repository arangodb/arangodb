/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the geo index
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var console = require("console");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function SphericalIndexCreationSuite() {
  'use strict';
  var cn = "UnitTestsCollectionSpherical";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.unload();
      collection.drop();
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: updates
////////////////////////////////////////////////////////////////////////////////

    testUpdates : function () {
      collection.truncate();

      //collection.ensureGeoIndex("coordinates", true);
      collection.ensureIndex({type: "geospatial", 
                              fields: ["coordinates"], 
                              geoJson: true
                             });

      [
        { _key: "a", coordinates : [ 138.46782, 36.199674 ] },
        { _key: "b", coordinates : [ 135.827227, 34.755123 ] },
        { _key: "c", coordinates : [ 140.362435, 37.41623 ] },
        { _key: "d", coordinates : [ 135.820818, 34.736769 ] },
        { _key: "e", coordinates : [ 140.371997, 37.397514 ] },
        { _key: "f", coordinates : [ 140.375676, 37.442712 ] },
        { _key: "g", coordinates : [ 136.711924, 35.405093 ] },
        { _key: "h", coordinates : [ 138.609493, 36.208831 ] },
        { _key: "i", coordinates : [ 136.715309, 35.406745 ] },
        { _key: "j", coordinates : [ 139.781937, 35.894845 ] },
        { _key: "k", coordinates : [ 134.888787, 34.339736 ] },
        { _key: "l", coordinates : [ 134.899736, 34.341144 ] },
        { _key: "m", coordinates : [ 134.846816, 34.316229 ] }
      ].forEach(function(doc) {
        collection.insert(doc);
      });

      assertEqual(13, collection.count());

      var query = "FOR doc IN " + collection.name() + " LET n = (" +
                  "LET c = doc.coordinates\n FOR other IN " + collection.name() +
                 "FILTER DISTANCE(c[1], c[0], other.coordinates[1], other.coordinates[0]) < 1000 RETURN other._key) " +
                  "UPDATE doc WITH {others: n} IN " + collection.name();

      internal.db._query(query);

      query = "FOR doc IN " + collection.name() + " SORT doc._key " +
              "RETURN { key: doc._key, coordinates: doc.coordinates, others: doc.others }";

      var actual = internal.db._query(query).toArray();
      var expected = [
        { "key" : "a", "coordinates" : [ 138.46782, 36.199674 ], "others" : [ "a" ] },
        { "key" : "b", "coordinates" : [ 135.827227, 34.755123 ], "others" : [ "b" ] },
        { "key" : "c", "coordinates" : [ 140.362435, 37.41623 ], "others" : [ "c" ] },
        { "key" : "d", "coordinates" : [ 135.820818, 34.736769 ], "others" : [ "d" ] },
        { "key" : "e", "coordinates" : [ 140.371997, 37.397514 ], "others" : [ "e" ] },
        { "key" : "f", "coordinates" : [ 140.375676, 37.442712 ], "others" : [ "f" ] },
        { "key" : "g", "coordinates" : [ 136.711924, 35.405093 ], "others" : [ "g", "i" ] },
        { "key" : "h", "coordinates" : [ 138.609493, 36.208831 ], "others" : [ "h" ] },
        { "key" : "i", "coordinates" : [ 136.715309, 35.406745 ], "others" : [ "i", "g" ] },
        { "key" : "j", "coordinates" : [ 139.781937, 35.894845 ], "others" : [ "j" ] },
        { "key" : "k", "coordinates" : [ 134.888787, 34.339736 ], "others" : [ "k" ] },
        { "key" : "l", "coordinates" : [ 134.899736, 34.341144 ], "others" : [ "l" ] },
        { "key" : "m", "coordinates" : [ 134.846816, 34.316229 ], "others" : [ "m" ] }
      ];

      assertEqual(expected.length, actual.length);
      for (var i = 0; i < expected.length; ++i) {
        expected[i].coordinates[0] = expected[i].coordinates[0].toFixed(4);
        expected[i].coordinates[1] = expected[i].coordinates[1].toFixed(4);
        actual[i].coordinates[0] = actual[i].coordinates[0].toFixed(4);
        actual[i].coordinates[1] = actual[i].coordinates[1].toFixed(4);
      }

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation (list)
////////////////////////////////////////////////////////////////////////////////

    testCreationIndexLocationList : function () {
      //var idx = collection.ensureGeoIndex("loc");
      let idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: false});
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      //idx = collection.ensureGeoIndex("loc");
      idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: false});                        

      assertEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      //idx = collection.ensureGeoIndex("loc", true);
      idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: true});                  

      assertNotEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.unload();

      //idx = collection.ensureGeoIndex("loc", true);
      idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: true});            

      assertNotEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation (list, geo-json)
////////////////////////////////////////////////////////////////////////////////

    testCreationIndexLocationListGeo : function () {
      //var idx = collection.ensureGeoIndex("loc", true);
      let idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: true});      
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      //idx = collection.ensureGeoIndex("loc", true);
      idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: true});   

      assertEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      //idx = collection.ensureGeoIndex("loc", false);
      idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: false});

      assertNotEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.unload();

      //idx = collection.ensureGeoIndex("loc", false);
      idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: false});

      assertNotEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation (attributes)
////////////////////////////////////////////////////////////////////////////////

    testCreationIndexLocationAttributes : function () {
      //var idx = collection.ensureGeoIndex("lat", "lon");
      let idx = collection.ensureIndex({type: "geospatial", fields:["lat", "lon"]});
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertEqual(["lat", "lon"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      //idx = collection.ensureGeoIndex("lat", "lon");
      idx = collection.ensureIndex({type: "geospatial", fields:["lat", "lon"]});

      assertEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertEqual(["lat", "lon"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      collection.unload();

      //idx = collection.ensureGeoIndex("lat", "lon");
      idx = collection.ensureIndex({type: "geospatial", fields:["lat", "lon"]});      

      assertEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertEqual(["lat", "lon"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: constraint creation (list)
////////////////////////////////////////////////////////////////////////////////

    testCreationConstraintLocationList : function () {
      //var idx = collection.ensureGeoConstraint("loc", false);
      let idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: false});            
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      //idx = collection.ensureGeoConstraint("loc", false);
      idx = collection.ensureIndex({type: "geospatial", fields:["loc"], geoJson: false});       

      assertEqual(id, idx.id);
      assertEqual("geospatial", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      /*idx = collection.ensureGeoConstraint("loc", true, false);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoConstraint("loc", true, false);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);*/
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SphericalIndexCreationSuite);

return jsunity.done();

