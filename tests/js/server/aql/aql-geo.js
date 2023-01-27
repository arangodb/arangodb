/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print */

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
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var disableViewTests = true;

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var errors = require("@arangodb").errors;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

function makePolyInside(lon, lat) {
  // lon and lat are longitude and latitude ranges
  return {type:"Polygon",
          coordinates:[[[lon[0], lat[0]], [lon[1], lat[0]],
                        [lon[1], lat[1]], [lon[0], lat[1]], [lon[0], lat[0]]]]};
}

function makePolyOutside(lon, lat) {
  // lon and lat are longitude and latitude ranges
  return {type:"Polygon",
          coordinates:[[[lon[0], lat[0]], [lon[0], lat[1]],
                        [lon[1], lat[1]], [lon[1], lat[0]], [lon[0], lat[0]]]]};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function geoSuite(isSearchAlias, analyzerType) {

  let collWithoutIndex = "UnitTestsGeoWithoutIndex";
  let collWithIndex = "UnitTestsGeoWithIndex";
  let collWithView = "UnitTestsGeoWithView";
  let viewName = "UnitTestsGeoView";
  let noIndex;
  let withIndex;
  let withView;
  let view;

  let keyCounter = 0;

  function waitForArangoSearch() {
    let res = getQueryResults(
      `FOR d IN ${viewName} OPTIONS { waitForSync:true } LIMIT 1 RETURN 1`);
  }

  function insertAll(x) {
    for (let i = 0; i < x.length; ++i) {
      x[i]._key = "K" + (++keyCounter);
    }
    noIndex.insert(x);
    withIndex.insert(x);
    withView.insert(x);
  }

  function truncateAll(x) {
    noIndex.truncate();
    withIndex.truncate();
    withView.truncate();
  }

  function compareKeyLists(namea, a, nameb, b) {
    let good = true;
    let msg = "";
    if (a.length !== b.length) {
      good = false;
      msg += "Different number of results " + namea + " (" + a.length +
             ") and " + nameb + " (" + b.length + ").\n";
    }
    let i = 0;
    let j = 0;
    let count = 0;
    while (i < a.length && j < b.length) {
      if (a[i] < b[j]) {
        good = false;
        msg += "Found key " + a[i] + " in '" + namea + "' but not in '"
               + nameb + "'\n";
        ++i;
      } else if (a[i] > b[j]) {
        good = false;
        msg += "Found key " + b[j] + " in '" + nameb + "' but not in '"
               + namea + "'\n";
        ++j;
      } else {
        ++i;
        ++j;
      }
      if (++count >= 10) {
        return {good, msg};
      }
    }
    while (i < a.length) {
      good = false;
      msg += "Found key " + a[i] + " in '" + namea + "' but not in '"
             + nameb + "'\n";
      ++i;
      if (++count >= 10) {
        return {good, msg};
      }
    }
    while (j < b.length) {
      good = false;
      msg += "Found key " + b[j] + " in '" + nameb + "' but not in '"
             + namea + "'\n";
      ++j;
      if (++count >= 10) {
        return {good, msg};
      }
    }
    return {good, msg};
  }

  function compare(query, queryView) {
    let q = `FOR d IN @@coll ${query} RETURN d._key`;
    let qv = `FOR d IN ${viewName} ${queryView} RETURN d._key`;
    let wo = getQueryResults(q, {"@coll": collWithoutIndex}).sort();
    let wi = getQueryResults(q, {"@coll": collWithIndex}).sort();
    let wv = getQueryResults(qv, {}).sort();
    let oi = compareKeyLists("without index", wo, "with index", wi);
    let ov = compareKeyLists("without index", wo, "with view", wv);
    if (!oi.good || !ov.good) {
      print("Query for collections:", q);
      print("Query for views:", qv);
      print("Without index:", wo.length);
      print("With index:", wi.length);
      print("With view:", wv.length);
      print("Errors with index: ", oi.msg);
      print("Errors with view: ", ov.msg);
    }
    if (disableViewTests) {
      ov.good = true;   // fake goodness
    }
    return {oi, ov};
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up all
////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._dropView(viewName);
      db._drop(collWithoutIndex);
      db._drop(collWithIndex);
      db._drop(collWithView);

      noIndex = db._create(collWithoutIndex);
      withIndex = db._create(collWithIndex);
      withIndex.ensureIndex({type: "geo", geoJson: true, fields: ["geo"]});
      withView = db._create(collWithView);
      let analyzers = require("@arangodb/analyzers");
      analyzers.save("geo_json", analyzerType, {}, ["frequency", "norm", "position"]);
      if (isSearchAlias) {
        let i = db.UnitTestsGeoWithView.ensureIndex({type: "inverted", fields: [{name: "geo", analyzer: "geo_json"}]});
        view = db._createView(viewName, "search-alias", {
          indexes: [{collection: "UnitTestsGeoWithView", index: i.name}]
        });
      } else {
        view = db._createView(viewName, "arangosearch", {
          links: {
            UnitTestsGeoWithView: {
              fields: {geo: {analyzers: ["geo_json"]}}
            }
          }
        });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down all
////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._dropView(viewName);
      db._drop(collWithoutIndex);
      db._drop(collWithIndex);
      db._drop(collWithView);
/*
      let analyzers = require("@arangodb/analyzers");
      analyzers.remove(analyzerType);
*/
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      truncateAll();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test single point
////////////////////////////////////////////////////////////////////////////////

    testSetup : function () {
      insertAll([{geo: { type: "Point", coordinates: [50, 50] } }]);
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([50, 50], d.geo) < 5000`,
        `SEARCH ANALYZER(GEO_DISTANCE([50, 50], d.geo) < 5000, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test point near centroid of polygon
////////////////////////////////////////////////////////////////////////////////

    testPointNearCentroidOfPolygon : function () {
      insertAll([
        {geo:{type:"Polygon",
              coordinates:[[[10,10],[20,10],[20,20],[10,20],[10,10]]]}},
        {geo:{type:"Polygon",
              coordinates:[[[10,10],[20,10],[30,20],[10,20],[10,10]]]}},
        {geo:{type:"Polygon",
              coordinates:[[[10,10],[20,10],[20,20],[10,20],[10,11.1],
                [11,11.1],[11,19],[19,19],[19,11],[10,11],[10,10]]]}}
      ]);
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 5000`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 5000, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with grid of points, upper bound
////////////////////////////////////////////////////////////////////////////////

    testNearGrid : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 666666`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 666666, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with grid of points, upper and lower bound
////////////////////////////////////////////////////////////////////////////////

    testNearGridRingArea : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 666666 &&
                GEO_DISTANCE([15, 15], d.geo) >= 300000`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 666666 &&
                         GEO_DISTANCE([15, 15], d.geo) >= 300000, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with grid of points, only lower bound
////////////////////////////////////////////////////////////////////////////////

    testNearGridOuterArea : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) >= 300000`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) >= 300000, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with grid of points, upper bound, descending
////////////////////////////////////////////////////////////////////////////////

    testNearGridDescending : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 666666
         SORT GEO_DISTANCE([15, 15], d.geo) DESC`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 666666, "geo_json")
         SORT GEO_DISTANCE([15, 15], d.geo) DESC`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with grid of points, upper and lower bound, desc
////////////////////////////////////////////////////////////////////////////////

    testNearGridRingAreaDescending : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 666666 &&
                GEO_DISTANCE([15, 15], d.geo) >= 300000
         SORT GEO_DISTANCE([15, 15], d.geo) DESC`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 666666 &&
                         GEO_DISTANCE([15, 15], d.geo) >= 300000, "geo_json")
         SORT GEO_DISTANCE([15, 15], d.geo) DESC`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with grid of points, only lower bound, descending
////////////////////////////////////////////////////////////////////////////////

    testNearGridOuterAreaDescending : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) >= 300000`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) >= 300000, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with objects of different sizes
////////////////////////////////////////////////////////////////////////////////

    testNearObjectsSizes : function () {
      let l = [];
      for (let size = 0.0001; size <= 10; size *= 10) {
        for (let lon = 14.8; lon <= 15.2; lon += 0.04) {
          l.push({geo:{type:"Polygon", 
            coordinates:[[[lon-size, 10-size], [lon+size, 10-size],
                          [lon+size, 10+size], [lon-size, 10+size],
                          [lon-size, 10-size]]]},
            centroid:{type:"Point", coordinates:[lon, 10]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([10, 10], d.geo) <= 555974`,
        `SEARCH ANALYZER(GEO_DISTANCE([10, 10], d.geo) <= 555974, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with large objects different distance
////////////////////////////////////////////////////////////////////////////////

    testNearSlidingLargeObject : function () {
      let l = [];
      for (let size = 1; size <= 11; size += 1) {
        for (let lon = 11; lon <= 30; lon += 0.1) {
          l.push({geo:{type:"Polygon", 
            coordinates:[[[lon-size, 10-size], [lon+size, 10-size],
                          [lon+size, 10+size], [lon-size, 10+size],
                          [lon-size, 10-size]]]},
            centroid:{type:"Point", coordinates:[lon, 10]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let c = compare(
        `FILTER GEO_DISTANCE([10, 10], d.geo) <= 555974`,
        `SEARCH ANALYZER(GEO_DISTANCE([10, 10], d.geo) <= 555974, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with large non-convex object whose centroid
/// is not contained in the object.
////////////////////////////////////////////////////////////////////////////////

    testNearNonConvexObject : function () {
      insertAll([
        {geo:{type:"Polygon",
         coordinates:[[[0,0],[10,10],[0,20],[9,10],[0,0]]]}},
        {geo:{type:"Polygon",
         coordinates:[[[0,0],[10,10],[0,20],[0,19],[9,10],[0,1],[0,0]]]}}
      ]);
      // Centroid will be at approx. [7,10]
      waitForArangoSearch();
      for (let dist = 1000; dist <= 100000; dist += 100) {
        //print("Distance", dist);
        let c = compare(
          `FILTER GEO_DISTANCE([4.7874, 10.0735], d.geo) <= ${dist}`,
          `SEARCH ANALYZER(GEO_DISTANCE([4.7874, 10.0735], d.geo) <= ${dist}, "geo_json")`
        );
        assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains query with a polygon, move polygon over grip
////////////////////////////////////////////////////////////////////////////////

    testContainsWithSmallPoly : function () {
      let l = [];
      for (let lat = 9; lat <= 11; lat += 0.1) {
        for (let lon = 9; lon <= 11; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
        }
      }
      insertAll(l);
      waitForArangoSearch();
      let d = 0.001;
      for (let lat = 9; lat <= 11; lat += 0.4) {
        for (let lon = 9; lon <= 11; lon += 0.4) {
          let p = {type:"Polygon", coordinates:[[[lon-d, lat-d],
            [lon+d, lat-d], [lon+d, lat+d], [lon-d, lat+d], [lon-d, lat-d]]]};
          //print("Polygon:", JSON.stringify(p));
          let c = compare(
            `FILTER GEO_CONTAINS(${JSON.stringify(p)}, d.geo)`,
            `SEARCH ANALYZER(GEO_CONTAINS(${JSON.stringify(p)}, d.geo), "geo_json")`
          );
          assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains query with a large polygon and a point grid
////////////////////////////////////////////////////////////////////////////////

    testContainsGrid : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      for (let d = 1; d <= 5; d += 1) {
        let p = {type:"Polygon", coordinates:[[[15-d, 15], [15,15-d], [15+d,15],
          [15,15+d], [15-d,15]]]};
        //print("d=", d, JSON.stringify(p));
        let c = compare(
          `FILTER GEO_CONTAINS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_CONTAINS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains query with a complement of a large polygon and a 
/// point grid
////////////////////////////////////////////////////////////////////////////////

    testContainsGridComplement : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      for (let d = 1; d <= 5; d += 1) {
        let p = {type:"Polygon", coordinates:[[[15-d, 15], [15,15+d], [15+d,15],
          [15,15-d], [15-d,15]]]};
        //print("d=", d, JSON.stringify(p));
        let c = compare(
          `FILTER GEO_CONTAINS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_CONTAINS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersects query with a polygon, move polygon over grip
////////////////////////////////////////////////////////////////////////////////

    testIntersectsWithSmallPoly : function () {
      let l = [];
      for (let lat = 9; lat <= 11; lat += 0.1) {
        for (let lon = 9; lon <= 11; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
        }
      }
      insertAll(l);
      waitForArangoSearch();
      let d = 0.001;
      for (let lat = 9; lat <= 11; lat += 0.4) {
        for (let lon = 9; lon <= 11; lon += 0.4) {
          let p = {type:"Polygon", coordinates:[[[lon-d, lat-d],
            [lon+d, lat-d], [lon+d, lat+d], [lon-d, lat+d], [lon-d, lat-d]]]};
          //print("Polygon:", JSON.stringify(p));
          let c = compare(
            `FILTER GEO_INTERSECTS(${JSON.stringify(p)}, d.geo)`,
            `SEARCH ANALYZER(GEO_INTERSECTS(${JSON.stringify(p)}, d.geo), "geo_json")`
          );
          assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersects query with a large polygon and a point grid
////////////////////////////////////////////////////////////////////////////////

    testIntersectsGrid : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      for (let d = 1; d <= 5; d += 1) {
        let p = {type:"Polygon", coordinates:[[[15-d, 15], [15,15-d], [15+d,15],
          [15,15+d], [15-d,15]]]};
        //print("d=", d, JSON.stringify(p));
        let c = compare(
          `FILTER GEO_INTERSECTS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_INTERSECTS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersects query with a complement of a large polygon and a 
/// point grid
////////////////////////////////////////////////////////////////////////////////

    testIntersectsGridComplement : function () {
      let l = [];
      for (let lat = 9; lat <= 21; lat += 0.1) {
        for (let lon = 9; lon <= 21; lon += 0.1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      for (let d = 1; d <= 5; d += 1) {
        let p = {type:"Polygon", coordinates:[[[15-d, 15], [15,15+d], [15+d,15],
          [15,15-d], [15-d,15]]]};
        //print("d=", d, JSON.stringify(p));
        let c = compare(
          `FILTER GEO_INTERSECTS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_INTERSECTS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersects query with large non-convex object whose centroid
/// is not contained in the object.
////////////////////////////////////////////////////////////////////////////////

    testIntersectsNonConvexObject : function () {
      insertAll([
        {geo:{type:"Polygon",
         coordinates:[[[0,0],[10,10],[0,20],[9,10],[0,0]]]}},
        {geo:{type:"Polygon",
         coordinates:[[[0,0],[10,10],[0,20],[0,19],[9,10],[0,1],[0,0]]]}}
      ]);
      // Centroid will be at approx. [7,10]
      waitForArangoSearch();
      let smallPoly = {type:"Polygon",
        coordinates:[[[4.7873, 10.0734], [4.7875, 10.0734], [4.7875, 10.0736],
                      [4.7873, 10.0736], [4.7873, 10.0734]]]};
      let c = compare(
        `FILTER GEO_INTERSECTS(${JSON.stringify(smallPoly)}, d.geo)`,
        `SEARCH ANALYZER(GEO_INTERSECTS(${JSON.stringify(smallPoly)}, d.geo), "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test some areas which are more than half of the earth
////////////////////////////////////////////////////////////////////////////////

    testMoreThanHalf1 : function () {
      let l = [];
      for (let lat = 45; lat <= 55; lat += 1) {
        for (let lon = 90; lon <= 100; lon += 1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let polys = [
        makePolyInside([-85, 95], [-80, 50]),
        makePolyOutside([-85, 95], [-80, 50]),
        makePolyInside([-85, 85], [-80, 50]),
        makePolyOutside([-85, 85], [-80, 50]),
        makePolyInside([-85, 95], [-80, 40]),
        makePolyOutside([-85, 95], [-80, 40]),
        makePolyInside([-85, 85], [-80, 40]),
        makePolyOutside([-85, 85], [-80, 40]),
        makePolyInside([-85, 105], [-80, 60]),
        makePolyOutside([-85, 105], [-80, 60]),
        makePolyInside([170, -10], [-80, 60]),
        makePolyOutside([170, -10], [-80, 60]),
        makePolyInside([170, -10], [-80, 50]),
        makePolyOutside([170, -10], [-80, 50]),
      ];
      for (let p of polys) {
        let c = compare(
          `FILTER GEO_CONTAINS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_CONTAINS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test some areas which are more than half of the earth, around
/// date wrap
////////////////////////////////////////////////////////////////////////////////

    testMoreThanHalf2 : function () {
      let l = [];
      for (let lat = 45; lat <= 55; lat += 1) {
        for (let lon = 175; lon < 180; lon += 1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
        for (let lon = -179; lon <= -170; lon += 1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let polys = [
        makePolyInside([-85, 95], [-80, 50]),
        makePolyOutside([-85, 95], [-80, 50]),
        makePolyInside([-85, 85], [-80, 50]),
        makePolyOutside([-85, 85], [-80, 50]),
        makePolyInside([-85, 95], [-80, 40]),
        makePolyOutside([-85, 95], [-80, 40]),
        makePolyInside([-85, 85], [-80, 40]),
        makePolyOutside([-85, 85], [-80, 40]),
        makePolyInside([-85, 105], [-80, 60]),
        makePolyOutside([-85, 105], [-80, 60]),
        makePolyInside([170, -10], [-80, 60]),
        makePolyOutside([170, -10], [-80, 60]),
        makePolyInside([170, -10], [-80, 50]),
        makePolyOutside([170, -10], [-80, 50]),
      ];
      for (let p of polys) {
        let c = compare(
          `FILTER GEO_CONTAINS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_CONTAINS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test some areas which are more than half of the earth, around
/// north pole
////////////////////////////////////////////////////////////////////////////////

    testMoreThanHalf3 : function () {
      let l = [];
      for (let lat = 85; lat < 90; lat += 1) {
        for (let lon = 175; lon < 180; lon += 1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
        for (let lon = -179; lon <= -170; lon += 1) {
          l.push({geo:{type:"Point", coordinates:[lon, lat]}});
          if (l.length % 1000 === 0) {
            insertAll(l);
            l = [];
          }
        }
      }
      if (l.length > 0) {
        insertAll(l);
      }
      waitForArangoSearch();
      let polys = [
        makePolyInside([-85, 95], [-80, 87]),
        makePolyOutside([-85, 95], [-80, 87]),
        makePolyInside([-85, 85], [-80, 87]),
        makePolyOutside([-85, 85], [-80, 87]),
        makePolyInside([-85, 95], [-80, 80]),
        makePolyOutside([-85, 95], [-80, 80]),
        makePolyInside([-85, 85], [-80, 80]),
        makePolyOutside([-85, 85], [-80, 80]),
        makePolyInside([-85, 105], [-80, 89]),
        makePolyOutside([-85, 105], [-80, 89]),
        makePolyInside([170, -10], [-80, 89]),
        makePolyOutside([170, -10], [-80, 89]),
        makePolyInside([170, -10], [-80, 87]),
        makePolyOutside([170, -10], [-80, 87]),
      ];
      for (let p of polys) {
        let c = compare(
          `FILTER GEO_CONTAINS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_CONTAINS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief further non-convex cases
////////////////////////////////////////////////////////////////////////////////

    testMoreNonConvex : function () {
      let lat = 6.537;
      let long = 50.332;
      for (let x = 0; x < 100; ++x) {
        let points = [];
        for (let y = 0; y < 100; ++y) {
          points.push({ geo: { type: "Point",
              coordinates: [lat + x/1000, long + y/1000] } });
        }
        insertAll(points);
      }
      insertAll([{ geo: { "type": "Polygon", "coordinates": [
        [[ 37.614323, 55.705898 ],
          [ 37.615825, 55.705898 ],
          [ 37.615825, 55.70652  ],
          [ 37.614323, 55.70652  ],
          [ 37.614323, 55.705898 ]]
      ]}}]);
      insertAll([{ geo: {"type": "LineString", "coordinates": [
        [ 6.537, 50.332 ], [ 6.537, 50.376 ]]
      }}]);
      insertAll([{ geo: { "type": "MultiLineString", "coordinates": [
        [[ 6.537, 50.332 ], [ 6.537, 50.376 ]],
        [[ 6.621, 50.332 ], [ 6.621, 50.376 ]]
      ]}}]);
      insertAll([{ geo: { "type": "MultiPoint", "coordinates": [
        [ 6.537, 50.332 ], [ 6.537, 50.376 ],
        [ 6.621, 50.332 ], [ 6.621, 50.376 ]
      ]}}]);
      insertAll([{ geo: { "type": "MultiPolygon", "coordinates": [
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
      ]}}]);
      waitForArangoSearch();
      let multiLineString = {
        "type": "MultiLineString",
        "coordinates": [ [ [ 6.537, 50.332 ], [ 6.537, 50.376 ] ],
                         [ [ 6.621, 50.332 ], [ 6.621, 50.376 ] ] ] };
      let c = compare(
        `FILTER GEO_DISTANCE(${JSON.stringify(multiLineString)}, d.geo) < 100`,
        `SEARCH ANALYZER(GEO_DISTANCE(${JSON.stringify(multiLineString)}, d.geo) < 100, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      multiLineString = {
        "type": "MultiLineString",
        "coordinates": [ [[ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ]],
                         [[ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ]] ] };
      c = compare(
        `FILTER GEO_DISTANCE(${JSON.stringify(multiLineString)}, d.geo) < 100`,
        `SEARCH ANALYZER(GEO_DISTANCE(${JSON.stringify(multiLineString)}, d.geo) < 100, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      let multiPoint = {
        type: "MultiPoint",
        coordinates: [ [ 6.537, 50.332 ], [ 6.537, 50.376 ],
                       [ 6.621, 50.332 ], [ 6.621, 50.376 ] ] };
      c = compare(
        `FILTER GEO_DISTANCE(${JSON.stringify(multiPoint)}, d.geo) < 100`,
        `SEARCH ANALYZER(GEO_DISTANCE(${JSON.stringify(multiPoint)}, d.geo) < 100, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      multiPoint = {
        type: "MultiPoint",
        "coordinates": [ [ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ],
                         [ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ] ] };
      c = compare(
        `FILTER GEO_DISTANCE(${JSON.stringify(multiPoint)}, d.geo) < 100`,
        `SEARCH ANALYZER(GEO_DISTANCE(${JSON.stringify(multiPoint)}, d.geo) < 100, "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief further non-convex cases, GEO_IN_RANGE
////////////////////////////////////////////////////////////////////////////////

    testMoreNonConvexGeoInRange : function () {
      let lat = 6.537;
      let long = 50.332;
      for (let x = 0; x < 100; ++x) {
        let points = [];
        for (let y = 0; y < 100; ++y) {
          points.push({ geo: { type: "Point",
              coordinates: [lat + x/1000, long + y/1000] } });
        }
        insertAll(points);
      }
      insertAll([{ geo: { "type": "Polygon", "coordinates": [
        [[ 37.614323, 55.705898 ],
          [ 37.615825, 55.705898 ],
          [ 37.615825, 55.70652  ],
          [ 37.614323, 55.70652  ],
          [ 37.614323, 55.705898 ]]
      ]}}]);
      insertAll([{ geo: {"type": "LineString", "coordinates": [
        [ 6.537, 50.332 ], [ 6.537, 50.376 ]]
      }}]);
      insertAll([{ geo: { "type": "MultiLineString", "coordinates": [
        [[ 6.537, 50.332 ], [ 6.537, 50.376 ]],
        [[ 6.621, 50.332 ], [ 6.621, 50.376 ]]
      ]}}]);
      insertAll([{ geo: { "type": "MultiPoint", "coordinates": [
        [ 6.537, 50.332 ], [ 6.537, 50.376 ],
        [ 6.621, 50.332 ], [ 6.621, 50.376 ]
      ]}}]);
      insertAll([{ geo: { "type": "MultiPolygon", "coordinates": [
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
      ]}}]);
      waitForArangoSearch();
      let multiLineString = {
        "type": "MultiLineString",
        "coordinates": [ [ [ 6.537, 50.332 ], [ 6.537, 50.376 ] ],
                         [ [ 6.621, 50.332 ], [ 6.621, 50.376 ] ] ] };
      let c = compare(
        `FILTER GEO_IN_RANGE(${JSON.stringify(multiLineString)}, d.geo, 0, 100)`,
        `SEARCH ANALYZER(GEO_IN_RANGE(${JSON.stringify(multiLineString)}, d.geo, 0, 100), "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      multiLineString = {
        "type": "MultiLineString",
        "coordinates": [ [[ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ]],
                         [[ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ]] ] };
      c = compare(
        `FILTER GEO_IN_RANGE(${JSON.stringify(multiLineString)}, d.geo, 0, 100)`,
        `SEARCH ANALYZER(GEO_IN_RANGE(${JSON.stringify(multiLineString)}, d.geo, 0, 100), "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      let multiPoint = {
        type: "MultiPoint",
        coordinates: [ [ 6.537, 50.332 ], [ 6.537, 50.376 ],
                       [ 6.621, 50.332 ], [ 6.621, 50.376 ] ] };
      c = compare(
        `FILTER GEO_IN_RANGE(${JSON.stringify(multiPoint)}, d.geo, 0, 100)`,
        `SEARCH ANALYZER(GEO_IN_RANGE(${JSON.stringify(multiPoint)}, d.geo, 0, 100), "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
      multiPoint = {
        type: "MultiPoint",
        "coordinates": [ [ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ],
          [37.614323, 55.70652], [37.615825, 55.70652]]
      };
      c = compare(
        `FILTER GEO_IN_RANGE(${JSON.stringify(multiPoint)}, d.geo, 0, 100)`,
        `SEARCH ANALYZER(GEO_IN_RANGE(${JSON.stringify(multiPoint)}, d.geo, 0, 100), "geo_json")`
      );
      assertTrue(c.oi.good && c.ov.good, c.oi.msg + c.ov.msg);
    },
  };
}

function arangoSearchVPackGeoSuite() {
  let suite = {};
  deriveTestSuite(
    geoSuite(false, "geojson"),
    suite,
    "_vpack_arangosearch"
  );
  return suite;
}

function searchAliasVPackGeoSuite() {
  let suite = {};
  deriveTestSuite(
    geoSuite(true, "geojson"),
    suite,
    "_vpack_search-alias"
  );
  return suite;
}

function arangoSearchS2GeoSuite() {
  let suite = {};
  deriveTestSuite(
    geoSuite(false, "geojson-s2"),
    suite,
    "_s2_arangosearch"
  );
  return suite;
}

function searchAliasS2GeoSuite() {
  let suite = {};
  deriveTestSuite(
    geoSuite(true, "geojson-s2"),
    suite,
    "_s2_search-alias"
  );
  return suite;
}

jsunity.run(arangoSearchVPackGeoSuite);
jsunity.run(searchAliasVPackGeoSuite);
/*
jsunity.run(arangoSearchS2GeoSuite);
jsunity.run(searchAliasS2GeoSuite);
*/

return jsunity.done();
