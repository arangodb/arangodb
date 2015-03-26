/*global require, assertEqual, assertTrue, assertFalse, assertNotEqual, assertEqual */

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

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function GeoIndexCreationSuite() {
  "use strict";
  var cn = "UnitTestsCollectionGeo";
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
/// @brief test: index creation (list)
////////////////////////////////////////////////////////////////////////////////

    testCreationIndexLocationList : function () {
      var idx = collection.ensureGeoIndex("loc");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoIndex("loc");

      assertEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      idx = collection.ensureGeoIndex("loc", true);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoIndex("loc", true);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation (list, geo-json)
////////////////////////////////////////////////////////////////////////////////

    testCreationIndexLocationListGeo : function () {
      var idx = collection.ensureGeoIndex("loc", true);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoIndex("loc", true);

      assertEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      idx = collection.ensureGeoIndex("loc", false);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoIndex("loc", false);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation (attributes)
////////////////////////////////////////////////////////////////////////////////

    testCreationIndexLocationAttributes : function () {
      var idx = collection.ensureGeoIndex("lat", "lon");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertEqual(["lat", "lon"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoIndex("lat", "lon");

      assertEqual(id, idx.id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertEqual(["lat", "lon"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoIndex("lat", "lon");

      assertEqual(id, idx.id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertEqual(["lat", "lon"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: constraint creation (list)
////////////////////////////////////////////////////////////////////////////////

    testCreationConstraintLocationList : function () {
      var idx = collection.ensureGeoConstraint("loc", false);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("loc", false);

      assertEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("loc", true, false);

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
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: constraint creation (list, geo-json)
////////////////////////////////////////////////////////////////////////////////

    testCreationConstraintLocationListGeo : function () {
      var idx = collection.ensureGeoConstraint("loc", true, false);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("loc", true, false);

      assertEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("loc", false, false);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoConstraint("loc", false, false);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: constraint creation (attributes)
////////////////////////////////////////////////////////////////////////////////

    testCreationConstraintLocationAttributes : function () {
      var idx = collection.ensureGeoConstraint("lat", "lon", false);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("lat", "lon", false);

      assertEqual(id, idx.id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoConstraint("lat", "lon", false);

      assertEqual(id, idx.id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: constraint creation (list)
////////////////////////////////////////////////////////////////////////////////

    testCreationConstraintNullLocationList : function () {
      var idx = collection.ensureGeoConstraint("loc", true);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("loc", true);

      assertEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoConstraint("loc", false);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoConstraint("loc", false);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: constraint creation (list, geo-json)
////////////////////////////////////////////////////////////////////////////////

    testCreationConstraintNullLocationListGeo : function () {
      var idx = collection.ensureGeoConstraint("loc", true, true);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("loc", true, true);

      assertEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertTrue(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("loc", false, true);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoConstraint("loc", false, true);

      assertNotEqual(id, idx.id);
      assertEqual("geo1", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);
      assertEqual(["loc"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: constraint creation (attributes)
////////////////////////////////////////////////////////////////////////////////

    testCreationConstraintNullLocationAttributes : function () {
      var idx = collection.ensureGeoConstraint("lat", "lon", true);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureGeoConstraint("lat", "lon", true);

      assertEqual(id, idx.id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      collection.unload();

      idx = collection.ensureGeoConstraint("lat", "lon", true);

      assertEqual(id, idx.id);
      assertEqual("geo2", idx.type);
      assertFalse(idx.constraint);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Simple Queries
////////////////////////////////////////////////////////////////////////////////

function GeoIndexErrorHandlingSuite() {
  "use strict";
  var cn = "UnitTestsCollectionGeo";
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
    collection.drop();
    internal.wait(0.0);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: error handling index
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingIndexList : function () {
      collection.ensureGeoIndex("loc");

      var d1 = collection.save({ a : 1 });
      var d2 = collection.save({ loc : null });
      var d3 = collection.save({ loc : [0] });
      var d4 = collection.save({ loc : [ -100, -200 ] });
      collection.save({ loc : [ -10, -20 ]});

      assertEqual(1, collection.near(0,0).toArray().length);

      d1 = collection.replace(d1, { loc : [ 0, 0 ] });
      d2 = collection.replace(d2, { loc : [ 0, 0 ] });
      d3 = collection.replace(d3, { loc : [ 0, 0 ] });
      d4 = collection.replace(d4, { loc : [ 0, 0 ] });

      assertEqual(5, collection.near(0,0).toArray().length);

      collection.replace(d1, { a : 2 });
      collection.replace(d2, { loc : null });
      collection.replace(d3, { loc : [ 0 ] });
      collection.replace(d4, { loc : [ -100, -200 ] });

      assertEqual(1, collection.near(0,0).toArray().length);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Simple Queries
////////////////////////////////////////////////////////////////////////////////

function GeoIndexSimpleQueriesSuite() {
  "use strict";
  var cn = "UnitTestsCollectionGeo";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  setUp : function () {
    internal.db._drop(cn);
    collection = internal.db._create(cn, { waitForSync : false });

    try {
      for (var i = -90;  i <= 90;  i += 10) {
        for (var j = -180; j <= 180;  j+= 10) {
          collection.save({
            name : "Name/" + i + "/" + j,
                vloc : [ i, j ],
                gloc : [ j, i ],
                aloc : { latitude : i, longitude : j }
            });
        }
      }
    }
    catch (err) {
      console.error("cannot create locations: " + err);
    }
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  tearDown : function () {
    collection.drop();
    internal.wait(0.0);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: near query (list)
////////////////////////////////////////////////////////////////////////////////

    testNearLocationList : function () {
      collection.ensureGeoIndex("vloc");

      assertEqual(collection.count(), 703);

      var r = collection.near(0,0).toArray();

      assertEqual(100, r.length);

      r = collection.near(0,0).limit(3).toArray();

      assertEqual(3, r.length);
      assertEqual([0,0], r[0].vloc);

      r = collection.near(0,0).limit(1000).toArray();

      assertEqual(703, r.length);
      assertEqual([0,0], r[0].vloc);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: near query (list geo)
////////////////////////////////////////////////////////////////////////////////

    testNearLocationListGeo : function () {
      collection.ensureGeoIndex("gloc", true);

      assertEqual(collection.count(), 703);

      var r = collection.near(0,0).toArray();

      assertEqual(100, r.length);

      r = collection.near(0,0).limit(3).toArray();

      assertEqual(3, r.length);
      assertEqual([0,0], r[0].gloc);

      r = collection.near(0,0).limit(1000).toArray();

      assertEqual(703, r.length);
      assertEqual([0,0], r[0].gloc);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: near query (attributes)
////////////////////////////////////////////////////////////////////////////////

    testNearLocationAttributes : function () {
      collection.ensureGeoIndex("aloc.latitude", "aloc.longitude");

      assertEqual(collection.count(), 703);

      var r = collection.near(0,0).toArray();

      assertEqual(100, r.length);

      r = collection.near(0,0).limit(3).toArray();

      assertEqual(3, r.length);
      assertEqual({ latitude: 0, longitude: 0 }, r[0].aloc);

      r = collection.near(0,0).limit(1000).toArray();

      assertEqual(703, r.length);
      assertEqual({ latitude: 0, longitude: 0 }, r[0].aloc);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: near query (distance)
////////////////////////////////////////////////////////////////////////////////

    testNearLocationDistance : function () {
      collection.ensureGeoIndex("vloc");

      assertEqual(collection.count(), 703);

      var r = collection.near(0,0).distance().toArray();

      assertEqual(100, r.length);
      assertEqual(0, r[0].distance);
      assertEqual(1111949, Math.round(r[1].distance));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: near query (distance)
////////////////////////////////////////////////////////////////////////////////

    testNearLocationNamedDistance : function () {
      collection.ensureGeoIndex("vloc");

      assertEqual(collection.count(), 703);

      var r = collection.near(0,0).distance("xyz").toArray();

      assertEqual(100, r.length);
      assertEqual(undefined, r[0].distance);
      assertEqual(0, r[0].xyz);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within query (list)
////////////////////////////////////////////////////////////////////////////////

    testWithinLocationList : function () {
      collection.ensureGeoIndex("vloc");

      assertEqual(collection.count(), 703);

      var r = collection.within(50, 0, 1111949 * 6).toArray();

      assertEqual(218, r.length);

      r = collection.within(50, 0, 1111949 * 4).limit(3).toArray();

      assertEqual(3, r.length);
      assertEqual([50,0], r[0].vloc);

      r = collection.within(50, 0, 1111949 * 20).limit(1000).toArray();

      assertEqual(703, r.length);
      assertEqual([50,0], r[0].vloc);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within query (list geo)
////////////////////////////////////////////////////////////////////////////////

    testWithinLocationListGeo : function () {
      collection.ensureGeoIndex("gloc", true);

      assertEqual(collection.count(), 703);

      var r = collection.within(50, 0, 1111949 * 4).toArray();

      assertEqual(87, r.length);

      r = collection.within(50, 0, 1111949 * 4).limit(3).toArray();

      assertEqual(3, r.length);
      assertEqual([0,50], r[0].gloc);

      r = collection.within(50, 0, 1111949 * 20).limit(1000).toArray();

      assertEqual(703, r.length);
      assertEqual([0,50], r[0].gloc);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within query (attributes)
////////////////////////////////////////////////////////////////////////////////

    testWithinLocationAttributes : function () {
      collection.ensureGeoIndex("aloc.latitude", "aloc.longitude");

      assertEqual(collection.count(), 703);

      var r = collection.within(50, 0, 1111949 * 4).toArray();

      assertEqual(87, r.length);

      r = collection.within(50, 0, 1111949 * 4).limit(3).toArray();

      assertEqual(3, r.length);
      assertEqual({ latitude: 50, longitude: 0 }, r[0].aloc);

      r = collection.within(50, 0, 1111949 * 20).limit(1000).toArray();

      assertEqual(703, r.length);
      assertEqual({ latitude: 50, longitude: 0 }, r[0].aloc);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within query (distance)
////////////////////////////////////////////////////////////////////////////////

    testWithinLocationDistance : function () {
      collection.ensureGeoIndex("vloc");

      assertEqual(collection.count(), 703);

      var r = collection.within(50, 0, 1111949 * 4).distance().toArray();

      assertEqual(87, r.length);
      assertEqual(0, r[0].distance);
      assertEqual(714214, Math.round(r[1].distance));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within query (distance)
////////////////////////////////////////////////////////////////////////////////

    testWithinLocationNamedDistance : function () {
      collection.ensureGeoIndex("vloc");

      assertEqual(collection.count(), 703);

      var r = collection.within(50, 0, 1111949 * 4).distance("xyz").toArray();

      assertEqual(87, r.length);
      assertEqual(undefined, r[0].distance);
      assertEqual(0, r[0].xyz);
      assertEqual(714214, Math.round(r[1].xyz));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: selecting (distance)
////////////////////////////////////////////////////////////////////////////////

    testSelectGeoIndex : function () {
      collection.ensureGeoIndex("vloc");
      collection.ensureGeoIndex("gloc", true);

      assertEqual(collection.count(), 703);

      var r = collection.geo("vloc").within(50, 0, 1000).toArray();

      assertEqual(1, r.length);
      assertEqual([50, 0], r[0].vloc);

      r = collection.geo("gloc", true).within(50, 0, 1000).toArray();

      assertEqual(1, r.length);
      assertEqual([0, 50], r[0].gloc);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within rectangle
////////////////////////////////////////////////////////////////////////////////

    testWithinRectangleFlat : function () {
      collection.ensureGeoIndex("lat", "lon");
      var doc = collection.save({ location: [ 50, 50 ] });
      doc = collection.save({ lat: 2, lon: 2 });

      var r = collection.near(0, 0).limit(1).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);

      r = collection.within(0, 0, 1000000).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);
      
      r = collection.withinRectangle(0, 0, 5, 5).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within rectangle
////////////////////////////////////////////////////////////////////////////////

    testWithinRectangleFlatNoDocs : function () {
      collection.ensureGeoIndex("lat", "lon");
      collection.save({ location: { lat: 2, lon: 2 } });
      collection.save({ lat: "2222", lon: "2222" });
      collection.save({ lat: 2, lon: "2222" });
      collection.save({ lat: "2222", lon: 2 });
      collection.save({ lat: 2, lon: null });
      collection.save({ lat: null, lon: 2 });
      collection.save({ lat: null, lon: null });
      collection.save({ lat: true, lon: true });
      collection.save({ lat: [ 2, 2 ], lon: [ 2, 2 ] });

      var r = collection.near(0, 0).toArray();
      assertEqual(0, r.length);

      r = collection.within(0, 0, 1000000).toArray();
      assertEqual(0, r.length);
      
      r = collection.withinRectangle(0, 0, 5, 5).toArray();
      assertEqual(0, r.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within rectangle
////////////////////////////////////////////////////////////////////////////////

    testWithinRectangleSingleFlat : function () {
      collection.ensureGeoIndex("location");
      var doc = collection.save({ location: [ 50, 50 ] });
      doc = collection.save({ location: [ 2, 2 ] });

      var r = collection.near(0, 0).limit(1).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);

      r = collection.within(0, 0, 1000000).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);
      
      r = collection.withinRectangle(0, 0, 5, 5).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within rectangle
////////////////////////////////////////////////////////////////////////////////

    testWithinRectangleSingleFlatNoDocs : function () {
      collection.ensureGeoIndex("location");
      collection.save({ location: { lat: 2, lon: 2 } });
      collection.save({ location: "2222" });
      collection.save({ location: 2 });
      collection.save({ location: null });
      collection.save({ location: true });

      var r = collection.near(0, 0).toArray();
      assertEqual(0, r.length);

      r = collection.within(0, 0, 1000000).toArray();
      assertEqual(0, r.length);
      
      r = collection.withinRectangle(0, 0, 5, 5).toArray();
      assertEqual(0, r.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within rectangle
////////////////////////////////////////////////////////////////////////////////

    testWithinRectangleNested : function () {
      collection.ensureGeoIndex("location.lat", "location.lon");
      var doc = collection.save({ location: { lat: 50, lon: 50 } });
      doc = collection.save({ location: { lat: 2, lon: 2 } });

      var r = collection.near(0, 0).limit(1).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);

      r = collection.within(0, 0, 1000000).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);
      
      r = collection.withinRectangle(0, 0, 5, 5).toArray();
      assertEqual(1, r.length);
      assertEqual(doc._key, r[0]._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: within rectangle
////////////////////////////////////////////////////////////////////////////////

    testWithinRectangleNestedNoDocs : function () {
      collection.ensureGeoIndex("location.lat", "location.lon");
      collection.save({ location: { foo: 2, bar: 2 } });
      collection.save({ location: { lat: "2222", lon: "2222" } });
      collection.save({ location: { lat: [ 2, 2 ], lon: [ 2, 2 ] } });
      collection.save({ location: [ 2, 2 ] });
      collection.save({ location: "2222" });
      collection.save({ location: null });
      collection.save({ location: true });
      collection.save({ lat: 2, lon: 2 });

      var r = collection.near(0, 0).toArray();
      assertEqual(0, r.length);

      r = collection.within(0, 0, 1000000).toArray();
      assertEqual(0, r.length);
      
      r = collection.withinRectangle(0, 0, 5, 5).toArray();
      assertEqual(0, r.length);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(GeoIndexCreationSuite);
jsunity.run(GeoIndexErrorHandlingSuite);
jsunity.run(GeoIndexSimpleQueriesSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
