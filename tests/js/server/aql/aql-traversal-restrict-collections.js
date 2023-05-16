/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = require("@arangodb").db;
const errors = internal.errors;

const vc1Name = "v1";
const vc2Name = "v2";
const ec1Name = "e1";
const ec2Name = "e2";
const viewName = "vvvv";
const gn = "connectedComponentsGraph";

const cleanup = function() {
  db._dropView(viewName);
  db._drop(vc1Name);
  db._drop(vc2Name);
  db._drop(ec1Name);
  db._drop(ec2Name);

  try {
    require("@arangodb/general-graph")._drop(gn, true);
  } catch (err) {
  }
};

const createBaseGraph = function () {
  const vc1 = db._create(vc1Name);
  const vc2 = db._create(vc2Name);
  const ec1 = db._createEdgeCollection(ec1Name);
  const ec2 = db._createEdgeCollection(ec2Name);

  let docs = [];
  for (let i = 0; i < 10; ++i) {
    docs.push({ _key: "node_" + i, value: i });
  }
  vc1.insert(docs);
  vc2.insert(docs);

  docs = [];
  for (let i = 0; i < 10; ++i) {
    docs.push({_from: vc1Name + "/node_" + i, _to: vc2Name + "/node_" + i});
    docs.push({_from: vc2Name + "/node_" + i, _to: vc1Name + "/node_" + i});
    if (i < 9) {
      docs.push({_from: vc1Name + "/node_" + i, _to: vc1Name + "/node_" + (i + 1)});
    }
    if (i > 0) {
      docs.push({_from: vc2Name + "/node_" + i, _to: vc2Name + "/node_" + (i - 1)});
    }
  }
  ec1.insert(docs);
};


function vertexCollectionRestrictionSuite() {
  return {
    setUpAll : function () {
      cleanup();
      createBaseGraph();
      db._createView(viewName, "arangosearch", {});

      require("@arangodb/graph-examples/example-graph").loadGraph(gn);
    },

    tearDownAll : function () {
      cleanup();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test object access for path object
////////////////////////////////////////////////////////////////////////////////

    testNoRestriction : function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name}).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },

    testNoPracticalRestriction: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {vertexCollections: ["${vc1Name}", "${vc2Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },

    testRestrict1: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {vertexCollections: ["${vc1Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [
        vc1Name + "/node_8",
      ];

      assertEqual(actual, expected);
    },

    testRestrict2: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {vertexCollections: ["${vc2Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [
        vc2Name + "/node_3",
      ];

      assertEqual(actual, expected);
    },

    testNoRestrictionBfs : function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {bfs: true}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name}).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },

    testNoPracticalRestrictionBfs: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {bfs: true, vertexCollections: ["${vc1Name}", "${vc2Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },

    testRestrict1Bfs: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {bfs: true, vertexCollections: ["${vc1Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [
        vc1Name + "/node_8",
      ];

      assertEqual(actual, expected);
    },

    testRestrict2Bfs: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {bfs: true, vertexCollections: ["${vc2Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [
        vc2Name + "/node_3",
      ];

      assertEqual(actual, expected);
    },
    
    testRestrictOnlySomeVertexCollections: function () {
      {
        // specify only vc1
        const query = `WITH @@vc1, @@vc2
                       FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                         OPTIONS {vertexCollections: ["${vc1Name}"]}
                         SORT v._id
                         RETURN DISTINCT v._id`;

        const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
        const expected = [
          vc1Name + "/node_8",
        ];

        assertEqual(actual, expected);
      }
      
      {
        // specify only vc2
        const query = `WITH @@vc1, @@vc2
                       FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                         OPTIONS {vertexCollections: ["${vc2Name}"]}
                         SORT v._id
                         RETURN DISTINCT v._id`;

        const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
        const expected = [
          vc2Name + "/node_3",
        ];

        assertEqual(actual, expected);
      }
    },
    
    testRestrictEmptyVertexCollections: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {vertexCollections: []}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },
    
    testRestrictInvalidVertexCollections: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       // specify non-existing vertex collections
                       OPTIONS {vertexCollections: ["piff", "paff"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      try {
        db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },
    
    testRestrictInvalidVertexCollectionType: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       // specify an edge collection instead of a vertex collection
                       OPTIONS {vertexCollections: ["${ec1Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [];
      assertEqual(actual, expected);
    },
    
    testRestrictViewAsVertexCollection: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       // specify a view collection instead of a vertex collection
                       OPTIONS {vertexCollections: ["${viewName}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      try {
        db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code, err.errorNum);
      }
    },
    
    testRestrictOnlySomeEdgeCollections: function () {
      {
        // specify only ec1
        const query = `WITH @@vc1, @@vc2
                       FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1, @@ec2
                         OPTIONS {edgeCollections: ["${ec1Name}"]}
                         SORT v._id
                         RETURN DISTINCT v._id`;

        const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name, '@ec2': ec2Name }).toArray();
        const expected = [
          vc1Name + "/node_4",
          vc1Name + "/node_6",
          vc1Name + "/node_8",
          vc2Name + "/node_3",
          vc2Name + "/node_5",
          vc2Name + "/node_7",
        ];

        assertEqual(actual, expected);
      }
      
      {
        // specify only ec2
        const query = `WITH @@vc1, @@vc2
                       FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1, @@ec2
                         OPTIONS {edgeCollections: ["${ec2Name}"]}
                         SORT v._id
                         RETURN DISTINCT v._id`;

        const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name, '@ec2': ec2Name }).toArray();
        const expected = [];
        assertEqual(actual, expected);
      }
    },
    
    testRestrictEmptyEdgeCollections: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       OPTIONS {edgeCollections: []}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name }).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },
    
    testRestrictInvalidEdgeCollections: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       // specify non-existing edge collections
                       OPTIONS {edgeCollections: ["piff", "paff"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      try {
        db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },
    
    testRestrictInvalidEdgeCollectionType: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       // specify a vertex collection instead of an edge collection
                       OPTIONS {edgeCollections: ["${vc1Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      try {
        db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code, err.errorNum);
      }
    },
    
    testRestrictViewAsEdgeCollection: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       // specify a view instead of an edge collection
                       OPTIONS {edgeCollections: ["${viewName}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      try {
        db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code, err.errorNum);
      }
    },
    
    testRestrictDifferentEdgeCollection: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec1
                       // specify different edge collection
                       OPTIONS {edgeCollections: [@ec2]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      try {
        db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec1': ec1Name, 'ec2': ec2Name });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.code, err.errorNum);
      }
    },

    testAccessCollectionNamedGraphInvalidType: function () {
      try {
        db._query(`FOR v IN ANY "components/A1" GRAPH "${gn}" OPTIONS { edgeCollections: "${vc1Name}" } RETURN v`);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code, err.errorNum);
      }
    },
  
    testAccessCollectionNamedGraphInvalidEdgeCollection: function () {
      try {
        db._query(`FOR v IN ANY "components/A1" GRAPH "${gn}" OPTIONS { edgeCollections: "piff" } RETURN v`);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },
    
    testAccessCollectionNamedGraphEdgeCollectionNotInGraph: function () {
      try {
        db._query(`FOR v IN ANY "components/A1" GRAPH "${gn}" OPTIONS { edgeCollections: "${ec1Name}" } RETURN v`);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.code, err.errorNum);
      }
    },
    
    testAccessCollectionNamedGraphVertexCollectionNotInGraph: function () {
      try {
        db._query(`FOR v IN ANY "components/A1" GRAPH "${gn}" OPTIONS { vertexCollections: "${vc1Name}" } RETURN v`);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code, err.errorNum);
      }
    },

  };
}

jsunity.run(vertexCollectionRestrictionSuite);

return jsunity.done();
