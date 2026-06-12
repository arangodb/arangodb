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
const isCluster = require("internal").isCluster();

function geoContainsFieldFirstTestSuite() {
  const colName = "UnitTestsGeoContainsFieldFirst";

  const containmentDocs = [
    {
      _key: "big_square",
      geometry: {
        type: "Polygon",
        coordinates: [[[10, 10], [30, 10], [30, 30], [10, 30], [10, 10]]]
      }
    },
    {
      _key: "small_square",
      geometry: {
        type: "Polygon",
        coordinates: [[[12, 12], [18, 12], [18, 18], [12, 18], [12, 12]]]
      }
    },
    {
      _key: "long_corridor",
      geometry: {
        type: "Polygon",
        coordinates: [[[40, 10], [80, 10], [80, 12], [40, 12], [40, 10]]]
      }
    },
    {
      _key: "large_region",
      geometry: {
        type: "Polygon",
        coordinates: [[[-50, -50], [50, -50], [50, 50], [-50, 50], [-50, -50]]]
      }
    }
  ];

  const innerPolygonCoords = [[[14, 14], [16, 14], [16, 16], [14, 16], [14, 14]]];
  const corridorPolygonCoords = [[[45, 10.5], [47, 10.5], [47, 11.5], [45, 11.5], [45, 10.5]]];
  const innerMultiPolygonCoords = [
    [[[14, 14], [16, 14], [16, 16], [14, 16], [14, 14]]],
    [[[13, 13], [14, 13], [14, 14], [13, 14], [13, 13]]]
  ];

  let geocol;

  const hasNoFilterNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0,
      query.string + " Has no FilterNode");
  };
  const hasNoSortNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "SortNode").length, 0,
      query.string + " Has no SortNode");
  };
  const hasIndexNode = function (plan, query) {
    const rn = findExecutionNodes(plan, "IndexNode");
    assertEqual(rn.length, 1, query.string + " Has IndexNode");
    assertEqual(rn[0].indexes.length, 1);
    assertTrue(rn[0].indexes[0].type === "geo");
  };

  const sortedKeys = function (keys) {
    return keys.slice().sort();
  };

  const sortedByKey = function (rows) {
    return rows.slice().sort(function (l, r) {
      return l.key < r.key ? -1 : (l.key > r.key ? 1 : 0);
    });
  };

  return {
    setUpAll: function () {
      db._drop(colName);
      geocol = db._create(colName);
      geocol.ensureIndex({ type: "geo", fields: ["geometry"], geoJson: true });
      geocol.insert(containmentDocs);
    },

    tearDownAll: function () {
      db._drop(colName);
      geocol = null;
    },

    testGeoContainsIndexedFieldFirst: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(15, 15))
            RETURN doc._key`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedKeys(["big_square", "small_square", "large_region"]),
        sortedKeys(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstReturnsCorrectMatches: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            RETURN { key: doc._key,
                     contains: GEO_CONTAINS(doc.geometry, GEO_POINT(15, 15)) }`,
        bindVars: { "@cc": colName }
      };

      const expected = [
        { key: "big_square", contains: true },
        { key: "small_square", contains: true },
        { key: "long_corridor", contains: false },
        { key: "large_region", contains: true }
      ];

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(sortedByKey(expected), sortedByKey(result.toArray()), query.string);
    },

    testGeoContainsFieldFirstPointInPolygon: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(45, 11))
            RETURN doc._key`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedKeys(["long_corridor", "large_region"]),
        sortedKeys(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstContainmentSemantics: function () {
      const query = {
        string: `
          LET pt = GEO_POINT(15, 15)
          FOR doc IN @@cc
            RETURN { key: doc._key, result: GEO_CONTAINS(doc.geometry, pt) }`,
        bindVars: { "@cc": colName }
      };

      const expected = [
        { key: "big_square", result: true },
        { key: "small_square", result: true },
        { key: "long_corridor", result: false },
        { key: "large_region", result: true }
      ];

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(sortedByKey(expected), sortedByKey(result.toArray()), query.string);
    },

    testGeoContainsIndexedGeometryContainsPoint: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(0, 0))
            RETURN doc._key`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(sortedKeys(["large_region"]), sortedKeys(result.toArray()), query.string);
    },

    testGeoContainsFieldFirstGeoPolygon: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POLYGON(@coords))
            RETURN doc._key`,
        bindVars: { "@cc": colName, coords: innerPolygonCoords }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedKeys(["big_square", "small_square", "large_region"]),
        sortedKeys(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstGeoMultiPoint: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_MULTIPOINT([[15, 15], [16, 16]]))
            RETURN doc._key`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedKeys(["big_square", "small_square", "large_region"]),
        sortedKeys(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstGeoMultiPolygon: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_MULTIPOLYGON(@coords))
            RETURN doc._key`,
        bindVars: { "@cc": colName, coords: innerMultiPolygonCoords }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedKeys(["big_square", "small_square", "large_region"]),
        sortedKeys(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstGeoPolygonInCorridor: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POLYGON(@coords))
            RETURN doc._key`,
        bindVars: { "@cc": colName, coords: corridorPolygonCoords }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      assertEqual(
        sortedKeys(["long_corridor", "large_region"]),
        sortedKeys(result.toArray()),
        query.string);
    },

    testGeoContainsFieldFirstGeoIndexBehavior: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(15, 15))
            RETURN doc`,
        bindVars: { "@cc": colName }
      };

      const plan = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).explain();
      hasIndexNode(plan, query);
      hasNoFilterNode(plan, query);
    },

    testGeoContainsFieldFirstSortByDistance: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(15, 15))
            SORT GEO_DISTANCE(GEO_POINT(15, 15), doc.geometry)
            RETURN GEO_DISTANCE(GEO_POINT(15, 15), doc.geometry)`,
        bindVars: { "@cc": colName }
      };

      const result = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).execute();
      const distances = result.toArray().map(function (d) {
        return parseFloat(d.toFixed(5));
      });
      let prev = -1;
      distances.forEach(function (d) {
        assertTrue(d >= prev, d + " >= " + prev);
        prev = d;
      });
    },

    testGeoContainsFieldFirstSortGeoIndexBehavior: function () {
      const query = {
        string: `
          FOR doc IN @@cc
            FILTER GEO_CONTAINS(doc.geometry, GEO_POINT(15, 15))
            SORT GEO_DISTANCE(GEO_POINT(15, 15), doc.geometry)
            LIMIT 5
            RETURN doc`,
        bindVars: { "@cc": colName }
      };

      const plan = db._createStatement({
        query: query.string,
        bindVars: query.bindVars
      }).explain();
      hasIndexNode(plan, query);
      hasNoFilterNode(plan, query);
      if (!isCluster) {
        hasNoSortNode(plan, query);
      }
    },

    testGeoContainsFieldFirstGeoConstructorsGeoIndexBehavior: function () {
      const queries = [
        {
          string: `
            FOR doc IN @@cc
              FILTER GEO_CONTAINS(doc.geometry, GEO_POLYGON(@coords))
              RETURN doc`,
          bindVars: { "@cc": colName, coords: innerPolygonCoords }
        },
        {
          string: `
            FOR doc IN @@cc
              FILTER GEO_CONTAINS(doc.geometry, GEO_MULTIPOINT([[15, 15], [16, 16]]))
              RETURN doc`,
          bindVars: { "@cc": colName }
        },
        {
          string: `
            FOR doc IN @@cc
              FILTER GEO_CONTAINS(doc.geometry, GEO_MULTIPOLYGON(@coords))
              RETURN doc`,
          bindVars: { "@cc": colName, coords: innerMultiPolygonCoords }
        }
      ];

      queries.forEach(function (query) {
        const plan = db._createStatement({
          query: query.string,
          bindVars: query.bindVars
        }).explain();
        hasIndexNode(plan, query);
        hasNoFilterNode(plan, query);
      });
    }
  };
}

jsunity.run(geoContainsFieldFirstTestSuite);

return jsunity.done();
