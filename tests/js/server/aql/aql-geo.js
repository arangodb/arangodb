/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

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

  function insertAll(x) {
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
    while (i < a.length && j <= b.length) {
      if (a[i] < b[j]) {
        good = false;
        msg += "Found key " + a[i] + " in '" + namea + "' but not in '"
               + nameb + "'";
        ++i;
      } else if (a[i] > b[j]) {
        good = false;
        msg += "Found key " + b[j] + " in '" + nameb + "' but not in '"
               + namea + "'";
        ++j;
      } else {
        ++i;
        ++j;
      }
    }
    while (i < a.length) {
      good = false;
      msg += "Found key " + a[i] + " in '" + namea + "' but not in '"
             + nameb + "'";
      ++i;
    }
    while (j < b.length) {
      good = false;
      msg += "Found key " + b[j] + " in '" + nameb + "' but not in '"
             + namea + "'";
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
/// @brief test near function
////////////////////////////////////////////////////////////////////////////////

    testSetup : function () {
      insertAll([{geo: { type: "Point", coordinates: [50, 50] } }]);
      compare(
        `FILTER GEO_DISTANCE([50, 50], d.geo < 1000)`,
        `SEARCH ANALYZER(GEO_DISTANCE([50, 50], d.geo) < 1000, "geo_json")`
      );
    },
  };

}

jsunity.run(geoSuite);

return jsunity.done();
