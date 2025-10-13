/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2025
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db,
  indexId;
var gm = require("@arangodb/general-graph");

const graphName = "WithTestGraph";
const vertexCollectionName1 = "VertexCollection1";
const vertexCollectionName2 = "VertexCollection2";
const vertexCollectionName3 = "VertexCollection3";
const edgeCollectionName = "EdgeCollection";
const edgeCollectionName2 = "EdgeCollection2";
const edgeCollectionName3 = "Edgecollection3";

var cleanup = function() {
  try {
    gm._drop(graphName, true);
  } catch (e) {
  }
  db._drop(vertexCollectionName1);
  db._drop(vertexCollectionName2);
  db._drop(vertexCollectionName3);
  db._drop(edgeCollectionName);
  db._drop(edgeCollectionName2);
  db._drop(edgeCollectionName3);
};

var createBaseGraph = function() {
    gm._create(graphName, [gm._relation(edgeCollectionName,
                                        [vertexCollectionName1, vertexCollectionName2, vertexCollectionName3],
                                        [vertexCollectionName2, vertexCollectionName3]),
                           gm._relation(edgeCollectionName2,
                                        vertexCollectionName3,
                                        vertexCollectionName2)], [], {});

  db._createEdgeCollection(edgeCollectionName3);

  db[vertexCollectionName1].insert([
    {
      _key: "A",
      _id: vertexCollectionName1 + "/A",
      name: "Danny",
      surname: "Carey"
    },
    {
      _key: "B",
      _id: vertexCollectionName1 + "/B",
      name: "Mike",
      surname: "Portnoy"
    },
    {
      _key: "C",
      _id: vertexCollectionName1 + "/C",
      name: "Ringo",
      surname: "Starr",
    }
  ]);
  db[edgeCollectionName].insert([
    {
      _id: edgeCollectionName + "/400",
      _from: vertexCollectionName1 + "/A",
      _to: vertexCollectionName2 + "/ONE",
    },
    {
      _id: edgeCollectionName + "/402",
      _from: vertexCollectionName1 + "/C",
      _to: vertexCollectionName2 + "/TWO",
    },
    {
      _id: edgeCollectionName + "/404",
      _from: vertexCollectionName1 + "/A",
      _to: vertexCollectionName2 + "/THREE",
    },
    {
      _id: edgeCollectionName + "/406",
      _from: vertexCollectionName1 + "/C",
      _to: vertexCollectionName2 + "/ONE",
    },
    {
      _id: edgeCollectionName + "/407",
      _from: vertexCollectionName1 + "/A",
      _to: vertexCollectionName3 + "/ALPHA"
    },
    {
      _id: edgeCollectionName + "/407",
      _from: vertexCollectionName3 + "/ALPHA",
      _to: vertexCollectionName3 + "/ALPHA"
    }
  ]);
  db[vertexCollectionName2].insert([
    {
      _key: "ONE",
      _id: vertexCollectionName2 + "/ONE",
      name: "phone",
    },
    {
      _key: "TWO",
      _id: vertexCollectionName2 + "/TWO",
      name: "car",
    },
    {
      _key: "THREE",
      _id: vertexCollectionName2 + "/THREE",
      name: "chair",
    }
  ]);
  db[edgeCollectionName2].insert([
    {
      _from: vertexCollectionName3 + "/ALPHA",
      _to: vertexCollectionName2 + "/ONE",
    },
    {
      _from: vertexCollectionName3 + "/GAMMA",
      _to: vertexCollectionName2 + "/TWO",
    },
    {
      _from: vertexCollectionName3 + "/ALPHA",
      _to: vertexCollectionName2 + "/THREE",
    },
    {
      _from: vertexCollectionName3 + "/BETA",
      _to: vertexCollectionName2 + "/ONE",
    }
  ]);
  db[vertexCollectionName3].insert([
    {
      _key: "ALPHA",
      _id: vertexCollectionName3 + "/ALPHA"
    },
    {
      _key: "BETA",
      _id: vertexCollectionName3 + "/BETA"
    },
    {
      _key: "GAMMA",
      _id: vertexCollectionName3 + "/GAMMA"
    }
  ]);
  db[edgeCollectionName3].insert([
    {
      _from: vertexCollectionName1 + "/A",
      _to: vertexCollectionName2 + "/ONE"
    }
  ]);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function withClauseTestSuite() {
  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function() {
      cleanup();
      createBaseGraph();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function() {
      cleanup();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with clause not needed with named graph
    ////////////////////////////////////////////////////////////////////////////////
    testNamedGraphWithClauseNotNeeded: function() {
      const startNode = "VertexCollection1/A";
      const query = `FOR v,e,p IN 1..1 ANY "${startNode}" GRAPH ${graphName}
                       RETURN p.vertices[*]._id`;

      var actual = db._query(query).toArray();
      assertEqual(actual, [
          [
              "VertexCollection1/A", 
              "VertexCollection3/ALPHA" 
          ],
          [ 
              "VertexCollection1/A", 
              "VertexCollection2/THREE" 
          ], 
          [ 
              "VertexCollection1/A", 
              "VertexCollection2/ONE" 
          ]]);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief WITH clause not needed: vertexCollections option gives the only
    ///        required vertex collection.
    ///        If the OPTIONS are not given, the query works too, but the query
    ///        engine automatically adds all vertex collections to the query, as
    ///        they are part of the named graph definition including EdgeCollection
    ////////////////////////////////////////////////////////////////////////////////
    testWithClauseNotNeededOptions: function() {
      const startNode = "VertexCollection3/ALPHA";
      const query = `FOR v,e,p IN 1..1 OUTBOUND "${startNode}" ${edgeCollectionName}
                       OPTIONS { vertexCollections: "${vertexCollectionName3}" }
                       RETURN p.vertices[*]._id`;

      var actual = db._query(query).toArray();
      assertEqual(actual, [
          [ 
              "VertexCollection3/ALPHA",
              "VertexCollection3/ALPHA"
          ]]); 
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Should fail requiring a WITH clause because the usage of the
    /// vertexCollections clause prevents lookup in the named graph.
    ////////////////////////////////////////////////////////////////////////////////
    testWithClauseNotNeededOptionsRestriction: function() {
      const startNode = "VertexCollection3/ALPHA";
      const query = `FOR v,e,p IN 1..1 OUTBOUND "${startNode}" ${edgeCollectionName}
                       OPTIONS { vertexCollections: "${vertexCollectionName1}" }
                       RETURN p.vertices[*]._id`;

      try {
        const actual = db._query(query).toArray();
        
        if(require("internal").isCluster()) {
          assertTrue(false);
        }
      } catch (err) {
        assertEqual(internal.errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum, JSON.stringify(err));
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with clause not needed with edge collection if a named graph
    //  exists
    ////////////////////////////////////////////////////////////////////////////////
    testWithClauseNotNeeded: function() {
      const startNode = "VertexCollection1/A";
      const query = `FOR v,e,p IN 1..1 ANY "${startNode}" ${edgeCollectionName}
                       RETURN SORTED(p.vertices[*]._id)`;

      var actual = db._query(query).toArray();
      assertEqual(actual, [
          [ 
              "VertexCollection1/A", 
              "VertexCollection3/ALPHA" 
          ], 
          [ 
              "VertexCollection1/A", 
              "VertexCollection2/THREE" 
          ], 
          [ 
              "VertexCollection1/A", 
              "VertexCollection2/ONE" 
          ]]);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with clause not needed with edge collection if a named graph
    //  exists using bindVars
    ////////////////////////////////////////////////////////////////////////////////
    testWithClauseNotNeededBindVar: function() {
      const query = `FOR start IN @@vertex1
                       FOR v, e1, p IN 1..1 ANY start._id @@edge
                         RETURN SORTED(p.vertices[*]._id)`;

      const bindVars = {
        "@vertex1": vertexCollectionName1,
        "@edge": edgeCollectionName
      };

      var actual = db._query(query, bindVars).toArray();
      assertEqual(actual, [
          [ 
              "VertexCollection1/A", 
              "VertexCollection3/ALPHA" 
          ], 
          [ 
              "VertexCollection1/A", 
              "VertexCollection2/THREE" 
          ], 
          [ 
              "VertexCollection1/A", 
              "VertexCollection2/ONE" 
          ], 
          [ 
              "VertexCollection1/C", 
              "VertexCollection2/ONE" 
          ], 
          [ 
              "VertexCollection1/C", 
              "VertexCollection2/TWO" 
          ]]);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with clause not needed with edge collection if a named graph
    //  exists using bindVars and multiple collections
    ////////////////////////////////////////////////////////////////////////////////
    testWithClauseNotNeededTwoCollections: function() {
      const query = `FOR start IN @@vertex1
                       FOR v, e1, p IN 2..2 ANY start._id @@edge, @@edge2
                         RETURN p.vertices[*]._id`;

      const bindVars = {
        "@vertex1": vertexCollectionName1,
        "@edge": edgeCollectionName,
        "@edge2": edgeCollectionName2
      };

      const actual = db._query(query, bindVars).toArray();
      assertEqual(actual.sort(), [
        [ "VertexCollection1/A", "VertexCollection2/ONE", "VertexCollection1/C" ], 
        [ "VertexCollection1/A", "VertexCollection2/ONE", "VertexCollection3/ALPHA" ], 
        [ "VertexCollection1/A", "VertexCollection2/ONE", "VertexCollection3/BETA" ], 
        [ "VertexCollection1/A", "VertexCollection2/THREE", "VertexCollection3/ALPHA" ], 
        [ "VertexCollection1/A", "VertexCollection3/ALPHA", "VertexCollection2/ONE" ], 
        [ "VertexCollection1/A", "VertexCollection3/ALPHA", "VertexCollection2/THREE" ], 
        [ "VertexCollection1/A", "VertexCollection3/ALPHA", "VertexCollection3/ALPHA" ], 
        [ "VertexCollection1/A", "VertexCollection3/ALPHA", "VertexCollection3/ALPHA" ], 
        [ "VertexCollection1/C", "VertexCollection2/ONE", "VertexCollection1/A" ], 
        [ "VertexCollection1/C", "VertexCollection2/ONE", "VertexCollection3/ALPHA" ], 
        [ "VertexCollection1/C", "VertexCollection2/ONE", "VertexCollection3/BETA" ], 
        [ "VertexCollection1/C", "VertexCollection2/TWO", "VertexCollection3/GAMMA" ]].sort());
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with clause not needed with edge collection if a named graph
    //  exists using bindVars and multiple collections and direction
    ////////////////////////////////////////////////////////////////////////////////
    testWithClauseNotNeededTwoCollectionsDirection: function() {
      const query = `FOR start IN @@vertex1
                       FOR v, e1, p IN 1..2 ANY start._id INBOUND @@edge, @@edge2
                         RETURN p.vertices[*]._id`;

      const bindVars = {
        "@vertex1": vertexCollectionName1,
        "@edge": edgeCollectionName,
        "@edge2": edgeCollectionName2
      };

      const actual = db._query(query, bindVars).toArray();
      assertEqual(actual, []);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with clause *is* needed with edge collection if no named graph
    //  has definiton of vertex collections.
    ////////////////////////////////////////////////////////////////////////////////
    testWithClauseNeededIfNoNamedGraph: function() {
      const startNode = vertexCollectionName1 + "/A";
      const query = `FOR v, e1, p IN 1..2 ANY "${startNode}" @@edge
                         RETURN p.vertices[*]._id`;

      const bindVars = {
        "@edge": edgeCollectionName3
      };


      try {
        const actual = db._query(query, bindVars).toArray();
        
        if(require("internal").isCluster()) {
          assertTrue(false);
        }
      } catch (err) {
        assertEqual(internal.errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum, JSON.stringify(err));
      }
    },

    testWithClauseNotNeededShortestPath: function() {
      const startNode = vertexCollectionName1 + "/A";
      const endNode = vertexCollectionName2 + "/ONE";
      const query = `FOR p IN OUTBOUND SHORTEST_PATH "${startNode}" TO "${endNode}" ${edgeCollectionName} 
                       RETURN p._id`;

      const actual = db._query(query).toArray();
      assertEqual(actual, [ "VertexCollection1/A", "VertexCollection2/ONE" ]);
    },

    disabled_testWithClauseNotNeededShortestPathOptions: function() {
      const startNode = vertexCollectionName1 + "/A";
      const endNode = vertexCollectionName2 + "/ONE";
      const query = `FOR p IN OUTBOUND SHORTEST_PATH "${startNode}" TO "${endNode}" ${edgeCollectionName} 
                       OPTIONS { vertexCollections: [ "${vertexCollectionName1}" ] }
                       RETURN p._id`;

      const actual = db._query(query).toArray();
      assertEqual(actual, [ "VertexCollection1/A", "VertexCollection2/ONE" ]);
    },

    testWithClauseNotNeededKShortestPaths: function() {
      const startNode = vertexCollectionName1 + "/A";
      const endNode = vertexCollectionName2 + "/ONE";
      const query = `FOR p IN OUTBOUND K_SHORTEST_PATHS "${startNode}" TO "${endNode}" ${edgeCollectionName} 
                       RETURN p.vertices[*]._id`;

      const actual = db._query(query).toArray();
      assertEqual(actual, [ [ "VertexCollection1/A", "VertexCollection2/ONE" ] ]);
    },

    disabled_testWithClauseNotNeededKShortestPathsOptions: function() {
      const startNode = vertexCollectionName1 + "/A";
      const endNode = vertexCollectionName2 + "/ONE";
      const query = `FOR p IN OUTBOUND K_SHORTEST_PATHS "${startNode}" TO "${endNode}" ${edgeCollectionName} 
                       OPTIONS { vertexCollections: [ "${vertexCollectionName1}" ] }
                       RETURN p.vertices[*]._id`;

      const actual = db._query(query).toArray();
      assertEqual(actual, [ [ "VertexCollection1/A", "VertexCollection2/ONE" ] ]);
    },

    testWithClauseNotNeededKPaths: function() {
      const startNode = vertexCollectionName1 + "/A";
      const endNode = vertexCollectionName2 + "/ONE";
      const query = `FOR p IN 1..1 OUTBOUND K_PATHS "${startNode}" TO "${endNode}" ${edgeCollectionName} 
                       RETURN p.vertices[*]._id`;

      const actual = db._query(query).toArray();
      assertEqual(actual, [ [ "VertexCollection1/A", "VertexCollection2/ONE" ] ]);
    },

    disabled_testWithClauseNotNeededKPathsOptions: function() {
      const startNode = vertexCollectionName1 + "/A";
      const endNode = vertexCollectionName2 + "/ONE";
      const query = `FOR p IN 1..1 OUTBOUND K_PATHS "${startNode}" TO "${endNode}" ${edgeCollectionName} 
                       OPTIONS { vertexCollections: [ "${vertexCollectionName1}" ] }
                       RETURN p.vertices[*]._id`;

      const actual = db._query(query).toArray();
      assertEqual(actual, [ [ "VertexCollection1/A", "VertexCollection2/ONE" ] ]);
    },

    testWithClauseNotNeededAllShortestPaths: function() {
      const startNode = vertexCollectionName1 + "/A";
      const endNode = vertexCollectionName2 + "/ONE";
      const query = `FOR p IN OUTBOUND ALL_SHORTEST_PATHS "${startNode}" TO "${endNode}" ${edgeCollectionName} 
                       RETURN p.vertices[*]._id`;

      const actual = db._query(query).toArray();
      assertEqual(actual, [ [ "VertexCollection1/A", "VertexCollection2/ONE" ] ]); 
    },

    disabled_testWithClauseNotNeededAllShortestPathsOptions: function() {
      const startNode = vertexCollectionName1 + "/A";
      const endNode = vertexCollectionName2 + "/ONE";
      const query = `FOR p IN OUTBOUND ALL_SHORTEST_PATHS "${startNode}" TO "${endNode}" ${edgeCollectionName} 
                       OPTIONS { vertexCollections: [ "${vertexCollectionName1}" ] }
                       RETURN p.vertices[*]._id`;

      const actual = db._query(query).toArray();
      assertEqual(actual, [ [ "VertexCollection1/A", "VertexCollection2/ONE" ] ]); 
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(withClauseTestSuite);

return jsunity.done();
