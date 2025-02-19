/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Markus Pfeiffer
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const vertexCollectionName = "person";
const edgeCollectionName = "link";

const cleanup = function() {
  db._drop(vertexCollectionName);
  db._drop(edgeCollectionName);
};

const createBaseGraph = function () {
  const vc = db._create(vertexCollectionName);
  const ec = db._createEdgeCollection(edgeCollectionName);

  vc.save([{"_key":"287057244","name":"Gerald","age":18},
           {"_key":"287056982","name":"Chris","age":34},
           {"_key":"287056986","name":"Minerva","age":54},
           {"_key":"287056980","name":"George","age":53},
           {"_key":"287056984","name":"Ella","age":65},
           {"_key":"287056990","name":"July","age":42},
           {"_key":"287056976","name":"Peter","age":5},
           {"_key":"287056988","name":"Kay","age":4}]);

  ec.save([{"_key":"287057504","_from":"person/287056980","_to":"person/287056988","field":"dogs"},
           {"_key":"287057440","_from":"person/287057244","_to":"person/287056984","field":"home"},
           {"_key":"287057500","_from":"person/287056986","_to":"person/287056976","field":"dogs"},
           {"_key":"287057444","_from":"person/287056982","_to":"person/287056990","field":"home"}]);
};

function pathTraversalObjectRegressionSuite() {
  return {

    setUpAll : function () {
      cleanup();
      createBaseGraph();
    },

    tearDownAll : function () {
      cleanup();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test object access for path object
////////////////////////////////////////////////////////////////////////////////

    testPathObjectEdgesAccessPrefixNotOK : function () {
      const query = `FOR person IN @@person
                       FOR v, e, p IN 1..1 OUTBOUND person @@links
                         FILTER p.edge[0].field == "dogs"
                         RETURN {
                           field: p.edge[0].field,
                           cond: p.edge[0].field == "dogs"
                         }`;

      var actual = db._query(query, {'@person': vertexCollectionName,
                                     '@links': edgeCollectionName});
      assertEqual(actual.toArray(), []);
    },

    testPathObjectVertexAccessPrefixNotOK : function () {
      const query = `FOR person IN @@person
                       FOR v, e, p IN 1..1 OUTBOUND person @@links
                         FILTER p.verti[0].name == "Gerald"
                         RETURN {
                           name: p.verti[0].name,
                           cond: p.verti[0].name == "Gerald"
                         }`;

      var actual = db._query(query, {'@person': vertexCollectionName,
                                     '@links': edgeCollectionName});
      assertEqual(actual.toArray(), []);
    },

    testPathObjectEdgesAccessOK : function () {
      const query = `FOR person IN @@person
                       FOR v, e, p IN 1..1 OUTBOUND person @@links
                         FILTER p.edges[0].field == "dogs"
                         RETURN {
                           field: p.edges[0].field,
                           cond: p.edges[0].field == "dogs"
                         }`;

      var actual = db._query(query, {'@person': vertexCollectionName,
                                     '@links': edgeCollectionName});
      assertEqual(actual.toArray(), [{field: "dogs", cond: true},
                                     {field: "dogs", cond: true}]);
    },

    testPathObjectVertexAccessOK : function () {
      const query = `FOR person IN @@person
                       FOR v, e, p IN 1..1 OUTBOUND person @@links
                         FILTER p.vertices[0].name == "Gerald"
                         RETURN {
                           name: p.vertices[0].name,
                           cond: p.vertices[0].name == "Gerald"
                         }`;

      var actual = db._query(query, {'@person': vertexCollectionName,
                                     '@links': edgeCollectionName});
      assertEqual(actual.toArray(), [{name: "Gerald", cond: true}]);
    },

    // This tests a regression occurred in ArangoDB 3.11 reported by a
    // user in the GitHub Issue #19175.
    testTraversalConditionFilterNormalizationRegression: function () {
      const query = `
        LET filter0 = (FOR m0 IN [] FILTER "" IN m0.kinds RETURN m0)
        FOR v IN filter0 FOR z IN 0..1 OUTBOUND v ${edgeCollectionName}
          COLLECT x = v WITH COUNT INTO counter1
          FILTER counter1 == 0
          RETURN x
      `;
      let actual = db._query(query);
      assertEqual(actual.toArray(), []);
    }
  };
}

jsunity.run(pathTraversalObjectRegressionSuite);

return jsunity.done();
