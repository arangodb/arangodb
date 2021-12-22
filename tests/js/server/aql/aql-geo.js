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

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var errors = require("@arangodb").errors;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function geoSuite () {

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
    withView.insert(
      {_key:"sentinel", geo:{"type":"Point", coordinates: [-13, -13]}});
    // Wait until sentinel visible in view:
    while (true) {
      let res = getQueryResults(`
        FOR d IN ${viewName}
          SEARCH ANALYZER(GEO_DISTANCE([-13, -13], d.geo) < 100, "geo_json")
          RETURN d._key`);
      if (res.length === 1 && res[0] === "sentinel") {
        break;
      }
      print("Waiting for arangosearch index to have sentinel...");
      require("internal").wait(0.5);
    }
    withView.remove("sentinel");
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
    }
    while (i < a.length) {
      good = false;
      msg += "Found key " + a[i] + " in '" + namea + "' but not in '"
             + nameb + "'\n";
      ++i;
    }
    while (j < b.length) {
      good = false;
      msg += "Found key " + b[j] + " in '" + nameb + "' but not in '"
             + namea + "'\n";
      ++j;
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
      print("Without index:", wo);
      print("With index:", wi);
      print("With view:", wv);
      print("Errors with index: ", oi.msg);
      print("Errors with view: ", ov.msg);
      //require("internal").wait(3600);
    }
    assertTrue(oi.good && ov.good, oi.msg + ov.msg);
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
      withIndex.ensureIndex({type:"geo", geoJson: true, fields: ["geo"]});
      withView = db._create(collWithView);
      let analyzers = require("@arangodb/analyzers");
      let a = analyzers.save("geo_json", "geojson", {}, ["frequency", "norm", "position"]);
      view = db._createView(viewName, "arangosearch", {
        links: { UnitTestsGeoWithView: {
          fields: { geo: {analyzers: ["geo_json"]}}}}});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down all
////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._dropView(viewName);
      db._drop(collWithoutIndex);
      db._drop(collWithIndex);
      db._drop(collWithView);
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
      compare(
        `FILTER GEO_DISTANCE([50, 50], d.geo) < 5000`,
        `SEARCH ANALYZER(GEO_DISTANCE([50, 50], d.geo) < 5000, "geo_json")`
      );
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
      compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 5000`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 5000, "geo_json")`
      );
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
      compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 666666`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 666666, "geo_json")`
      );
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
      compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 666666 &&
                GEO_DISTANCE([15, 15], d.geo) >= 300000`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 666666 &&
                         GEO_DISTANCE([15, 15], d.geo) >= 300000, "geo_json")`
      );
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
      compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) >= 300000`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) >= 300000, "geo_json")`
      );
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
      compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 666666
         SORT GEO_DISTANCE([15, 15], d.geo) DESC`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 666666, "geo_json")
         SORT GEO_DISTANCE([15, 15], d.geo) DESC`
      );
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
      compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) <= 666666 &&
                GEO_DISTANCE([15, 15], d.geo) >= 300000
         SORT GEO_DISTANCE([15, 15], d.geo) DESC`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) <= 666666 &&
                         GEO_DISTANCE([15, 15], d.geo) >= 300000, "geo_json")
         SORT GEO_DISTANCE([15, 15], d.geo) DESC`
      );
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
      compare(
        `FILTER GEO_DISTANCE([15, 15], d.geo) >= 300000`,
        `SEARCH ANALYZER(GEO_DISTANCE([15, 15], d.geo) >= 300000, "geo_json")`
      );
    },

  };
}

jsunity.run(geoSuite);

return jsunity.done();
