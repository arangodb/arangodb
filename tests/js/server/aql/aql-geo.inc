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

(function () {
  const disableViewTests = false;

  const jsunity = require("jsunity");
  const db = require("@arangodb").db;
  const errors = require("@arangodb").errors;
  const helper = require("@arangodb/aql-helper");
  const getQueryResults = helper.getQueryResults;
  const assertQueryError = helper.assertQueryError;
  const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
  const isEnterprise = require("internal").isEnterprise();

  return function (isSearchAlias, analyzerType, analyzerDefinition, isInt = false) {

    function makePolyInside(lon, lat) {
      // lon and lat are longitude and latitude ranges
      return {
        type: "Polygon",
        coordinates: [[[lon[0], lat[0]], [lon[1], lat[0]],
          [lon[1], lat[1]], [lon[0], lat[1]], [lon[0], lat[0]]]]
      };
    }

    function makePolyOutside(lon, lat) {
      // lon and lat are longitude and latitude ranges
      return {
        type: "Polygon",
        coordinates: [[[lon[0], lat[0]], [lon[0], lat[1]],
          [lon[1], lat[1]], [lon[1], lat[0]], [lon[0], lat[0]]]]
      };
    }

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
    let fullScanQuery = `FOR d IN @@coll ${query} RETURN d._key`;
    let viewQuery = `FOR d IN ${viewName} ${queryView} RETURN d._key`;
    let invertedIndexQuery = `FOR d IN ${collWithIndex} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} ${query} RETURN d._key`;

    let resFullScanNoIndexes = getQueryResults(fullScanQuery, {"@coll": collWithoutIndex}).sort();
    let resFullScanWithIndexes = getQueryResults(fullScanQuery, {"@coll": collWithIndex}).sort();
    let resView = getQueryResults(viewQuery, {}).sort();
    let resInvertedIndex = getQueryResults(invertedIndexQuery, {}).sort();

    let cmpResFullScanWithIndexes = compareKeyLists("without index", resFullScanNoIndexes, "with index", resFullScanWithIndexes);
    let cmpResView = compareKeyLists("without index", resFullScanNoIndexes, "with view", resView);
    let cmpResInvertedIndex = compareKeyLists("without index", resFullScanNoIndexes, "with inverted index", resInvertedIndex);

    let explanation = db._createStatement(invertedIndexQuery).explain().plan;
    let utilizedIndexes = explanation.nodes.filter(node => node.type === 'IndexNode')[0].indexes;
    assertEqual(utilizedIndexes.length, 1);
    assertEqual(utilizedIndexes[0].name, "inverted");

    if (!cmpResFullScanWithIndexes.good || !cmpResView.good || !cmpResInvertedIndex.good) {
      print("Query for collections:", fullScanQuery);
      print("Query for views:", viewQuery);
      print("Query for collections with Inverted Index:", invertedIndexQuery);

      print("Without index:", resFullScanNoIndexes.length);
      print("With index:", resFullScanWithIndexes.length);
      print("With view:", resView.length);
      print("With Inverted Index:", resInvertedIndex.length);

      print("Errors with index: ", cmpResFullScanWithIndexes.msg);
      print("Errors with view: ", cmpResView.msg);
      print("Errors with Inverted Index: ", cmpResInvertedIndex.msg);
    }

    if (disableViewTests) {
      cmpResView.good = true;   // fake goodness
      cmpResInvertedIndex.good = true;   // fake goodness
    }
    return {cmpResFullScanWithIndexes, cmpResView, cmpResInvertedIndex};
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
      analyzers.save("geo_json", analyzerType, analyzerDefinition, ["frequency", "norm", "position"]);

      withIndex.save({ name_1: "name", "value": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      withView.save({ name_1: "name", "value": [{ "nested_1": [{ "nested_2": "foo123"}]}]});

      // ensure inverted indexes for each collections

      let commonInvertedIndexMeta = {};
      if (isEnterprise) {
        commonInvertedIndexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
          {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "name_1",
          {"name": "geo", "analyzer": "geo_json"}
        ]};
      } else {
        commonInvertedIndexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
          {"name": "value[*]"},
          "name_1",
          {"name": "geo", "analyzer": "geo_json"}
        ]};
      }
      withView.ensureIndex(commonInvertedIndexMeta);
      withIndex.ensureIndex(commonInvertedIndexMeta);

      if (isSearchAlias) {
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {type: "inverted", fields: [
            {name: "geo", analyzer: "geo_json"},
            {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}]
          };
        } else {
          indexMeta = {type: "inverted", fields: [
            {name: "geo", analyzer: "geo_json"},
            {"name": "value[*]"}]
          };
        }
        let i = db.UnitTestsGeoWithView.ensureIndex(indexMeta);
        view = db._createView(viewName, "search-alias", {
          indexes: [{collection: "UnitTestsGeoWithView", index: i.name}]
        });
      } else {
        let meta = {};
        if (isEnterprise) {
          meta = {
            links: {
              UnitTestsGeoWithView: {
                fields: {geo: {analyzers: ["geo_json"]}, "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}
              }
            }
          };
        } else {
          meta = {
            links: {
              UnitTestsGeoWithView: {
                fields: {geo: {analyzers: ["geo_json"]}}
              }
            }
          };
        }
        view = db._createView(viewName, "arangosearch", meta);
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
      let analyzers = require("@arangodb/analyzers");
      analyzers.remove("geo_json");
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
    },
    
    testPointOutsideOfPolygon : function () {
      let l = [];
      for (let size = 1; size <= 11; size += 1) {
        for (let lon = 11; lon <= 30; lon += 0.1) {
          l.push({
            geo: {
              type: "Polygon",
              coordinates: [[[lon - size, 10 - size], [lon + size, 10 - size],
                [lon + size, 10 + size], [lon - size, 10 + size],
                [lon - size, 10 - size]]]
            },
            centroid: {type: "Point", coordinates: [lon, 10]}
          });
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
        `FILTER GEO_DISTANCE(d.geo, GEO_POINT(6.93, 10.94)) > 0`,
        `SEARCH ANALYZER(GEO_DISTANCE(d.geo, GEO_POINT(6.93, 10.94)) > 0, "geo_json")`
      );
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with objects of different sizes
////////////////////////////////////////////////////////////////////////////////

    testNearObjectsSizes : function () {
      let l = [];
      for (let size = 0.0001; size <= 10; size *= 10) {
        for (let lon = 14.8; lon <= 15.2; lon += 0.04) {
          l.push({
            geo: {
              type: "Polygon",
              coordinates: [[[lon - size, 10 - size], [lon + size, 10 - size],
                [lon + size, 10 + size], [lon - size, 10 + size],
                [lon - size, 10 - size]]]
            },
            centroid: {type: "Point", coordinates: [lon, 10]}
          });
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test near query with large objects different distance
////////////////////////////////////////////////////////////////////////////////

    testNearSlidingLargeObject : function () {
      let l = [];
      for (let size = 1; size <= 11; size += 1) {
        for (let lon = 11; lon <= 30; lon += 0.1) {
          l.push({
            geo: {
              type: "Polygon",
              coordinates: [[[lon - size, 10 - size], [lon + size, 10 - size],
                [lon + size, 10 + size], [lon - size, 10 + size],
                [lon - size, 10 - size]]]
            },
            centroid: {type: "Point", coordinates: [lon, 10]}
          });
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
        assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
          assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
        let p = {
          type: "Polygon", coordinates: [[[15 - d, 15], [15, 15 - d], [15 + d, 15],
            [15, 15 + d], [15 - d, 15]]]
        };
        //print("d=", d, JSON.stringify(p));
        let c = compare(
          `FILTER GEO_CONTAINS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_CONTAINS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        if (!isInt) {
          assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
        }
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
        let p = {
          type: "Polygon", coordinates: [[[15 - d, 15], [15, 15 + d], [15 + d, 15],
            [15, 15 - d], [15 - d, 15]]]
        };
        //print("d=", d, JSON.stringify(p));
        let c = compare(
          `FILTER GEO_CONTAINS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_CONTAINS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        if (!isInt) {
          assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
        }
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
          assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
        let p = {
          type: "Polygon", coordinates: [[[15 - d, 15], [15, 15 - d], [15 + d, 15],
            [15, 15 + d], [15 - d, 15]]]
        };
        //print("d=", d, JSON.stringify(p));
        let c = compare(
          `FILTER GEO_INTERSECTS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_INTERSECTS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        if (!isInt) {
          assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
        }
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
        let p = {
          type: "Polygon", coordinates: [[[15 - d, 15], [15, 15 + d], [15 + d, 15],
            [15, 15 - d], [15 - d, 15]]]
        };
        //print("d=", d, JSON.stringify(p));
        let c = compare(
          `FILTER GEO_INTERSECTS(${JSON.stringify(p)}, d.geo)`,
          `SEARCH ANALYZER(GEO_INTERSECTS(${JSON.stringify(p)}, d.geo), "geo_json")`
        );
        if (!isInt) {
          assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
        }
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test some areas which are more than half of the earth
////////////////////////////////////////////////////////////////////////////////

    testMoreThanHalf1Grid: function () {
      let l = [];
      for (let lat = 45; lat <= 55; lat += 1) {
        for (let lon = 90; lon <= 100; lon += 1) {
          l.push({geo: {type: "Point", coordinates: [lon, lat]}});
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
        if (!isInt) {
          assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
        }
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
        assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
        assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
        "coordinates": [[[6.537, 50.332], [6.537, 50.376]],
          [[6.621, 50.332], [6.621, 50.376]]]
      };
      let c = compare(
        `FILTER GEO_DISTANCE(${JSON.stringify(multiLineString)}, d.geo) < 100`,
        `SEARCH ANALYZER(GEO_DISTANCE(${JSON.stringify(multiLineString)}, d.geo) < 100, "geo_json")`
      );
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
      multiLineString = {
        "type": "MultiLineString",
        "coordinates": [ [[ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ]],
                         [[ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ]] ] };
      c = compare(
        `FILTER GEO_DISTANCE(${JSON.stringify(multiLineString)}, d.geo) < 100`,
        `SEARCH ANALYZER(GEO_DISTANCE(${JSON.stringify(multiLineString)}, d.geo) < 100, "geo_json")`
      );
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
      let multiPoint = {
        type: "MultiPoint",
        coordinates: [ [ 6.537, 50.332 ], [ 6.537, 50.376 ],
                       [ 6.621, 50.332 ], [ 6.621, 50.376 ] ] };
      c = compare(
        `FILTER GEO_DISTANCE(${JSON.stringify(multiPoint)}, d.geo) < 100`,
        `SEARCH ANALYZER(GEO_DISTANCE(${JSON.stringify(multiPoint)}, d.geo) < 100, "geo_json")`
      );
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
      multiPoint = {
        type: "MultiPoint",
        "coordinates": [ [ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ],
                         [ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ] ] };
      c = compare(
        `FILTER GEO_DISTANCE(${JSON.stringify(multiPoint)}, d.geo) < 100`,
        `SEARCH ANALYZER(GEO_DISTANCE(${JSON.stringify(multiPoint)}, d.geo) < 100, "geo_json")`
      );
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
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
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
      multiLineString = {
        "type": "MultiLineString",
        "coordinates": [ [[ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ]],
                         [[ 37.614323, 55.70652 ], [ 37.615825, 55.70652 ]] ] };

      c = compare(
        `FILTER GEO_IN_RANGE(${JSON.stringify(multiLineString)}, d.geo, 0, 100)`,
        `SEARCH ANALYZER(GEO_IN_RANGE(${JSON.stringify(multiLineString)}, d.geo, 0, 100), "geo_json")`
      );
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
      let multiPoint = {
        type: "MultiPoint",
        coordinates: [ [ 6.537, 50.332 ], [ 6.537, 50.376 ],
                       [ 6.621, 50.332 ], [ 6.621, 50.376 ] ] };

      c = compare(
        `FILTER GEO_IN_RANGE(${JSON.stringify(multiPoint)}, d.geo, 0, 100)`,
        `SEARCH ANALYZER(GEO_IN_RANGE(${JSON.stringify(multiPoint)}, d.geo, 0, 100), "geo_json")`
      );
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
      multiPoint = {
        type: "MultiPoint",
        "coordinates": [ [ 37.614323, 55.705898 ], [ 37.615825, 55.705898 ],
          [37.614323, 55.70652], [37.615825, 55.70652]]
      };

      c = compare(
        `FILTER GEO_IN_RANGE(${JSON.stringify(multiPoint)}, d.geo, 0, 100)`,
        `SEARCH ANALYZER(GEO_IN_RANGE(${JSON.stringify(multiPoint)}, d.geo, 0, 100), "geo_json")`
      );
      assertTrue(c.cmpResFullScanWithIndexes.good && c.cmpResView.good && c.cmpResInvertedIndex.good, c.cmpResFullScanWithIndexes.msg + c.cmpResView.msg + c.cmpResInvertedIndex.msg);
    },
  };
  };
}());
