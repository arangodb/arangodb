/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Hyderabad, India
// / Copyright 2004-2014 triAGENS GmbH, Hyderabad, India
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Hyderabad, India
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");

const findExecutionNodes = helper.findExecutionNodes;
const db = require("internal").db;

function geoContainsFieldFirstTimezonesTestSuite() {
  const colName = "UnitTestsGeoContainsFieldFirstTimezones";

  const timezoneDocs = [
    {
      tzid: "Asia/Kolkata",
      geometry: {
        type: "Polygon",
        coordinates: [[[68, 6], [98, 6], [98, 38], [68, 38], [68, 6]]]
      }
    },
    {
      tzid: "Europe/London",
      geometry: {
        type: "Polygon",
        coordinates: [[[-10, 49], [3, 49], [3, 60], [-10, 60], [-10, 49]]]
      }
    },
    {
      tzid: "America/New_York",
      geometry: {
        type: "Polygon",
        coordinates: [[[-80, 35], [-70, 35], [-70, 45], [-80, 45], [-80, 35]]]
      }
    },
    {
      tzid: "Australia/Sydney",
      geometry: {
        type: "Polygon",
        coordinates: [[[150, -38], [154, -38], [154, -32], [150, -32], [150, -38]]]
      }
    }
  ];

  const indiaRegion = {
    type: "Polygon",
    coordinates: [[[77, 12], [78, 12], [78, 13], [77, 13], [77, 12]]]
  };

  let geocol;

  const hasNoFilterNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0,
      query.string + " Has no FilterNode");
  };
  const hasIndexNode = function (plan, query) {
    const rn = findExecutionNodes(plan, "IndexNode");
    assertEqual(rn.length, 1, query.string + " Has IndexNode");
    assertEqual(rn[0].indexes.length, 1);
    assertTrue(rn[0].indexes[0].type === "geo");
  };

  const sortedTzids = function (tzids) {
    return tzids.slice().sort();
  };

  const sortedByTzid = function (rows) {
    return rows.slice().sort(function (l, r) {
      return l.tzid < r.tzid ? -1 : (l.tzid > r.tzid ? 1 : 0);
    });
  };

  return {
    setUpAll: function () {
      db._drop(colName);
      geocol = db._create(colName);
      geocol.ensureIndex({ type: "geo", fields: ["geometry"], geoJson: true });
      geocol.insert(timezoneDocs);
    },

    tearDownAll: function () {
      db._drop(colName);
      geocol = null;
    },

    testTimezonesDatasetTypes: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            RETURN { tzid: doc.tzid, type: doc.geometry.type }`,
        bindVars: { "@cc": colName }
      };

      const expected = [
        { tzid: "America/New_York", type: "Polygon" },
        { tzid: "Asia/Kolkata", type: "Polygon" },
        { tzid: "Australia/Sydney", type: "Polygon" },
        { tzid: "Europe/London", type: "Polygon" }
      ];

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(sortedByTzid(expected), sortedByTzid(result.toArray()), query.string);
    },

    testGeoContainsFieldFirstIndia: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(77.5946, 12.9716))
            RETURN doc.tzid`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedTzids(["Asia/Kolkata"]),
        sortedTzids(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstLondon: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(-0.1276, 51.5072))
            RETURN doc.tzid`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedTzids(["Europe/London"]),
        sortedTzids(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstNewYork: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(-74.0060, 40.7128))
            RETURN doc.tzid`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedTzids(["America/New_York"]),
        sortedTzids(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstSydney: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(151.2093, -33.8688))
            RETURN doc.tzid`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedTzids(["Australia/Sydney"]),
        sortedTzids(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstPolygonInPolygon: function () {
      const query = {
        string: `
          LET indiaRegion = @region
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, indiaRegion)
            RETURN doc.tzid`,
        bindVars: { "@cc": colName, region: indiaRegion }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedTzids(["Asia/Kolkata"]),
        sortedTzids(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstContainmentSemantics: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            RETURN { tzid: doc.tzid,
                     containsIndia: GEO_CONTAINS(doc.geometry, GEO_POINT(77.5946, 12.9716)) }`,
        bindVars: { "@cc": colName }
      };

      const expected = [
        { tzid: "America/New_York", containsIndia: false },
        { tzid: "Asia/Kolkata", containsIndia: true },
        { tzid: "Australia/Sydney", containsIndia: false },
        { tzid: "Europe/London", containsIndia: false }
      ];

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(sortedByTzid(expected), sortedByTzid(result.toArray()), query.string);
    },

    testGeoContainsFieldFirstBoundary: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(68, 20))
            RETURN doc.tzid`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedTzids(["Asia/Kolkata"]),
        sortedTzids(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstGeoIndexBehavior: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(77.5946, 12.9716))
            RETURN doc`,
        bindVars: { "@cc": colName }
      };

      const plan = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).explain();
      hasIndexNode(plan, query);
      hasNoFilterNode(plan, query);
    }
  };
}

jsunity.run(geoContainsFieldFirstTimezonesTestSuite);

return jsunity.done();
