/*jshint globalstrict:false, strict:false, unused: false */
/*global assertEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the general-graph class
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
/// @author Florian Bartels, Michael Hackstein
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var graph = require("@arangodb/general-graph");
var cluster;
var isOnServer = (typeof ArangoClusterComm === "object");
if (isOnServer) {
  cluster = require("@arangodb/cluster");
} else {
  cluster = {};
}
var ERRORS = arangodb.errors;

var _ = require("lodash");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: general-graph Creation and edge definition
////////////////////////////////////////////////////////////////////////////////

function GeneralGraphCreationSuite() {
  'use strict';
  var rn = "UnitTestRelationName";
  var rn1 = "UnitTestRelationName1";
  var vn1 = "UnitTestVertices1";
  var vn2 = "UnitTestVertices2";
  var vn3 = "UnitTestVertices3";
  var vn4 = "UnitTestVertices4";
  var gn = "UnitTestGraph";
  var edgeDef = graph._edgeDefinitions(
    graph._relation(rn, [vn1], [vn1]),
    graph._relation(rn1,
      [vn1, vn2], [vn3, vn4]
    )
  );

  var gN1 = "UnitTestEdgeDefDeleteGraph1",
    gN2 = "UnitTestEdgeDefDeleteGraph2",
    ec1 = "UnitTestEdgeDefDeleteEdgeCol1",
    ec2 = "UnitTestEdgeDefDeleteEdgeCol2",
    ec3 = "UnitTestEdgeDefDeleteEdgeCol3",
    vc1 = "UnitTestEdgeDefDeleteVertexCol1",
    vc2 = "UnitTestEdgeDefDeleteVertexCol2",
    vc3 = "UnitTestEdgeDefDeleteVertexCol3",
    vc4 = "UnitTestEdgeDefDeleteVertexCol4",
    vc5 = "UnitTestEdgeDefDeleteVertexCol5",
    vc6 = "UnitTestEdgeDefDeleteVertexCol6";

  return {

    setUp: function() {
      try {
        graph._drop(gN1, true);
      } catch(ignore) {
      }
      try {
        graph._drop(gN2, true);
      } catch(ignore) {
      }

    },

    tearDown: function() {
      db._drop("UnitTestsGraphRenamed1");
      db._drop("UnitTestsGraphRenamed2");
      db._drop("UnitTestsGraphRenamed3");
      db._drop(ec1);
      db._drop(ec2);
      db._drop(ec3);
      db._drop(vc1);
      db._drop(vc2);
      db._drop(vc3);
      db._drop(vc4);
      db._drop(vc5);
      db._drop(vc6);
      try {
        graph._drop(gN1, true);
      } catch(ignore) {
      }
      try {
        graph._drop(gN2, true);
      } catch(ignore) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rename
////////////////////////////////////////////////////////////////////////////////
    
    test_collectionRenameEdge: function() {
      if ((cluster && cluster.isCluster && cluster.isCluster()) || (! cluster || ! cluster.isCluster)) {
        return;
      }

      var g = graph._create(
        gN1,
        graph._edgeDefinitions(
          graph._relation(rn1, [vn2, vn1], [vn4, vn3])
        )
      );

      var doc = db._graphs.document(gN1);
      assertEqual(1, doc.edgeDefinitions.length);
      assertEqual(rn1, doc.edgeDefinitions[0].collection);
      assertEqual(2, doc.edgeDefinitions[0].from.length);
      assertEqual([ vn1, vn2 ], doc.edgeDefinitions[0].from.sort());
      assertEqual(2, doc.edgeDefinitions[0].to.length);
      assertEqual([ vn3, vn4 ], doc.edgeDefinitions[0].to.sort());
      assertEqual([ ], doc.orphanCollections);

      db._collection(rn1).rename("UnitTestsGraphRenamed1");
      
      doc = db._graphs.document(gN1);
      assertEqual(1, doc.edgeDefinitions.length);
      assertEqual("UnitTestsGraphRenamed1", doc.edgeDefinitions[0].collection);
      assertEqual(2, doc.edgeDefinitions[0].from.length);
      assertEqual([ vn1, vn2 ], doc.edgeDefinitions[0].from.sort());
      assertEqual(2, doc.edgeDefinitions[0].to.length);
      assertEqual([ vn3, vn4 ], doc.edgeDefinitions[0].to.sort());
      assertEqual([ ], doc.orphanCollections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rename
////////////////////////////////////////////////////////////////////////////////
    
    test_collectionRenameVertex: function() {
      if ((cluster && cluster.isCluster && cluster.isCluster()) || (! cluster || ! cluster.isCluster)) {
        return;
      }

      var g = graph._create(
        gN1,
        graph._edgeDefinitions(
          graph._relation(rn1, [vn2, vn1], [vn4, vn3])
        )
      );

      db._collection(vn1).rename("UnitTestsGraphRenamed1");
      
      var doc = db._graphs.document(gN1);
      assertEqual(1, doc.edgeDefinitions.length);
      assertEqual(rn1, doc.edgeDefinitions[0].collection);
      assertEqual(2, doc.edgeDefinitions[0].from.length);
      assertEqual([ "UnitTestsGraphRenamed1", vn2 ].sort(), doc.edgeDefinitions[0].from.sort());
      assertEqual(2, doc.edgeDefinitions[0].to.length);
      assertEqual([ vn3, vn4 ], doc.edgeDefinitions[0].to.sort());
      assertEqual([ ], doc.orphanCollections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rename
////////////////////////////////////////////////////////////////////////////////
    
    test_collectionRenameVertices: function() {
      if ((cluster && cluster.isCluster && cluster.isCluster()) || (! cluster || ! cluster.isCluster)) {
        return;
      }

      var g = graph._create(
        gN1,
        graph._edgeDefinitions(
          graph._relation(rn1, [vn2, vn1], [vn4, vn3])
        )
      );

      db._collection(rn1).rename("UnitTestsGraphRenamed1");
      db._collection(vn1).rename("UnitTestsGraphRenamed2");
      db._collection(vn4).rename("UnitTestsGraphRenamed3");
      
      var doc = db._graphs.document(gN1);
      assertEqual(1, doc.edgeDefinitions.length);
      assertEqual("UnitTestsGraphRenamed1", doc.edgeDefinitions[0].collection);
      assertEqual(2, doc.edgeDefinitions[0].from.length);
      assertEqual([ "UnitTestsGraphRenamed2", vn2 ].sort(), doc.edgeDefinitions[0].from.sort());
      assertEqual(2, doc.edgeDefinitions[0].to.length);
      assertEqual([ vn3, "UnitTestsGraphRenamed3" ].sort(), doc.edgeDefinitions[0].to.sort());
      assertEqual([ ], doc.orphanCollections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Graph Creation
////////////////////////////////////////////////////////////////////////////////

    test_undirectedRelation : function () {
      var r = graph._relation(rn, [vn1, vn2], [vn1, vn2]);

      assertEqual(r, {
        collection: rn,
        from: [vn1, vn2],
        to: [vn1, vn2]
      });

    },

    test_undirectedRelationWithSingleCollection : function () {
      var r = graph._relation(rn, vn1, vn1);

      assertEqual(r, {
        collection: rn,
        from: [vn1],
        to: [vn1]
      });

    },

    test_undirectedRelationWithMissingName : function () {
      try {
        graph._relation("", [vn1, vn2], [vn1, vn2]);
        fail();
      }
      catch (err) {
        assertEqual(err.errorMessage, "Invalid parameter type. arg1 must be non empty string");
      }
    },

    test_relationWithTooFewArgs : function () {
      try {
        graph._relation([vn1, vn2]);
        fail();
      }
      catch (err) {
        assertEqual(err.errorMessage, "Invalid number of arguments. Expected: 3");
      }
    },

    test_collectionSorting: function() {
      var g = graph._create(
        gn,
        graph._edgeDefinitions(
          graph._relation(rn1, [vn2, vn1], [vn4, vn3])
        )
      );

      assertEqual([vn1, vn2], g.__edgeDefinitions[0].from);
      assertEqual([vn3, vn4], g.__edgeDefinitions[0].to);
    },

    test_directedRelation : function () {
      var r = graph._relation(rn,
          [vn1, vn2], [vn3, vn4]);

      assertEqual(r, {
        collection: rn,
        from: [vn1, vn2],
        to: [vn3, vn4]
      });

    },

    test_directedRelationWithMissingName : function () {
      try {
        graph._relation("",
          [vn1, vn2], [vn3, vn4]);
        fail();
      }
      catch (err) {
        assertEqual(err.errorMessage, "Invalid parameter type. arg1 must be non empty string");
      }
    },

    test_directedRelationWithTooFewArgs : function () {
      try {
        graph._relation([vn1, vn2], [vn3, vn4]);
        fail();
      }
      catch (err) {
        assertEqual(err.errorMessage, "Invalid number of arguments. Expected: 3");
      }
    },

    test_relationWithInvalidSecondArg : function () {
      try {
        var param = {};
        param[vn1] = vn2;
        graph._relation(rn, param, vn3);
        fail();
      }
      catch (err) {
        assertEqual(err.errorMessage, "Invalid parameter type. arg2 must be non empty string or array");
      }


    },

    test_relationWithInvalidThirdArg : function () {
      try {
        var param = {};
        param[vn1] = vn2;
        graph._relation(rn, vn3, param);
        fail();
      }
      catch (err) {
        assertEqual(err.errorMessage, "Invalid parameter type. arg3 must be non empty string or array");
      }
    },

    testEdgeDefinitions : function () {

      //with empty args
      assertEqual(graph._edgeDefinitions(), []);

      //with args
      assertEqual(graph._edgeDefinitions(
        graph._relation(rn, vn1, vn1),
        graph._relation(rn1,
          [vn1, vn2], [vn3, vn4])
      ), [
        {
          collection: rn,
          from: [vn1],
          to: [vn1]
        },
        {
          collection: rn1,
          from: [vn1, vn2],
          to: [vn3, vn4]
        }
      ]);
    },

    testExtendEdgeDefinitions : function () {


      //with empty args
      assertEqual(graph._edgeDefinitions(), []);

      //with args
      var ed =graph._edgeDefinitions(
        graph._relation("relationName", "vertexC1", "vertexC1"),
        graph._relation("relationName",
          ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"])
      );
      graph._extendEdgeDefinitions(ed,
        graph._relation("relationName", "vertexC1", "vertexC1")
      );
      assertEqual(ed,  [
        {
          collection: "relationName",
          from: ["vertexC1"],
          to: ["vertexC1"]
        },
        {
          collection: "relationName",
          from: ["vertexC1", "vertexC2"],
          to: ["vertexC3", "vertexC4"]
        },
        {
          collection: "relationName",
          from: ["vertexC1"],
          to: ["vertexC1"]
        }
      ]);

    },


    test_create : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      var a = graph._create(
        gn,
        graph._edgeDefinitions(
          graph._relation(rn, vn1, vn1),
          graph._relation(rn1, [vn1, vn2], [vn3, vn4])
        )
      );
      assertTrue(a.__vertexCollections.hasOwnProperty(vn1));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn2));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn3));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn4));
      assertTrue(a.__edgeCollections.hasOwnProperty(rn));
      assertTrue(a.__edgeCollections.hasOwnProperty(rn1));
      assertEqual(a.__edgeDefinitions, [
        {
          "collection" : rn,
          "from" : [
            vn1
          ],
          "to" : [
            vn1
          ]
        },
        {
          "collection" : rn1,
          "from" : [
            vn1,
            vn2
          ],
          "to" : [
            vn3,
            vn4
          ]
        }
      ]
      );
    },

    test_create_WithOut_EdgeDefiniton : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      var g = graph._create(
        gn
      );
      assertEqual(g.__edgeDefinitions, []);
    },

    test_create_WithOut_Name : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      try {
        graph._create(
          "",
          graph._edgeDefinitions(
            graph._relation("relationName", "vertexC1", "vertexC1"),
            graph._relation("relationName2",
              ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"]
            )
          )
        );
        fail();
      } catch (err) {
        assertEqual(err.errorMessage, ERRORS.ERROR_GRAPH_CREATE_MISSING_NAME.message);
      }
    },

    test_create_With_Already_Existing_Graph : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      graph._create(gn, edgeDef);
      try {
        graph._create(gn, edgeDef);
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_GRAPH_DUPLICATE.code);
        assertEqual(err.errorMessage, ERRORS.ERROR_GRAPH_DUPLICATE.message);
      }
    },

    test_get_graph : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      graph._create(gn, edgeDef);
      var a = graph._graph(gn);

      assertTrue(a.__vertexCollections.hasOwnProperty(vn1));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn2));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn3));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn4));
      assertTrue(a.__edgeCollections.hasOwnProperty(rn));
      assertTrue(a.__edgeCollections.hasOwnProperty(rn1));
      assertEqual(a.__edgeDefinitions, [
        {
          "collection" : rn,
          "from" : [
            vn1
          ],
          "to" : [
            vn1
          ]
        },
        {
          "collection" : rn1,
          "from" : [
            vn1,
            vn2
          ],
          "to" : [
            vn3,
            vn4
          ]
        }
      ]
      );
    },

    test_get_graph_without_hit : function () {
      try {
        graph._graph(gn + "UnknownExtension");
        fail();
      } catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_GRAPH_NOT_FOUND.code);
        assertEqual(e.errorMessage, ERRORS.ERROR_GRAPH_NOT_FOUND.message);
      }
    },

    test_creationOfGraphShouldNotAffectCollections: function() {
      if(graph._exists(gn)) {
        graph._drop(gn, true);
      }
      var edgeDef2 = [graph._relation(rn, vn1, vn2)];
      var g = graph._create(gn, edgeDef2);
      var v1 = g[vn1].save({_key: "1"})._id;
      var v2 = g[vn2].save({_key: "2"})._id;
      var v3 = g[vn1].save({_key: "3"})._id;

      g[rn].save(v1, v2, {});
      assertEqual(g[vn1].count(), 2);
      assertEqual(g[vn2].count(), 1);
      assertEqual(g[rn].count(), 1);
      try {
        g[rn].save(v2, v3, {});
        fail();
      } catch (e) {
        // This should create an error
        assertEqual(g[rn].count(), 1);
      }

      try {
        db[rn].save(v2, v3, {});
      } catch (e) {
        // This should not create an error
        fail();
      }
      assertEqual(g[rn].count(), 2);

      db[vn2].remove(v2);
      // This should not remove edges
      assertEqual(g[rn].count(), 2);

      g[vn1].remove(v1);
      // This should remove edges
      assertEqual(g[rn].count(), 1);
      graph._drop(gn, true);
    },


    test_deleteEdgeDefinitionFromExistingGraph1: function() {
      var dr1 = graph._relation(ec1, [vc1], [vc1, vc2]),
        g1 = graph._create(gN1, [dr1]);

      try {
        g1._deleteEdgeDefinition(ec1);
      } catch (e) {
        assertEqual(
          e.errorMessage,
          arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message
        );
      }

    },

    test_deleteEdgeDefinitionFromExistingGraph2: function() {

      var dr1 = graph._relation(ec1, [vc1], [vc1, vc2]),
        dr2 = graph._relation(ec2, [vc3], [vc4, vc5]),
        dr3 = graph._relation(ec3, [vc4], [vc5]),
        g1 = graph._create(gN1, [dr1, dr2, dr3]);

      assertEqual([dr1, dr2, dr3], g1.__edgeDefinitions);
      g1._deleteEdgeDefinition(ec1);
      assertEqual([dr2, dr3], g1.__edgeDefinitions);
      assertEqual([vc1, vc2], g1._orphanCollections());
      assertTrue(db._collection(ec1) !== null);

      g1._deleteEdgeDefinition(ec2);
      assertEqual([dr3], g1.__edgeDefinitions);
      assertEqual([vc1, vc2, vc3], g1._orphanCollections());
      assertTrue(db._collection(ec2) !== null);
    },

    test_deleteEdgeDefinitionFromExistingGraphAndDropIt: function() {

      var dr1 = graph._relation(ec1, [vc1], [vc1, vc2]),
        dr2 = graph._relation(ec2, [vc3], [vc4, vc5]),
        dr3 = graph._relation(ec3, [vc4], [vc5]),
        g1 = graph._create(gN1, [dr1, dr2, dr3]);

      assertEqual([dr1, dr2, dr3], g1.__edgeDefinitions);
      g1._deleteEdgeDefinition(ec1, true);
      assertEqual([dr2, dr3], g1.__edgeDefinitions);
      assertEqual([vc1, vc2], g1._orphanCollections());
      assertTrue(db._collection(ec1) === null);

      g1._deleteEdgeDefinition(ec2, true);
      assertEqual([dr3], g1.__edgeDefinitions);
      assertEqual([vc1, vc2, vc3], g1._orphanCollections());
      assertTrue(db._collection(ec2) === null);
    },

    test_extendEdgeDefinitionFromExistingGraph1: function() {

      try {
        graph._drop(gN1, true);
      } catch(ignore) {
      }

      var dr1 = graph._relation(ec1, [vc1], [vc2]),
        dr2 = graph._relation(ec1, [vc2], [vc3]),
        g1 = graph._create(gN1, [dr1]);

      try {
        g1._extendEdgeDefinitions(dr2);
      } catch (e) {
        assertEqual(
          e.errorMessage,
          arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message
        );
      }

      try {
        graph._drop(gN1, true);
      } catch(ignore) {
      }

    },

    test_extendEdgeDefinitionFromExistingGraph2: function() {

      var dr1 = graph._relation(ec1, [vc1], [vc1, vc2]),
        dr2 = graph._relation(ec2, [vc3], [vc4, vc5]),
        dr2a = graph._relation(ec2, [vc3], [vc4]),
        g1 = graph._create(gN1, [dr1]),
        g2 = graph._create(gN2, [dr2]);

      try {
        g1._extendEdgeDefinitions(dr2a);
      } catch (e) {
        assertEqual(
          e.errorMessage,
          ec2 + " " + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message
        );
      }

      try {
        graph._drop(gN1, true);
      } catch(ignore) {
      }
      try {
        graph._drop(gN2, true);
      } catch(ignore) {
      }

    },

    test_extendEdgeDefinitionFromExistingGraph3: function() {
      try {
        graph._drop(gN1, true);
      } catch(ignore) {
      }
      try {
        graph._drop(gN2, true);
      } catch(ignore) {
      }

      var dr1 = graph._relation(ec1, [vc1], [vc1, vc2]),
        dr2 = graph._relation(ec2, [vc3], [vc4, vc5]),
        dr3 = graph._relation(ec3, [vc3], [vc4]),
        g1 = graph._create(gN1, [dr1]),
        g2 = graph._create(gN2, [dr2]);

      assertEqual([dr1], g1.__edgeDefinitions);
      g1._addVertexCollection(vc3);
      assertEqual([vc3], g1._orphanCollections());
      g1._extendEdgeDefinitions(dr3);
      assertEqual([dr1, dr3], g1.__edgeDefinitions);
      assertEqual([], g1._orphanCollections());
      g1._extendEdgeDefinitions(dr2);
      assertEqual([dr1, dr3, dr2], g1.__edgeDefinitions);

    },

    test_extendEdgeDefinitionFromExistingGraph4: function() {
      try {
        graph._drop(gN1, true);
      } catch(ignore) {
      }
      try {
        graph._drop(gN2, true);
      } catch(ignore) {
      }

      var dr1 = graph._relation(ec1, [vc1], [vc1, vc2]),
        dr2 = graph._relation(ec2, [vc4, vc3, vc1, vc2], [vc4, vc3, vc1, vc2]),
        g1 = graph._create(gN1, [dr1]);

      g1._extendEdgeDefinitions(dr2);
      assertEqual([dr1, dr2], g1.__edgeDefinitions);
      var edgeDefinition = _.findWhere(g1.__edgeDefinitions, {collection: ec2});
      assertEqual(edgeDefinition.from, [vc1, vc2, vc3, vc4]);
      assertEqual(edgeDefinition.to, [vc1, vc2, vc3, vc4]);


    },

    test_editEdgeDefinitionFromExistingGraph1: function() {
      var dr1 = graph._relation(ec1, [vc1], [vc1, vc2]),
        dr2 = graph._relation(ec2, [vc3], [vc4, vc5]),
        g1 = graph._create(gN1, [dr1]);

      try {
        g1._editEdgeDefinitions(dr2);
      } catch (e) {
        assertEqual(
          e.errorMessage,
          arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message
        );
      }

    },

    test_editEdgeDefinitionFromExistingGraph2: function() {

      var dr1 = graph._relation(ec1, [vc1, vc2], [vc3, vc4]),
        dr2 = graph._relation(ec2, [vc1], [vc4]),
        dr3 = graph._relation(ec1, [vc5], [vc5]),
        g1 = graph._create(gN1, [dr1, dr2]),
        g2 = graph._create(gN2, [dr1]);

      g1._editEdgeDefinitions(dr3);
      assertEqual([dr3, dr2], g1.__edgeDefinitions);
      assertEqual([dr3], g2.__edgeDefinitions);
      g2 = graph._graph(gN2);
      assertEqual(g1._orphanCollections().sort(), [vc2, vc3].sort());
      assertEqual(g2._orphanCollections().sort(), [vc1, vc2, vc3, vc4].sort());

    },

    test_editEdgeDefinitionFromExistingGraph3: function() {

      var dr1 = graph._relation(ec1, [vc1], [vc1, vc2]),
        dr2 = graph._relation(ec1, [vc3], [vc4, vc5]),
        dr3 = graph._relation(ec2, [vc2], [vc2, vc3]),
        g1 = graph._create(gN1, [dr1, dr3]),
        g2 = graph._create(gN2, [dr1]);

      g1._addVertexCollection(vc4);
      g2._addVertexCollection(vc5);
      g2._addVertexCollection(vc6);
      g1._editEdgeDefinitions(dr2, true);

      assertEqual([dr2, dr3], g1.__edgeDefinitions);
      assertEqual([dr2], g2.__edgeDefinitions);
      g2 = graph._graph(gN2);
      assertEqual([vc1], g1._orphanCollections());
      assertEqual(g2._orphanCollections().sort(), [vc1, vc2, vc6].sort());

      try {
        graph._drop(gN1, true);
      } catch(ignore) {
      }
      try {
        graph._drop(gN2, true);
      } catch(ignore) {
      }

    }


  };
}

function GeneralGraphAQLQueriesSuite() {
  'use strict';
  // Definition of names
  var graphName = "UnitTestsGraph";
  var included = "UnitTestIncluded";
  var excluded = "UnitTestExcluded";
  var v1 = "UnitTestV1";
  var v2 = "UnitTestV2";
  var v3 = "UnitTestV3";

  var dropInclExcl = function() {
    if (graph._exists(graphName)) {
      graph._drop(graphName, true);
    }
  };

  var e1, e2, e3;

  var createInclExcl = function() {
    dropInclExcl();
    var inc = graph._relation(
      included, [v1], [v1, v2]
    );
    var exc = graph._relation(
      excluded, [v1], [v3]
    );
    var g = graph._create(graphName, [inc, exc]);
    g[v1].save({_key: "1"});
    g[v1].save({_key: "2"});
    g[v2].save({_key: "1"});
    g[v3].save({_key: "1"});
    e1 = g[included].save(
      v1 + "/1",
      v2 + "/1",
      {
        _key: "e1",
        val: true
      }
    )._id;
    e2 = g[included].save(
      v1 + "/2",
      v1 + "/1",
      {
        _key: "e2",
        val: false
      }
    )._id;
    e3 = g[excluded].save(
      v1 + "/1",
      v3 + "/1",
      {
        _key: "e3",
        val: false
      }
    )._id;
    return g;
  };

  var findIdInResult = function(result, id) {
    return _.some(result, function(i) {
      return i._id === id;
    });
  };

  // The testee graph object
  var g;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief setUp: query creation for edges and vertices
////////////////////////////////////////////////////////////////////////////////

    setUp: function() {
      g = createInclExcl();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: query creation for edges and vertices
////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {
      dropInclExcl();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict construct on edges
////////////////////////////////////////////////////////////////////////////////

    test_restrictOnEdges: function() {
      var query = g._edges().restrict(included);
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,{},@options_0)');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars
        .options_0
        .edgeCollectionRestriction, [included]);
      assertEqual(bindVars
        .options_0
        .direction, "outbound"
      );

      var result = query.toArray();
      assertEqual(result.length, 2);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertTrue(findIdInResult(result, e2), "Did not include e2");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: query creation for Vertices
////////////////////////////////////////////////////////////////////////////////

    test_vertices: function() {
      var query = g._vertices(v1 + "/1");
      assertEqual(query.printQuery(), 'FOR vertices_0 IN GRAPH_VERTICES('
        + '@graphName,@vertexExample_0,@options_0)');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.vertexExample_0, {_id: v1 + "/1"});
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result,  v1 + "/1"), "Did not include " +  v1 + "/1");

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict error handling
////////////////////////////////////////////////////////////////////////////////

    test_restrictErrorHandlingSingle: function() {
      try {
        g._edges(v1 + "/1").restrict([included, "unknown"]);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
        assertEqual(err.errorMessage, "edge collections: unknown are not known to the graph");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict error handling on multiple failures
////////////////////////////////////////////////////////////////////////////////

    test_restrictErrorHandlingMultiple: function() {
      try {
        g._edges(v1 + "/1").restrict(["failed", included, "unknown", "foxxle"]);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
        assertEqual(err.errorMessage,
          "edge collections: failed and unknown and foxxle are not known to the graph");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: filter construct on Edges
////////////////////////////////////////////////////////////////////////////////

   test_filterOnEdges: function() {
      var query = g._edges().filter({val: true});
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,{},@options_0) '
        + 'FILTER MATCHES(edges_0,[{"val":true}])');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{}],
        includeData: true
      });
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
   },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: counting of query results
////////////////////////////////////////////////////////////////////////////////
//
    test_queryCount: function() {
      var query = g._edges();
      assertEqual(query.count(), 3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Cursor iteration
////////////////////////////////////////////////////////////////////////////////
    test_cursorIteration: function() {
      var query = g._edges();
      var list = [e1, e2, e3];
      var next;
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 2);
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 1);
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 0);
      assertFalse(query.hasNext());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Cursor recreation after iteration
////////////////////////////////////////////////////////////////////////////////
    test_cursorIterationAndRecreation: function() {
      var query = g._edges();
      var list = [e1, e2, e3];
      var next;
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 2);
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 1);
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 0);
      assertFalse(query.hasNext());
      query = query.filter({val: true});
      list = [e1];
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 0);
      assertFalse(query.hasNext());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Is cursor recreated after counting of query results and appending filter
////////////////////////////////////////////////////////////////////////////////
    test_cursorRecreationAfterCount: function() {
      var query = g._edges();
      assertEqual(query.count(), 3);
      query = query.filter({val: true});
      assertEqual(query.count(), 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Is cursor recreated after to array of query results and appending filter
////////////////////////////////////////////////////////////////////////////////
    test_cursorRecreationAfterToArray: function() {
      var query = g._edges();
      var result = query.toArray();
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertTrue(findIdInResult(result, e2), "Did not include e2");
      assertTrue(findIdInResult(result, e3), "Did not include e3");
      query = query.filter({val: true});
      result = query.toArray();
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    }

  };

}

function ChainedFluentAQLResultsSuite() {
  'use strict';

  var gn = "UnitTestGraph";
  var user = "UnitTestUsers";
  var product = "UnitTestProducts";
  var isFriend = "UnitTestIsFriend";
  var hasBought = "UnitTestHasBought";
  var uaName = "Alice";
  var ubName = "Bob";
  var ucName = "Charly";
  var udName = "Diana";
  var p1Name = "HiFi";
  var p2Name = "Shirt";
  var p3Name = "TV";
  var pTypeElec = "Electro";
  var pTypeCloth = "Cloth";

  var ud1 = 2000;
  var ud2 = 2001;
  var ud3 = 2002;
  var ud4 = 2003;

  var d1 = 2004;
  var d2 = 2005;
  var d3 = 2006;
  var d4 = 2007;
  var d5 = 2008;

  var g;

  var edgeDef = [];
  edgeDef.push(graph._relation(isFriend, user, user));
  edgeDef.push(graph._relation(hasBought, user, product));


  var findBoughts = function(result, list) {
    var boughts = _.sortBy(
      _.filter(result, function(e) {
        return e._id.split("/")[0] === hasBought;
      }),
      "date"
    );
    assertEqual(list.length, boughts.length, "Did not return all expected boughts");
    _.each(list.sort(), function(v, i) {
      assertEqual(boughts[i].date, v);
    });
  };

  var findFriends = function(result, list) {
    var friends = _.sortBy(
      _.filter(result, function(e) {
        return e._id.split("/")[0] === isFriend;
      }),
      "since"
    );
    assertEqual(list.length, friends.length, "Did not return all expected friendships");
    _.each(list.sort(), function(v, i) {
      assertEqual(friends[i].since, v);
    });
  };

  var dropData = function() {
    try {
      graph._drop(gn, true);
    } catch(ignore) {

    }
  };

  var createTestData = function() {
    dropData();
    g = graph._create(gn, edgeDef);
    var ua = g[user].save({name: uaName})._id;
    var ub = g[user].save({name: ubName})._id;
    var uc = g[user].save({name: ucName})._id;
    var ud = g[user].save({name: udName})._id;

    var p1 = g[product].save({name: p1Name, type: pTypeElec})._id;
    var p2 = g[product].save({name: p2Name, type: pTypeCloth})._id;
    var p3 = g[product].save({name: p3Name, type: pTypeElec})._id;

    g[isFriend].save(ua, ub, {
      since: ud1
    });
    g[isFriend].save(ua, uc, {
      since: ud2
    });
    g[isFriend].save(ub, ud, {
      since: ud3
    });
    g[isFriend].save(uc, ud, {
      since: ud4
    });

    g[hasBought].save(ua, p1, {
      date: d1
    });
    g[hasBought].save(ub, p1, {
      date: d2
    });
    g[hasBought].save(ub, p3, {
      date: d3
    });

    g[hasBought].save(ud, p1, {
      date: d4
    });
    g[hasBought].save(ud, p2, {
      date: d5
    });

  };

  var plainVertexQueryStmt = function(depth, mergeWith) {
    var stmt = "FOR vertices_" + depth + " IN "
      + "GRAPH_VERTICES("
      + "@graphName,";
    if (mergeWith !== undefined) {
      if (Array.isArray(mergeWith)) {
        var i;
        stmt += "[";
        for (i = 0; i < mergeWith.length; ++i) {
          if (i > 0) {
            stmt += ",";
          }
          stmt += "MERGE(@vertexExample_" + depth 
            + "," + mergeWith[i] + ")";
        }
        stmt += "]";
      } else {
        stmt += "MERGE(@vertexExample_" + depth
          + "," + mergeWith + ")";
      }
    } else {
      stmt += "@vertexExample_" + depth;
    }
    stmt += ",@options_" + depth + ")";
    return stmt;
  };

  var plainNeighborQueryStmt = function(depth, vdepth) {
    return "FOR neighbors_" + depth + " IN "
      + "GRAPH_NEIGHBORS("
      + "@graphName,"
      + "vertices_" + vdepth + ","
      + "@options_" + depth + ")";
  };

  var vertexMergeStmt = function(direction, eDepth) {
    switch(direction) {
      case "both":
        return [
          vertexMergeStmt("from", eDepth),
          vertexMergeStmt("to", eDepth)
        ];
      case "from":
        return  "{'_id': edges_" + eDepth + "._from}";
      case "to":
        return  "{'_id': edges_" + eDepth + "._to}";
      default:
        fail("Helper function does not know direction:" + direction);
    }

  };

  var plainEdgesQueryStmt = function(depth, vDepth, type) {
    if (!type) {
      type = "vertices";
    }
    var q = "FOR edges_" + depth + " IN "
      + "GRAPH_EDGES("
      + "@graphName,";
    if(vDepth > -1) {
      q += type + "_" + vDepth;
      q += ",";
    } else {
      q += "{},";
    }
    q += "@options_" + depth + ")";
    return q;
  };

  return {

    setUp: createTestData,

    tearDown: dropData,

    test_getAllVerticesResultingAQL: function() {
      var query = g._vertices();
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, {});
      assertEqual(query.bindVars.options_0, {});
    },

    test_getAllVertices: function() {
      var result = g._vertices().toArray();
      assertEqual(result.length, 7);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, ubName);
      assertEqual(sorted[2].name, ucName);
      assertEqual(sorted[3].name, udName);
      assertEqual(sorted[4].name, p1Name);
      assertEqual(sorted[5].name, p2Name);
      assertEqual(sorted[6].name, p3Name);
    },

    test_getVertexByIdResultingAQL: function() {
      var a_id = g[user].firstExample({name: uaName})._id;
      var query = g._vertices(a_id);
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, {_id: a_id});
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVertexById: function() {
      var a_id = g[user].firstExample({name: uaName})._id;
      var result = g._vertices(a_id).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0].name, uaName);
    },

    test_getVerticesByIdResultingAQL: function() {
      var a_id = g[user].firstExample({name: uaName})._id;
      var b_id = g[user].firstExample({name: ubName})._id;
      var query = g._vertices([a_id, b_id]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, [{_id: a_id}, {_id: b_id}]);
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVerticesById: function() {
      var a_id = g[user].firstExample({name: uaName})._id;
      var b_id = g[user].firstExample({name: ubName})._id;
      var result = g._vertices([a_id, b_id]).toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, ubName);
    },

    test_getVertexByExampleResultingAQL: function() {
      var query = g._vertices({
        name: uaName
      });
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, {name: uaName});
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVertexByExample: function() {
      var result = g._vertices({
        name: uaName
      }).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0].name, uaName);
    },

    test_getVerticesByExampleResultingAQL: function() {
      var query = g._vertices([{
        name: uaName
      },{
        name: p1Name
      }]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, [
        {name: uaName},
        {name: p1Name}
      ]);
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVerticesByExample: function() {
      var result = g._vertices([{
        name: uaName
      },{
        name: p1Name
      }]).toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, p1Name);
    },

    test_getVerticesByExampleAndIdMixResultingAQL: function() {
      var b_id = g[user].firstExample({name: ubName})._id;
      var query = g._vertices([{
        name: uaName
      },
      b_id,
      {
        name: ucName
      }]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, [
        {name: uaName},
        {_id: b_id},
        {name: ucName}
      ]);
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVerticesByExampleAndIdMix: function() {
      var b_id = g[user].firstExample({name: ubName})._id;
      var result = g._vertices([{
        name: uaName
      },
      b_id,
      {
        name: ucName
      }]).toArray();
      assertEqual(result.length, 3);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, ubName);
      assertEqual(sorted[2].name, ucName);
    },

    test_getAllEdgesResultingAQL: function() {
      var query = g._edges();
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{}],
        includeData: true
      });
    },

    test_getAllEdges: function() {
      var result = g._edges().toArray();
      assertEqual(result.length, 9);
      findFriends(result, [ud1, ud2, ud3, ud4]);
      findBoughts(result, [d1, d2, d3, d4, d5]);
    },

    test_getEdgeByIdResultingAQL: function() {
      var a_id = g[hasBought].firstExample({date: d1})._id;
      var query = g._edges(a_id);
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{_id: a_id}],
        includeData: true
      });
    },

    test_getEdgeById: function() {
      var a_id = g[hasBought].firstExample({date: d1})._id;
      var result = g._edges(a_id).toArray();
      assertEqual(result.length, 1);
      findBoughts(result, [d1]);
    },

    test_getEdgesByIdResultingAQL: function() {
      var a_id = g[hasBought].firstExample({date: d1})._id;
      var b_id = g[isFriend].firstExample({since: ud2})._id;
      var query = g._edges([a_id, b_id]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [
          {_id: a_id},
          {_id: b_id}
        ],
        includeData: true
      });
    },

    test_getEdgesById: function() {
      var a_id = g[hasBought].firstExample({date: d1})._id;
      var b_id = g[isFriend].firstExample({since: ud2})._id;
      var result = g._edges([a_id, b_id]).toArray();
      assertEqual(result.length, 2);
      findBoughts(result, [d1]);
      findFriends(result, [ud2]);
    },

    test_getEdgeByExampleResultingAQL: function() {
      var query = g._edges({
        date: d2
      });
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [
          {date: d2}
        ],
        includeData: true
      });
    },

    test_getEdgeByExample: function() {
      var result = g._edges({
        date: d2
      }).toArray();
      assertEqual(result.length, 1);
      findBoughts(result, [d2]);
    },

    test_getEdgesByExampleResultingAQL: function() {
      var query = g._edges([{
        since: ud3
      },{
        date: d3
      }]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [
          {since: ud3},
          {date: d3}
        ],
        includeData: true
      });
    },

    test_getEdgesByExample: function() {
      var result = g._edges([{
        since: ud3
      },{
        date: d3
      }]).toArray();
      assertEqual(result.length, 2);
      findBoughts(result, [d3]);
      findFriends(result, [ud3]);
    },

    test_getEdgesByExampleAndIdMixResultingAQL: function() {
      var b_id = g[hasBought].firstExample({date: d1})._id;
      var query = g._edges([{
        date: d5
      },
      b_id,
      {
        since: ud1
      }]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [
          {date: d5},
          {_id: b_id},
          {since: ud1}
        ],
        includeData: true
      });
    },

    test_getEdgesByExampleAndIdMix: function() {
      var b_id = g[hasBought].firstExample({date: d1})._id;
      var result = g._edges([{
        date: d5
      },
      b_id,
      {
        since: ud1
      }]).toArray();
      assertEqual(result.length, 3);
      findBoughts(result, [d1, d5]);
      findFriends(result, [ud1]);
    },

    test_getEdgesForSelectedVertexResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .edges();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {name: uaName});
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "any",
        edgeExamples: [{}],
        includeData: true
      });
    },

    test_getEdgesForSelectedVertex: function() {
      var result = g._vertices({name: uaName})
        .edges()
        .toArray();
      assertEqual(result.length, 3);
      findBoughts(result, [d1]);
      findFriends(result, [ud1, ud2]);
    },

    test_getEdgesForSelectedVertexWithoutEdges: function() {
      var result = g._vertices({name: p1Name})
        .edges()
        .restrict(isFriend)
        .toArray();
      assertEqual(result.length, 0);
    },

    test_getEdgesWithRestrictionIfThereAreNone: function() {
      var emptyGN = "UnitTestEmptyGraph";
      var emptyEdges = "UnitTestEmptyEdges";
      var emptyVertices = "UnitTestEmptyVertices";
      try {
        graph._drop(emptyGN, true);
      } catch (ignore) {
      }
      var g2 = graph._create(emptyGN, [
        graph._relation(emptyEdges, emptyVertices, emptyVertices)
      ]);
      g2[emptyVertices].save({_key: "highlander"});
      var res = g2._vertices(emptyVertices + "/highlander").edges().restrict(emptyEdges).toArray();
      assertEqual(res, []);
      graph._drop(emptyGN, true);
    },

    test_getInEdgesForSelectedVertexResultingAQL: function() {
      var query = g._vertices({name: ubName})
        .inEdges();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {name: ubName});
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "inbound",
        edgeExamples: [{}],
        includeData: true
      });
    },

    test_getInEdgesForSelectedVertex: function() {
      var result = g._vertices({name: ubName})
        .inEdges()
        .toArray();
      assertEqual(result.length, 1);
      findFriends(result, [ud1]);
    },

    test_getOutEdgesForSelectedVertexResultingAQL: function() {
      var query = g._vertices({name: ubName})
        .outEdges();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {name: ubName});
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "outbound",
        edgeExamples: [{}],
        includeData: true
      });
    },

    test_getOutEdgesForSelectedVertex: function() {
      var result = g._vertices({name: ubName})
        .outEdges()
        .toArray();
      assertEqual(result.length, 3);
      findBoughts(result, [d2, d3]);
      findFriends(result, [ud3]);
    },

    test_getVerticesForSelectedEdgeResultingAQL: function() {
      var query = g._edges({since: ud1})
        .vertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainEdgesQueryStmt(0));
      expected.push(plainVertexQueryStmt(1, vertexMergeStmt("both", 0)));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{since: ud1}],
        includeData: true
      });
      assertEqual(query.bindVars.options_1, {});
    },

    test_getVerticesForSelectedEdge: function() {
      var result = g._edges({since: ud1})
        .vertices()
        .toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, ubName);
    },

    test_toVertexForSelectedEdgeResultingAQL: function() {
      var query = g._edges({since: ud1})
        .toVertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainEdgesQueryStmt(0));
      expected.push(plainVertexQueryStmt(1, vertexMergeStmt("to", 0)));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{since: ud1}],
        includeData: true
      });
      assertEqual(query.bindVars.options_1, {});
    },

    test_toVertexForSelectedEdge: function() {
      var result = g._edges({since: ud1})
        .toVertices()
        .toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0].name, ubName);
    },

    test_fromVertexForSelectedEdgeResultingAQL: function() {
      var query = g._edges({since: ud1})
        .fromVertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainEdgesQueryStmt(0));
      expected.push(plainVertexQueryStmt(1, vertexMergeStmt("from", 0)));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{since: ud1}],
        includeData: true
      });
      assertEqual(query.bindVars.options_1, {});
    },

    test_fromVertexForSelectedEdge: function() {
      var result = g._edges({since: ud1})
        .fromVertices()
        .toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0].name, uaName);
    },

    test_getAllVerticesThroughOutgoingEdgeResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .outEdges()
        .toVertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      expected.push(plainVertexQueryStmt(2, vertexMergeStmt("to", 1)));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {
        name: uaName
      });
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "outbound",
        edgeExamples: [{}],
        includeData: true
      });
      assertEqual(query.bindVars.options_2, {});
    },

    test_getAllVerticesThroughOutgoingEdges: function() {
      var result = g._vertices({name: uaName})
        .outEdges()
        .toVertices()
        .toArray();
      assertEqual(result.length, 3);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, ubName);
      assertEqual(sorted[1].name, ucName);
      assertEqual(sorted[2].name, p1Name);
    },

    test_getAllVerticesThroughOutgoingEdgesWithFilterResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .outEdges([
        {since: ud1},
        {date: d1}
      ])
        .toVertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      expected.push(plainVertexQueryStmt(2, vertexMergeStmt("to", 1)));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {
        name: uaName
      });
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "outbound",
        edgeExamples: [
          {since: ud1},
          {date: d1}
        ],
        includeData: true
      });
      assertEqual(query.bindVars.options_2, {});
    },

    test_getAllVerticesThroughOutgoingEdgesWithFilter: function() {
      var result = g._vertices({name: uaName})
      .outEdges([
        {since: ud1},
        {date: d1}
      ]).toVertices()
        .toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, ubName);
      assertEqual(sorted[1].name, p1Name);
    },

    test_getNeighborsOfSelectedVerticesResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .neighbors();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainNeighborQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        neighborExamples: {},
        includeData: true
      });
    },

    test_getNeighborsOfSelectedVertices: function() {
      var result = g._vertices({name: uaName})
        .neighbors()
        .toArray();
      assertEqual(result.length, 3);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, ubName);
      assertEqual(sorted[1].name, ucName);
      assertEqual(sorted[2].name, p1Name);
    },

    test_getExampleNeighborsOfSelectedVerticesResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .neighbors([{
          name: ubName
        },{
          name: p1Name
        }]);
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainNeighborQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        neighborExamples: [{
          name: ubName
        },{
          name: p1Name
        }],
        includeData: true
      });
    },

    test_getExampleNeighborsOfSelectedVertices: function() {
      var result = g._vertices({name: uaName})
        .neighbors([{
          name: ubName
        },{
          name: p1Name
        }])
        .toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, ubName);
      assertEqual(sorted[1].name, p1Name);
    },

    test_getEdgesOfNeighborsResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .neighbors()
        .outEdges();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainNeighborQueryStmt(1, 0));
      expected.push(plainEdgesQueryStmt(2, 1, "neighbors"));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        neighborExamples: {},
        includeData: true
      });
      assertEqual(query.bindVars.options_2, {
        direction: "outbound",
        edgeExamples: [{}],
        includeData: true
      });
    },

    test_getEdgesOfNeighbors: function() {
      var result = g._vertices({name: uaName})
        .neighbors()
        .outEdges()
        .toArray();
      assertEqual(result.length, 4);
      findFriends(result, [ud3, ud4]);
      findBoughts(result, [d2, d3]);
    },

    test_path: function() {
      var result = g._vertices({name: uaName})
        .edges()
        .toVertices()
        .path()
        .toArray();
      assertEqual(uaName, result[0][0].name);
      assertEqual(ud1, result[0][1].since);
      assertEqual(ubName, result[0][2].name);
      assertEqual(uaName, result[1][0].name);
      assertEqual(ud2, result[1][1].since);
      assertEqual(ucName, result[1][2].name);
      assertEqual(uaName, result[2][0].name);
      assertEqual(d1, result[2][1].date);
      assertEqual(p1Name, result[2][2].name);
    },

    test_pathVertices: function() {
      var result = g._vertices({name: uaName})
        .edges()
        .toVertices()
        .pathVertices()
        .toArray();
      assertEqual(uaName, result[0][0].name);
      assertEqual(ubName, result[0][1].name);
      assertEqual(uaName, result[1][0].name);
      assertEqual(ucName, result[1][1].name);
      assertEqual(uaName, result[2][0].name);
      assertEqual(p1Name, result[2][1].name);
    },

    test_pathEdges: function() {
      var result = g._vertices({name: uaName})
        .edges()
        .toVertices()
        .pathEdges()
        .toArray();
      assertEqual(ud1, result[0][0].since);
      assertEqual(ud2, result[1][0].since);
      assertEqual(d1, result[2][0].date);
    }


  };
}

function EdgesAndVerticesSuite() {
  'use strict';

  var g;
  var vertexId1, vertexId2;
  var unitTestGraphName = "unitTestGraph";

  var ec1 = "unitTestEdgeCollection1";
  var ec2 = "unitTestEdgeCollection2";
  var vc1 = "unitTestVertexCollection1";
  var vc2 = "unitTestVertexCollection2";
  var vc3 = "unitTestVertexCollection3";
  var vc4 = "unitTestVertexCollection4";

  var ids = {};

  var fillCollections = function() {
    var vertex = g[vc1].save({first_name: "Tam"});
    ids.vId11 = vertex._id;
    vertex = g[vc1].save({first_name: "Tem"});
    ids.vId12 = vertex._id;
    vertex = g[vc1].save({first_name: "Tim"});
    ids.vId13 = vertex._id;
    vertex = g[vc1].save({first_name: "Tom"});
    ids.vId14 = vertex._id;
    vertex = g[vc1].save({first_name: "Tum"});
    ids.vId15 = vertex._id;
    vertex = g[vc3].save({first_name: "Tam"});
    ids.vId31 = vertex._id;
    vertex = g[vc3].save({first_name: "Tem"});
    ids.vId32 = vertex._id;
    vertex = g[vc3].save({first_name: "Tim"});
    ids.vId33 = vertex._id;
    vertex = g[vc3].save({first_name: "Tom"});
    ids.vId34 = vertex._id;
    vertex = g[vc3].save({first_name: "Tum"});
    ids.vId35 = vertex._id;

    var edge = g[ec1].save(ids.vId11, ids.vId12, {});
    ids.eId11 = edge._id;
    edge = g[ec1].save(ids.vId11, ids.vId13, {});
    ids.eId12 = edge._id;
    edge = g[ec1].save(ids.vId11, ids.vId14, {});
    ids.eId13 = edge._id;
    edge = g[ec1].save(ids.vId11, ids.vId15, {});
    ids.eId14 = edge._id;
    edge = g[ec1].save(ids.vId12, ids.vId11, {});
    ids.eId15 = edge._id;
    edge = g[ec1].save(ids.vId13, ids.vId11, {});
    ids.eId16 = edge._id;
    edge = g[ec1].save(ids.vId14, ids.vId11, {});
    ids.eId17 = edge._id;
    edge = g[ec1].save(ids.vId15, ids.vId11, {});
    ids.eId18 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId31, {});
    ids.eId21 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId32, {});
    ids.eId22 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId33, {});
    ids.eId23 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId34, {});
    ids.eId24 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId35, {});
    ids.eId25 = edge._id;
    return ids;
  };

  var pickStartByName = function (name) {
    var exampleFilter = `FILTER x.first_name == "${name}" RETURN x`;
    return `FOR start IN UNION(
       (FOR x IN ${vc1} ${exampleFilter}),
       (FOR x IN ${vc2} ${exampleFilter}),
       (FOR x IN ${vc3} ${exampleFilter}),
       (FOR x IN ${vc4} ${exampleFilter}))`;

  };

  return {

    setUp : function() {
      try {
        graph._drop(unitTestGraphName, true);
      } catch (ignore) {
      }
      g = graph._create(
        unitTestGraphName,
        graph._edgeDefinitions(
          graph._relation(ec1, vc1, vc1),
          graph._relation(ec2,
            [vc1, vc2], [vc3, vc4]
          )
        )
      );
    },

    tearDown : function() {
      graph._drop(unitTestGraphName, true);
    },

    test_connectingEdges : function () {
      fillCollections();
      var query = `${pickStartByName("Tam")} FOR v, e IN ANY start GRAPH "${unitTestGraphName}" FILTER v.first_name == "Tem" RETURN e`;
      var res = db._query(query).toArray();
      assertEqual(res.length, 3);
    },

    test_connectingEdgesWithEdgeCollectionRestriction : function () {
      fillCollections();
      var query = `${pickStartByName("Tam")} FOR v, e IN ANY start GRAPH "${unitTestGraphName}" RETURN DISTINCT e`;
      var res = db._query(query).toArray();
      assertEqual(res.length, 13);

      query = `${pickStartByName("Tam")} FOR v, e IN ANY start ${ec2} RETURN DISTINCT e`;
      res = db._query(query).toArray();
      assertEqual(res.length, 5);
    },

    test_connectingEdgesWithVertexCollectionRestriction : function () {
      fillCollections();
      var query = `FOR start IN ${vc1} FOR v, e IN ANY start GRAPH "${unitTestGraphName}" RETURN DISTINCT e`;
      var res = db._query(query).toArray();
      assertEqual(res.length, 13);
      query = `FOR start IN ${vc1} FOR v, e IN ANY start GRAPH "${unitTestGraphName}" FILTER IS_SAME_COLLECTION("${vc3}", v) RETURN DISTINCT e`;
      res = db._query(query).toArray();
      assertEqual(res.length, 5);
    },

    test_connectingEdgesWithIds : function () {
      var ids = fillCollections();
      var query = `FOR v, e IN ANY "${ids.vId11}" GRAPH "${unitTestGraphName}" FILTER v._id == "${ids.vId13}" RETURN e`;
      var res = db._query(query).toArray();
      assertEqual(res.length, 2);
    },

    test_dropGraph1 : function () {
      var myGraphName = unitTestGraphName + "2";
      var myEdgeColName = "unitTestEdgeCollection4711";
      var myVertexColName = vc1;
      graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._relation(myEdgeColName, myVertexColName, myVertexColName)
        )
      );
      graph._drop(myGraphName, true);
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(myVertexColName) !== null);
      assertTrue(db._collection(myEdgeColName) === null);
    },

    test_dropGraph2 : function () {
      var myGraphName = unitTestGraphName + "2";
      var myEdgeColName = "unitTestEdgeCollection4711";
      var myVertexColName = vc1;
      graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._relation(myEdgeColName, myVertexColName, myVertexColName)
        )
      );
      graph._drop(myGraphName);
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(myVertexColName) !== null);
      assertTrue(db._collection(myEdgeColName) !== null);
    },

    test_createGraphWithCollectionDuplicateOK : function () {
      var myGraphName = unitTestGraphName + "2";
      graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._relation(ec1, vc1, vc1)
        )
      );
      assertTrue(graph._exists(myGraphName));
      graph._drop(myGraphName, true);
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(vc1) !== null);
      assertTrue(db._collection(ec1) !== null);
    },

    test_createGraphWithMalformedEdgeDefinitions : function () {
      var myGraphName = unitTestGraphName + "2";
      try {
        graph._create(
          myGraphName,
          [ "foo" ]
        );
      } catch (e) {
        assertEqual(
          e.errorMessage,
          arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.message
        );
      }
      assertFalse(graph._exists(myGraphName));
    },

    test_createGraphWithCollectionDuplicateNOK1 : function () {
      var myGraphName = unitTestGraphName + "2";
      try {
        graph._create(
          myGraphName,
          graph._edgeDefinitions(
            graph._relation(ec1, vc2, vc2)
          )
        );
      } catch (e) {
        assertEqual(
          e.errorMessage,
          ec1 + " " + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message
        );
      }
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(vc2) !== null);
      assertTrue(db._collection(ec1) !== null);
    },

    test_createGraphWithCollectionDuplicateNOK2 : function () {
      var myGraphName = unitTestGraphName + "2";
      var myED = "unitTestEdgeCollection4711";
      var myVD1 = "unitTestVertexCollection4711";
      var myVD2 = "unitTestVertexCollection4712";
      try {graph._drop(myGraphName, true);} catch (ignore){}
      try {db._drop(myED);} catch (ignore){}
      try {
        graph._create(
          myGraphName,
          graph._edgeDefinitions(
            graph._relation(myED, myVD1, myVD1),
            graph._relation(myED, myVD2, myVD2)
          )
        );
      } catch (e) {
        assertEqual(
          e.errorMessage,
          arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message
        );
      }
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(myVD1) === null);
      assertTrue(db._collection(myVD2) === null);
      assertTrue(db._collection(myED) === null);
    },

    test_edgeCollections : function () {

      var edgeCollections = g._edgeCollections();
      assertEqual(edgeCollections[0].name(), ec1);
      assertEqual(edgeCollections[1].name(), ec2);
    },

    test_vertexCollections : function () {

      var vertexCollections = g._vertexCollections();
      assertEqual(vertexCollections[0].name(), vc1);
      assertEqual(vertexCollections[1].name(), vc2);
      assertEqual(vertexCollections[2].name(), vc3);
      assertEqual(vertexCollections[3].name(), vc4);
    },

    test_vC_save : function () {
      var vertex = g[vc1].save({first_name: "Tom"});
      assertFalse(vertex.error);
      vertexId1 = vertex._id;
      var vertexObj = g[vc1].document(vertexId1);
      assertEqual(vertexObj.first_name, "Tom");
    },

    test_vC_replace : function () {
      var vertex = g[vc1].save({first_name: "Tom"});
      var vertexId = vertex._id;
      vertex = g[vc1].replace(vertexId, {first_name: "Tim"});
      assertFalse(vertex.error);
      var vertexObj = g[vc1].document(vertexId);
      assertEqual(vertexObj.first_name, "Tim");
    },

    test_vC_update : function () {
      var vertex = g[vc1].save({first_name: "Tim"});
      var vertexId = vertex._id;
      vertex = g[vc1].update(vertexId, {age: 42});
      assertFalse(vertex.error);
      var vertexObj = g[vc1].document(vertexId);
      assertEqual(vertexObj.first_name, "Tim");
      assertEqual(vertexObj.age, 42);
    },

    test_vC_remove : function () {
      var col = g[vc1];
      var counter = col.count();
      var vertex = col.save({first_name: "Tim"});
      assertEqual(col.count(), counter + 1);
      var vertexId = vertex._id;
      var result = col.remove(vertexId);
      assertEqual(col.count(), counter);
      assertTrue(result);
    },

    test_vC_remove_by_key : function () {
      var col = g[vc1];
      var counter = col.count();
      var vertex = col.save({first_name: "Tim"});
      assertEqual(col.count(), counter + 1);
      var vertexId = vertex._key;
      var result = col.remove(vertexId);
      assertEqual(col.count(), counter);
      assertTrue(result);
    },

    test_vC_removeWithEdge : function () {
      var vertex1 = g[vc1].save({first_name: "Tim"});
      var vId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tom"});
      var vId2 = vertex2._id;
      var edge = g[ec1].save(vId1, vId2, {});
      var edgeId = edge._id;
      var result = g[vc1].remove(vId1);
      assertTrue(result);
      assertFalse(db[ec1].exists(edgeId));
      result = g[vc1].remove(vId2);
      assertTrue(result);
    },

    test_eC_save_undirected : function() {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vId2 = vertex2._id;
      var edge = g[ec1].save(vertexId1, vId2, {});
      assertFalse(edge.error);
      g[vc1].remove(vId1);
      g[vc1].remove(vId2);
    },

    test_eC_save_directed : function() {
      var vertex1 = g[vc2].save({first_name: "Tom"});
      vertexId1 = vertex1._id;
      var vertex2 = g[vc4].save({first_name: "Tim"});
      vertexId2 = vertex2._id;
      var edge = g[ec2].save(vertexId1, vertexId2, {});
      assertFalse(edge.error);
      g[vc2].remove(vertexId1);
      g[vc4].remove(vertexId2);
    },

    test_eC_save_withError : function() {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      vertexId1 = vertex1._id;
      var vertex2 = g[vc2].save({first_name: "Tim"});
      vertexId2 = vertex2._id;
      try {
        g[ec1].save(vertexId1, vertexId2, {});
        fail();
      } catch (e) {
        assertEqual(e.errorNum, 1906);
      }
      g[vc1].remove(vertexId1);
      g[vc2].remove(vertexId2);
    },

    test_eC_replace : function() {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g[ec1].replace(edgeId1, {_from: vertexId1, _to: vertexId2, label: "knows"});
      assertFalse(edge.error);
      var edgeObj = g[ec1].document(edgeId1);
      assertEqual(edgeObj.label, "knows");
      assertEqual(edgeObj._id, edgeId1);
    },

    test_eC_update : function () {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g[ec1].replace(edgeId1, {_from: vertexId1, _to: vertexId2, label: "knows"});
      edge = g[ec1].update(edgeId1, {blub: "blub"});
      assertFalse(edge.error);
      var edgeObj = g[ec1].document(edgeId1);
      assertEqual(edgeObj.label, "knows");
      assertEqual(edgeObj.blub, "blub");
      assertEqual(edgeObj._id, edgeId1);
    },

    test_eC_remove : function () {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g[ec1].remove(edgeId1);
      assertTrue(edge);
    },

    test_eC_remove_by_key : function () {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var counter = g[ec1].count();
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      assertEqual(g[ec1].count(), counter + 1);
      var edgeId1 = edge._key;
      edge = g[ec1].remove(edgeId1);
      assertTrue(edge);
      assertEqual(g[ec1].count(), counter);
    },

    test_eC_removeWithEdgesAsVertices : function () {

      var myGraphName = unitTestGraphName + "0815";
      var myEC02 = "unitTestEdgeCollection02";
      var myVC01 = "unitTestVertexCollection01";
      try {
        graph._drop(myGraphName, true);
        db._drop(myEC02);
        db._drop(myVC01);
      } catch (ignore) {
      }
      var g2 = graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._relation(myEC02,
            [ec1], [myVC01]
          )
        )
      );
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var vertex3 = g2.unitTestVertexCollection01.save({first_name: "Ralph"});
      var vertexId3 = vertex3._id;
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      var edge2 = g2.unitTestEdgeCollection02.save(edge._id, vertexId3, {});

      var edgeId1 = edge._id;
      edge = g[ec1].remove(edgeId1);
      assertTrue(edge);
      assertFalse(db._exists(edge2._id));
      graph._drop(myGraphName, true);
      assertFalse(graph._exists(myGraphName));
    },

    test_eC_removeWithEdgesAsVerticesCircle : function () {
      var gN1 = "unitTestGraphCircle1";
      var gN2 = "unitTestGraphCircle2";
      var gN3 = "unitTestGraphCircle3";
      var gN4 = "unitTestGraphCircle4";
      var eC1 = "unitTestEdgeCollectionCircle1";
      var eC2 = "unitTestEdgeCollectionCircle2";
      var eC3 = "unitTestEdgeCollectionCircle3";
      var eC4 = "unitTestEdgeCollectionCircle4";
      var vC1 = "unitTestVertexCollectionCircle1";
      var vC2 = "unitTestVertexCollectionCircle2";
      var vC3 = "unitTestVertexCollectionCircle3";
      var vC4 = "unitTestVertexCollectionCircle4";
      try {
        graph._drop(gN1, true);
        graph._drop(gN2, true);
        graph._drop(gN3, true);
        graph._drop(gN4, true);
        db._drop(eC1);
        db._drop(eC2);
        db._drop(eC3);
        db._drop(eC4);
        db._drop(vC1);
        db._drop(vC2);
        db._drop(vC3);
        db._drop(vC4);
      } catch (ignore) {
      }

      db._createEdgeCollection(eC1);
      db._createEdgeCollection(eC2);
      db._createEdgeCollection(eC3);
      db._createEdgeCollection(eC4);
      db._create(vC1);
      db._create(vC2);
      db._create(vC3);
      db._create(vC4);
      var vertex1 = db[vC1].save({});
      vertexId1 = vertex1._id;
      var vertex2 = db[vC1].save({});
      vertexId2 = vertex2._id;
      var vertex3 = db[vC1].save({});
      var vertexId3 = vertex3._id;
      var vertex4 = db[vC1].save({});
      var vertexId4 = vertex4._id;
      var edge1 = db[eC1].save(eC4 + "/4", vertexId1, {_key: "1"});
      var edge2 = db[eC2].save(eC1 + "/1", vertexId2, {_key: "2"});
      var edge3 = db[eC3].save(eC2 + "/2", vertexId3, {_key: "3"});
      var edge4 = db[eC4].save(eC3 + "/3", vertexId4, {_key: "4"});

      var g1 = graph._create(
        gN1,
        graph._edgeDefinitions(
          graph._relation(eC1, [eC4], [vC1])
        )
      );
      var g2 = graph._create(
        gN2,
        graph._edgeDefinitions(
          graph._relation(eC2, [eC1], [vC2])
        )
      );
      var g3 = graph._create(
        gN3,
        graph._edgeDefinitions(
          graph._relation(eC3, [eC2], [vC3])
        )
      );
      var g4 = graph._create(
        gN4,
        graph._edgeDefinitions(
          graph._relation(eC4, [eC3], [vC4])
        )
      );

      assertTrue(db._exists(edge1._id));
      assertTrue(db._exists(edge2._id));
      assertTrue(db._exists(edge3._id));
      assertTrue(db._exists(edge4._id));
      assertTrue(db._exists(vertexId1));
      assertTrue(db._exists(vertexId2));
      assertTrue(db._exists(vertexId3));
      assertTrue(db._exists(vertexId4));
      var edge = g1[eC1].remove(edge1._id);
      assertFalse(db._exists(edge1._id));
      assertFalse(db._exists(edge2._id));
      assertFalse(db._exists(edge3._id));
      assertFalse(db._exists(edge4._id));
      assertTrue(db._exists(vertexId1));
      assertTrue(db._exists(vertexId2));
      assertTrue(db._exists(vertexId3));
      assertTrue(db._exists(vertexId4));

      graph._drop(gN1, true);
      graph._drop(gN2, true);
      graph._drop(gN3, true);
      graph._drop(gN4, true);
    },

    test_eC_malformedId : function() {
      [ null, "foo", [ ] ].forEach(function(v) {
        try {
          var x= g[ec2].save(v, v, {});
          fail();
        }
        catch (e) {
          assertEqual(
            e.errorMessage,
            arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
          );
        }
      });
    },

    test_getInVertex : function() {
      var ids = fillCollections();
      var result = g._fromVertex(ids.eId11);
      assertEqual(result._id, ids.vId11);
    },

    test_getOutVertex : function() {
      var ids = fillCollections();
      var result = g._toVertex(ids.eId11);
      assertEqual(result._id, ids.vId12);
      result = g._toVertex(ids.eId25);
      assertEqual(result._id, ids.vId35);
    },

    test_getFromVertexInvalidHandle : function() {
      try {
        g._fromVertex("foobar");
      }
      catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code);
      }
    },

    test_getFromVertexInvalidCollectionType : function() {
      try {
        g._fromVertex(vc1 + "/foobar");
      }
      catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.code);
      }
    },

    test_getFromVertexNonexistingCollection : function() {
      try {
        g._fromVertex("UnitTestsCollectionDoesNotExist/foobar");
      }
      catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.code);
      }
    }

  };
}

function GeneralGraphCommonNeighborsSuite() {
  'use strict';
  var testGraph, actual;

  var v1ColName = "UnitTestsAhuacatlVertex1";
  var v2ColName = "UnitTestsAhuacatlVertex2";
  var eColName = "UnitTestsAhuacatlEdge1";
  var v1;
  var v2;
  var v3;
  var v4;
  var v5;
  var v6;
  var v7;
  var v8;

  var createKeyValueObject = function(key, value) {
    var res = {};
    res[key] = value;
    return res;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(v1ColName);
      db._drop(v2ColName);
      db._drop(eColName);

      var vertex1 = db._create(v1ColName);
      var vertex2 = db._create(v2ColName);
      var edge1 = db._createEdgeCollection(eColName);

      v1 = vertex1.save({ _key: "v1" , hugo : true})._id;
      v2 = vertex1.save({ _key: "v2" ,hugo : true})._id;
      v3 = vertex1.save({ _key: "v3" , heinz : 1})._id;
      v4 = vertex1.save({ _key: "v4" , harald : "meier"})._id;
      v5 = vertex2.save({ _key: "v5" , ageing : true})._id;
      v6 = vertex2.save({ _key: "v6" , harald : "meier", ageing : true})._id;
      v7 = vertex2.save({ _key: "v7" ,harald : "meier"})._id;
      v8 = vertex2.save({ _key: "v8" ,heinz : 1, harald : "meier"})._id;

      function makeEdge(from, to, collection) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1] });
      }

      makeEdge(v1, v2, edge1);
      makeEdge(v2, v3, edge1);
      makeEdge(v3, v5, edge1);
      makeEdge(v2, v6, edge1);
      makeEdge(v6, v7, edge1);
      makeEdge(v4, v7, edge1);
      makeEdge(v3, v7, edge1);
      makeEdge(v8, v1, edge1);
      makeEdge(v3, v5, edge1);
      makeEdge(v3, v8, edge1);

      try {
        db._collection("_graphs").remove("_graphs/bla3");
      } catch (ignore) {
      }
      testGraph = graph._create(
        "bla3",
        graph._edgeDefinitions(
          graph._relation(eColName,
            [v1ColName, v2ColName],
            [v1ColName, v2ColName]
          )
        )
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop(v1ColName);
      db._drop(v2ColName);
      db._drop(eColName);
      try {
        db._collection("_graphs").remove("_graphs/bla3");
      } catch (ignore) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks GRAPH_COMMON_NEIGHBORS() and GRAPH_COMMON_PROPERTIES()
////////////////////////////////////////////////////////////////////////////////


    testNeighborsAnyV3: function () {
      actual = testGraph._neighbors(v3).sort();
      assertTrue(actual[0]._id, v2);
      assertTrue(actual[1]._id, v5);
      assertTrue(actual[2]._id, v7);
      assertTrue(actual[3]._id, v8);
    },

    testNeighborsAnyV6: function () {
      actual = testGraph._neighbors(v6).sort();
      assertTrue(actual[0]._id, v2);
      assertTrue(actual[1]._id, v7);
    },


    testCommonNeighborsAny: function () {
      actual = testGraph._commonNeighbors(v3 , v6);
      assertEqual(actual.length, 1);
      assertEqual(actual[0].left, v3);
      assertEqual(actual[0].right, v6);
      assertEqual(actual[0].neighbors.length, 2);
      actual[0].neighbors.sort();
      assertEqual(actual[0].neighbors[0], v2);
      assertEqual(actual[0].neighbors[1], v7);
      actual = testGraph._countCommonNeighbors(v3 , v6);
      assertEqual(actual[0][v3][0][v6] , 2);
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief checks GRAPH_COMMON_NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsIn: function () {
      actual = testGraph._commonNeighbors({} , {},  {direction : 'inbound'},  {direction : 'inbound'});
      assertEqual(actual.length, 8);

      actual = testGraph._countCommonNeighbors({} , {},  {direction : 'inbound'},  {direction : 'inbound'});
      assertEqual(actual.length, 5);

    },


////////////////////////////////////////////////////////////////////////////////
/// @brief checks GRAPH_COMMON_NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsOut: function () {
      actual = testGraph._commonNeighbors(
        {hugo: true}, {heinz: 1},
        {direction: 'outbound', minDepth: 1, maxDepth: 3},
        {direction: 'outbound', minDepth: 1, maxDepth: 3}
      );

      actual.sort(function(a ,b) {
        if (a.left < b.left) {
          return -1;
        }
        if (b.left < a.left) {
          return 1;
        }
        if (a.right < b.right) {
          return -1;
        }
        if (b.right < a.right) {
          return 1;
        }
        return 0;
      });
      assertEqual(actual.length, 4);
      assertEqual(actual[0].left, v1);
      assertEqual(actual[0].right, v3);
      assertEqual(actual[0].neighbors.length, 4);

      assertEqual(actual[1].left, v1);
      assertEqual(actual[1].right, v8);
      assertEqual(actual[1].neighbors.length, 3);

      assertEqual(actual[2].left, v2);
      assertEqual(actual[2].right, v3);
      assertEqual(actual[2].neighbors.length, 4);

      assertEqual(actual[3].left, v2);
      assertEqual(actual[3].right, v8);
      assertEqual(actual[3].neighbors.length, 3);

      actual = testGraph._countCommonNeighbors(
        {hugo: true }, {heinz: 1},
        {direction: 'outbound', minDepth: 1, maxDepth: 3},
        {direction: 'outbound', minDepth: 1, maxDepth: 3}
      );
      if (actual[0].hasOwnProperty(v1)) {
        if (actual[0][v1][0].hasOwnProperty(v8)) {
          assertEqual(actual[0][v1][0][v8], 3);
          assertEqual(actual[0][v1][1][v3], 4);
        } else {
          assertEqual(actual[0][v1][0][v3], 4);
          assertEqual(actual[0][v1][1][v8], 3);
        }

        if(actual[1][v2][0].hasOwnProperty(v8)) {
          assertEqual(actual[1][v2][0][v8], 3);
          assertEqual(actual[1][v2][1][v3], 4);
        } else {
          assertEqual(actual[1][v2][0][v3], 4);
          assertEqual(actual[1][v2][1][v8], 3);
        }
      } else {
        if(actual[0][v2][0].hasOwnProperty(v8)) {
          assertEqual(actual[0][v2][0][v8], 3);
          assertEqual(actual[0][v2][1][v3], 4);
        } else {
          assertEqual(actual[0][v2][0][v3], 4);
          assertEqual(actual[0][v2][1][v8], 3);
        }

        if (actual[1][v1][0].hasOwnProperty(v8)) {
          assertEqual(actual[1][v1][0][v8], 3);
          assertEqual(actual[1][v1][1][v3], 4);
        } else {
          assertEqual(actual[1][v1][0][v3], 4);
          assertEqual(actual[1][v1][1][v8], 3);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks GRAPH_COMMON_PROPERTIES()
////////////////////////////////////////////////////////////////////////////////

    testCommonProperties: function () {
      actual = testGraph._commonProperties({} ,{} ,{});
      assertEqual(actual.length, 8 );

      actual = testGraph._countCommonProperties({} ,{} ,{});
      assertEqual(actual, [
        createKeyValueObject(v1, 1),
        createKeyValueObject(v2, 1),
        createKeyValueObject(v3, 1),
        createKeyValueObject(v4, 3),
        createKeyValueObject(v5, 1),
        createKeyValueObject(v6, 4),
        createKeyValueObject(v7, 3),
        createKeyValueObject(v8, 4)
      ]);
    },

    testCommonPropertiesWithFilters: function () {
      actual = testGraph._commonProperties({ageing : true} , {harald : 'meier'},  {});
      assertEqual(actual[0][v5][0]._id  , v6);
      var proV6 = actual[1][v6];
      proV6.sort(function(a, b) {
        return a._id > b._id;
      });
      assertEqual(proV6[0]._id, v4);
      assertEqual(proV6[1]._id, v7);
      assertEqual(proV6[2]._id, v8);

      actual = testGraph._countCommonProperties({ageing : true} , {harald : 'meier'},  {});
      assertEqual(actual, [
        createKeyValueObject(v5, 1),
        createKeyValueObject(v6, 3)
      ]);

    },

    testCommonPropertiesWithFiltersAndIgnoringKeyHarald: function () {
      actual = testGraph._commonProperties( {} , {},  {ignoreProperties : 'harald'});

      assertEqual(actual[0][v1][0]._id  , v2);
      assertEqual(actual[1][v2][0]._id  , v1);
      assertEqual(actual[2][v3][0]._id  , v8);
      assertEqual(actual[3][v5][0]._id  , v6);
      assertEqual(actual[4][v6][0]._id  , v5);
      assertEqual(actual[5][v8][0]._id  , v3);

      actual = testGraph._countCommonProperties({} , {},  {ignoreProperties : 'harald'});
      assertEqual(actual, [
        createKeyValueObject(v1, 1),
        createKeyValueObject(v2, 1),
        createKeyValueObject(v3, 1),
        createKeyValueObject(v5, 1),
        createKeyValueObject(v6, 1),
        createKeyValueObject(v8, 1)
      ]);
    }
  };
}

function OrphanCollectionSuite() {
  'use strict';
  var prefix = "UnitTestGraphVertexCollection",
    g1,
    g2,
    gN1 = prefix + "Graph1",
    gN2 = prefix + "Graph2",
    eC1 = prefix + "EdgeCollection1",
    eC2 = prefix + "EdgeCollection2",
    vC1 = prefix + "VertexCollection1",
    vC2 = prefix + "VertexCollection2",
    vC3 = prefix + "VertexCollection3",
    vC4 = prefix + "VertexCollection4",
    vC5 = prefix + "VertexCollection5";

  return {

    setUp : function() {
      try {
        arangodb.db._collection("_graphs").remove(gN1);
      } catch (ignore) {
      }
      try {
        arangodb.db._collection("_graphs").remove(gN2);
      } catch (ignore) {
      }
      g1 = graph._create(
        gN1,
        graph._edgeDefinitions(
          graph._relation(
            eC1, [vC1], [vC1, vC2]
          )
        )
      );
      g2 = graph._create(
        gN2,
        graph._edgeDefinitions(
          graph._relation(
            eC2, [vC3], [vC1]
          )
        )
      );
    },

    tearDown : function() {
      try {
        graph._drop(gN1, true);
      } catch(ignore) { }
      try {
        graph._drop(gN2, true);
      } catch(ignore) { }
      try {
        db[vC1].drop();
      } catch (ignore) {}
      try {
        db[vC4].drop();
      } catch (ignore) {}
    },

    test_getOrphanCollection: function() {
      assertEqual(g1._orphanCollections(), []);
    },

    test_addVertexCollection1: function() {
      g1._addVertexCollection(vC5, true);
      assertEqual(g1._orphanCollections(), [vC5]);
    },

    test_addVertexCollection2: function() {
      try {
        g1._addVertexCollection(vC4, false);
      } catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code);
        assertEqual(e.errorMessage, vC4 + ERRORS.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message);
      }
      assertTrue(db._collection(vC4) === null);
      assertEqual(g1._orphanCollections(), []);
    },

    test_addVertexCollection3: function() {
      try {
        g1._addVertexCollection(eC1, false);
      } catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX.code);
        assertEqual(e.errorMessage, ERRORS.ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX.message);
      }
      assertTrue(db._collection(vC4) === null);
      assertEqual(g1._orphanCollections(), []);
    },

    test_addVertexCollection4: function() {
      try {
        g1._addVertexCollection(vC1);
      } catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF.code);
        assertEqual(e.errorMessage, ERRORS.ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF.message);
      }
    },

    test_removeVertexCollection1: function() {
      var name = "completelyNonsenseNameForACollectionBLUBBBBB";
      try {
        g1._removeVertexCollection(name);
      } catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code);
        assertEqual(e.errorMessage, ERRORS.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message);
      }
    },

    test_removeVertexCollection2: function() {
      g1._addVertexCollection(vC4, true);
      g1._addVertexCollection(vC5, true);
      assertEqual(g1._orphanCollections(), [vC4, vC5]);
      g1._removeVertexCollection(vC4, false);
      assertTrue(db._collection(vC4) !== null);
      assertEqual(g1._orphanCollections(), [vC5]);
      try {
        g1._removeVertexCollection(vC4, true);
      } catch (e) {
        assertEqual(e.errorNum, ERRORS.ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION.code);
        assertEqual(e.errorMessage, ERRORS.ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION.message);
      }
    },

    test_doNotDropOrphanCollectionsUsedInOtherEdgedefinitions: function() {
      assertTrue(db._collection(vC3) !== null);
      g1._addVertexCollection(vC3, true);
      assertTrue(db._collection(vC3) !== null);
      graph._drop(gN1, true);
      assertTrue(db._collection(vC3) !== null);
      graph._drop(gN2, true);
      assertTrue(db._collection(vC3) === null);
    },

    test_doNotDropCollectionsIfUsedAsOrphansInOtherGraphs: function() {
      assertTrue(db._collection(vC3) !== null);
      g1._addVertexCollection(vC3, true);
      assertTrue(db._collection(vC3) !== null);
      graph._drop(gN2, true);
      assertTrue(db._collection(vC3) !== null);
      graph._drop(gN1, true);
      assertTrue(db._collection(vC3) === null);
    },

    test_doNotDropOrphanCollectionsUsedAsOrphansInOtherGraphs: function() {
      assertTrue(db._collection(vC4) === null);
      g1._addVertexCollection(vC4, true);
      g2._addVertexCollection(vC4, true);
      assertTrue(db._collection(vC4) !== null);
      graph._drop(gN1, true);
      assertTrue(db._collection(vC4) !== null);
      graph._drop(gN2, true);
      assertTrue(db._collection(vC4) === null);
    },

    test_doNotAddTheSameOrphanCollectionMultipleTimes: function() {
      g1._addVertexCollection(vC4, true);
      try {
        g1._addVertexCollection(vC4, true);
        fail();
      } catch(err) {
        assertEqual(err.errorNum, arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.code);
        assertEqual(err.errorMessage,
          arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.message);
      }
    }
  };
}

function MeasurementsSuite() {
  'use strict';
  var g;
  var vertexId1, vertexId2;
  var unitTestGraphName = "unitTestGraph";

  var ec1 = "unitTestEdgeCollection1";
  var ec2 = "unitTestEdgeCollection2";
  var ec3 = "unitTestEdgeCollection3";
  var vc1 = "unitTestVertexCollection1";
  var vc2 = "unitTestVertexCollection2";


  var fillCollections = function() {
    var ids = {};
    var vertex = g[vc1].save({first_name: "Tam"});
    ids.vId11 = vertex._id;
    vertex = g[vc1].save({first_name: "Tem", age : 20});
    ids.vId12 = vertex._id;
    vertex = g[vc1].save({first_name: "Tim"});
    ids.vId13 = vertex._id;
    vertex = g[vc1].save({first_name: "Tom"});
    ids.vId14 = vertex._id;
    vertex = g[vc1].save({first_name: "Tum"});
    ids.vId15 = vertex._id;
    vertex = g[vc2].save({first_name: "Tam"});
    ids.vId31 = vertex._id;
    vertex = g[vc2].save({first_name: "Tem"});
    ids.vId32 = vertex._id;
    vertex = g[vc2].save({first_name: "Tim", age : 24});
    ids.vId33 = vertex._id;
    vertex = g[vc2].save({first_name: "Tom"});
    ids.vId34 = vertex._id;
    vertex = g[vc2].save({first_name: "Tum"});
    ids.vId35 = vertex._id;

    ids.eId11 = g[ec1].save(ids.vId11, ids.vId12, {});
    ids.eId12 = g[ec1].save(ids.vId12, ids.vId13, {});
    ids.eId13 = g[ec1].save(ids.vId13, ids.vId14, {});
    ids.eId14 = g[ec1].save(ids.vId14, ids.vId15, {});

    ids.eId21 = g[ec2].save(ids.vId11, ids.vId32, {});
    ids.eId22 = g[ec2].save(ids.vId11, ids.vId31, {});
    ids.eId23 = g[ec2].save(ids.vId14, ids.vId33, {});

    ids.eId31 = g[ec3].save(ids.vId31, ids.vId33, {});
    ids.eId32 = g[ec3].save(ids.vId31, ids.vId34, {});
    ids.eId33 = g[ec3].save(ids.vId33, ids.vId34, {});

    return ids;
  };

  var allIds;

  return {

    setUp : function() {
      g = graph._create(
        unitTestGraphName,
        graph._edgeDefinitions(
          graph._relation(ec1, vc1, vc1),
          graph._relation(ec2, vc1, vc2),
          graph._relation(ec3, vc2, vc2)
        )
      );
      allIds = fillCollections();
    },

    tearDown : function() {
      try  {
        graph._drop(unitTestGraphName, true);
      } catch (e) {

      }
    },

    test_radius : function () {
      var a = g._radius({});
      assertEqual(a , 3);
    },
    test_diameter : function () {
      var a = g._diameter({});
      assertEqual(a , 5);
    },

    test_shortestPaths : function () {
      var a = g._shortestPath([
        {first_name: 'Tim', age : 24},
        {first_name: 'Tom'}
      ], [
        {first_name: 'Tam'},
        {first_name: 'Tem', age : 20}
      ]);
      assertEqual(a.length , 9);
    },

    test_distanceTo : function () {
      var a = g._distanceTo([
        {first_name: 'Tim', age : 24},
        {first_name: 'Tom'}
      ], [
        {first_name: 'Tam'},
        {first_name: 'Tem', age : 20}
      ]);
      assertEqual(a.length , 9);
    }
  };
}

function MeasurementsMovedFromAQLSuite() {
  'use strict';

  const v1 = "UnitTests_Berliner";
  const v2 = "UnitTests_Hamburger";
  const v3 = "UnitTests_Frankfurter";
  const v4 = "UnitTests_Leipziger";
  var vertexIds = {};
  const graphName = "werKenntWen";
  var g;

  var validateNumericValues = function (actual, expected) {
    assertEqual(Object.keys(actual).sort(), Object.keys(expected).sort());
    for (let k of Object.keys(expected)) {
      assertEqual(actual[k].toFixed(4), expected[k].toFixed(4));
    }
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(v1);
      db._drop(v2);
      db._drop(v3);
      db._drop(v4);
      db._drop("UnitTests_KenntAnderenBerliner");
      db._drop("UnitTests_KenntAnderen");

      var KenntAnderenBerliner = "UnitTests_KenntAnderenBerliner";
      var KenntAnderen = "UnitTests_KenntAnderen";

      var Berlin = db._create(v1);
      var Hamburg = db._create(v2);
      var Frankfurt = db._create(v3);
      var Leipzig = db._create(v4);
      db._createEdgeCollection(KenntAnderenBerliner);
      db._createEdgeCollection(KenntAnderen);

      var Anton = Berlin.save({ _key: "Anton", gender: "male", age: 20});
      var Berta = Berlin.save({ _key: "Berta", gender: "female", age: 25});
      var Caesar = Hamburg.save({ _key: "Caesar", gender: "male", age: 30});
      var Dieter = Hamburg.save({ _key: "Dieter", gender: "male", age: 20});
      var Emil = Frankfurt.save({ _key: "Emil", gender: "male", age: 25});
      var Fritz = Frankfurt.save({ _key: "Fritz", gender: "male", age: 30});
      var Gerda = Leipzig.save({ _key: "Gerda", gender: "female", age: 40});

      vertexIds.Anton = Anton._id;
      vertexIds.Berta = Berta._id;
      vertexIds.Caesar = Caesar._id;
      vertexIds.Dieter = Dieter._id;
      vertexIds.Emil = Emil._id;
      vertexIds.Fritz = Fritz._id;
      vertexIds.Gerda = Gerda._id;

      try {
        db._collection("_graphs").remove(graphName);
      } catch (ignore) {
      }
      g = graph._create(
        graphName,
        graph._edgeDefinitions(
          graph._relation(KenntAnderenBerliner, v1, v1),
          graph._relation(KenntAnderen,
            [v1, v2, v3, v4],
            [v1, v2, v3, v4]
          )
        )
      );

      function makeEdge(from, to, collection, distance) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1], entfernung: distance});
      }

      makeEdge(Berta._id, Anton._id, g[KenntAnderenBerliner], 0.1);
      makeEdge(Caesar._id, Anton._id, g[KenntAnderen], 350);
      makeEdge(Caesar._id, Berta._id, g[KenntAnderen], 250.1);
      makeEdge(Berta._id, Gerda._id, g[KenntAnderen], 200);
      makeEdge(Gerda._id, Dieter._id, g[KenntAnderen], "blub");
      makeEdge(Dieter._id, Emil._id, g[KenntAnderen], 300);
      makeEdge(Emil._id, Fritz._id, g[KenntAnderen], 0.2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      graph._drop("werKenntWen", true);
    },

    // Test Radius
    testRadiusDefault: function () {
      var actual = g._radius();
      assertEqual(actual, 3);
    },

    testRadiusWeight: function () {
      var actual = g._radius({weightAttribute: "entfernung", defaultWeight: 80});
      assertEqual(actual.toFixed(1), 450.1);
    },

    testRadiusInbound: function () {
      var actual = g._radius({direction: "inbound"});
      assertEqual(actual, 1);
    },

    testRadiusInboundWeight: function () {
      var actual = g._radius({direction: "inbound", weightAttribute: "entfernung", defaultWeight: 80});
      assertEqual(actual.toFixed(1), 250.1);
    },

    testRadiusOutbound: function () {
      var actual = g._radius({direction: "outbound"});
      assertEqual(actual, 1);
    },

    testRadiusOutboundWeight: function () {
      var actual = g._radius({direction: "outbound", weightAttribute: "entfernung", defaultWeight: 80});
      assertEqual(actual.toFixed(1), 0.2);
    },


    // Test Diameter

    testDiameterDefault: function () {
      var actual = g._diameter();
      assertEqual(actual, 5);
    },

    testDiameterWeight: function () {
      var actual = g._diameter({weightAttribute: "entfernung", defaultWeight: 80});
      assertEqual(actual.toFixed(1), 830.3);
    },

    testDiameterInbound: function () {
      var actual = g._diameter({direction: "inbound"});
      assertEqual(actual, 5);
    },

    testDiameterInboundWeight: function () {
      var actual = g._diameter({direction: "inbound", weightAttribute: "entfernung", defaultWeight: 80});
      assertEqual(actual.toFixed(1), 830.3);
    },

    testDiameterOutbound: function () {
      var actual = g._diameter({direction: "outbound"});
      assertEqual(actual, 5);
    },

    testDiameterOutboundWeight: function () {
      var actual = g._diameter({direction: "outbound", weightAttribute: "entfernung", defaultWeight: 80});
      assertEqual(actual.toFixed(1), 830.3);
    },

    testAbsoluteEccentricityDefault: function () {
      var actual = g._absoluteEccentricity(vertexIds.Anton);
      var expected = {};
      expected[vertexIds.Anton] = 5;
      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityWeight: function () {
      var actual = g._absoluteEccentricity(vertexIds.Anton, {weightAttribute: "entfernung", defaultWeight: 80});
      var expected = {};
      expected[vertexIds.Anton] = 580.3;
      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityInbound: function () {
      var actual = g._absoluteEccentricity(vertexIds.Anton, {direction: "inbound"});
      var expected = {};
      expected[vertexIds.Anton] = 1;
      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityInboundWeight: function () {
      var actual = g._absoluteEccentricity(vertexIds.Anton, {direction: "inbound", weightAttribute: "entfernung", defaultWeight: 80});
      var expected = {};
      expected[vertexIds.Anton] = 250.2;
      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityOutbound: function () {
      var actual = g._absoluteEccentricity(vertexIds.Gerda, {direction: "outbound"});
      var expected = {};
      expected[vertexIds.Gerda] = 3;
      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityOutboundWeight: function () {
      var actual = g._absoluteEccentricity(vertexIds.Gerda, {direction: "outbound", weightAttribute: "entfernung", defaultWeight: 80});
      var expected = {};
      expected[vertexIds.Gerda] = 380.2;

      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityExample: function () {
      var actual = g._absoluteEccentricity({gender: "female"}, {direction: "outbound"});
      var expected = {};
      expected[vertexIds.Berta] = 4;
      expected[vertexIds.Gerda] = 3;

      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityExampleWeight: function () {
      var actual = g._absoluteEccentricity({gender: "female"}, {direction: "outbound", weightAttribute: "entfernung", defaultWeight: 80});
      var expected = {};
      expected[vertexIds.Berta] = 580.2;
      expected[vertexIds.Gerda] = 380.2;

      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityAll: function () {
      var actual = g._absoluteEccentricity({}, {direction: "outbound"});
      var expected = {};
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 4;
      expected[vertexIds.Caesar] = 5;
      expected[vertexIds.Dieter] = 2;
      expected[vertexIds.Emil] = 1;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 3;

      validateNumericValues(actual, expected);
    },

    testAbsoluteEccentricityAllWeight: function () {
      var actual = g._absoluteEccentricity({}, {direction: "outbound", weightAttribute: "entfernung", defaultWeight: 80});
      var expected = {};
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 580.2;
      expected[vertexIds.Caesar] = 830.3;
      expected[vertexIds.Dieter] = 300.2;
      expected[vertexIds.Emil] = 0.2;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 380.2;

      validateNumericValues(actual, expected);
    },

    testEccentricityOutbound: function () {
      var actual = g._eccentricity({direction: "outbound"});
      var expected = {};
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] =  1 / 4;
      expected[vertexIds.Caesar] = 1 / 5;
      expected[vertexIds.Dieter] = 1 / 2;
      expected[vertexIds.Emil] = 1 / 1;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 1 / 3;

      validateNumericValues(actual, expected);
    },

    testEccentricityOutboundWeight: function () {
      var actual = g._eccentricity({direction: "outbound", weightAttribute: "entfernung", defaultWeight: 80});
      var expected = {};
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 0.2 / 580.2;
      expected[vertexIds.Caesar] = 0.2 / 830.3;
      expected[vertexIds.Dieter] = 0.2 / 300.2;
      expected[vertexIds.Emil] = 0.2 / 0.2;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 0.2 / 380.2;

      validateNumericValues(actual, expected);
    },

    testFarness: function () {
      var actual = g._farness(vertexIds.Anton);
      var expected = { };
      expected[vertexIds.Anton] = 16;
      validateNumericValues(actual, expected);
    },

    testFarnessOutbound: function () {
      var actual = g._farness(vertexIds.Anton, {direction: "outbound"});
      var expected = { };
      expected[vertexIds.Anton] = 0;
      validateNumericValues(actual, expected);
    },

    testFarnessExampleWeight: function () {
      var actual = g._farness({gender: "male"}, {weightAttribute: "entfernung", defaultWeight: 80});
      var expected = {};
      expected[vertexIds.Anton] = 1890.9;
      expected[vertexIds.Caesar] = 3140.9;
      expected[vertexIds.Dieter] = 1770.4;
      expected[vertexIds.Emil] = 2670.4;
      expected[vertexIds.Fritz] = 2671.4;
      validateNumericValues(actual, expected);

      // Legacy
      actual = g._absoluteCloseness({gender: "male"}, {weightAttribute: "entfernung", defaultWeight: 80});
      validateNumericValues(actual, expected);
    },

    testFarnessAll: function () {
      var actual = g._farness({});
      var expected = { };
      expected[vertexIds.Anton] = 16;
      expected[vertexIds.Berta] = 12;
      expected[vertexIds.Caesar] = 16;
      expected[vertexIds.Dieter] = 12;
      expected[vertexIds.Emil] = 15;
      expected[vertexIds.Fritz] = 20;
      expected[vertexIds.Gerda] = 11;
      validateNumericValues(actual, expected);
    },

    testFarnessAllOutbound: function () {
      var actual = g._farness({}, {direction: "outbound"});
      var expected = { };
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 11;
      expected[vertexIds.Caesar] = 16;
      expected[vertexIds.Dieter] = 3;
      expected[vertexIds.Emil] = 1;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 6;
      validateNumericValues(actual, expected);
    },

    testClosenessDefault: function () {
      var actual = g._closeness();
      var expected = { };
      expected[vertexIds.Anton] = 11 / 16;
      expected[vertexIds.Berta] = 11 / 12;
      expected[vertexIds.Caesar] = 11 / 16;
      expected[vertexIds.Dieter] = 11 / 12;
      expected[vertexIds.Emil] = 11 / 15;
      expected[vertexIds.Fritz] = 11 / 20;
      expected[vertexIds.Gerda] = 11 / 11;
      validateNumericValues(actual, expected);
    },

    testClosenessOutbound: function () {
      var actual = g._closeness({direction: "outbound"});
      var expected = { };
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 1 / 11;
      expected[vertexIds.Caesar] = 1 / 16;
      expected[vertexIds.Dieter] = 1 / 3;
      expected[vertexIds.Emil] = 1 / 1;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 1 / 6;
      validateNumericValues(actual, expected);
    },

    testAbsoluteBetweness: function () {
      var actual = g._absoluteBetweenness(vertexIds.Anton);
      var expected = { };
      expected[vertexIds.Anton] = 0;
      validateNumericValues(actual, expected);
    },
    
    testAbsoluteBetwenessAll: function () {
      var actual = g._absoluteBetweenness({});
      var expected = { };
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 8;
      expected[vertexIds.Caesar] = 0;
      expected[vertexIds.Dieter] = 8;
      expected[vertexIds.Emil] = 5;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 9;
      validateNumericValues(actual, expected);
    },
    
    testAbsoluteBetwenessExample: function () {
      var actual = g._absoluteBetweenness({gender: "female"});
      var expected = { };
      expected[vertexIds.Berta] = 8;
      expected[vertexIds.Gerda] = 9;
      validateNumericValues(actual, expected);
    },
 
    testAbsoluteBetwenessAllOutbound: function () {
      var actual = g._absoluteBetweenness({}, {direction: "outbound"});
      var expected = { };
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 4;
      expected[vertexIds.Caesar] = 0;
      expected[vertexIds.Dieter] = 6;
      expected[vertexIds.Emil] = 4;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 6;
      validateNumericValues(actual, expected);
    },

    testAbsoluteBetwenessAllInbound: function () {
      var actual = g._absoluteBetweenness({}, {direction: "inbound"});
      var expected = { };
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 4;
      expected[vertexIds.Caesar] = 0;
      expected[vertexIds.Dieter] = 6;
      expected[vertexIds.Emil] = 4;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 6;
      validateNumericValues(actual, expected);
    },

    testBetwenessAny: function () {
      var actual = g._betweenness();
      var expected = { };
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 8 / 9;
      expected[vertexIds.Caesar] = 0;
      expected[vertexIds.Dieter] = 8 / 9;
      expected[vertexIds.Emil] = 5 / 9;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 9 / 9;
      validateNumericValues(actual, expected);
    },
    
    testBetwenessOutbound: function () {
      var actual = g._betweenness({direction: "outbound"});
      var expected = { };
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 4 / 6;
      expected[vertexIds.Caesar] = 0;
      expected[vertexIds.Dieter] = 6 / 6;
      expected[vertexIds.Emil] = 4 / 6;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 6 / 6;
      validateNumericValues(actual, expected);
    },

    testBetwenessInbound: function () {
      var actual = g._betweenness({direction: "inbound"});
      var expected = { };
      expected[vertexIds.Anton] = 0;
      expected[vertexIds.Berta] = 4 / 6;
      expected[vertexIds.Caesar] = 0;
      expected[vertexIds.Dieter] = 6 / 6;
      expected[vertexIds.Emil] = 4 / 6;
      expected[vertexIds.Fritz] = 0;
      expected[vertexIds.Gerda] = 6 / 6;
      validateNumericValues(actual, expected);
    }



  };
}




////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

// OBSOLETE!
// jsunity.run(GeneralGraphAQLQueriesSuite);
// jsunity.run(ChainedFluentAQLResultsSuite);
// jsunity.run(GeneralGraphCommonNeighborsSuite);

jsunity.run(EdgesAndVerticesSuite);
jsunity.run(GeneralGraphCreationSuite);
jsunity.run(OrphanCollectionSuite);
jsunity.run(MeasurementsSuite);
jsunity.run(MeasurementsMovedFromAQLSuite);

return jsunity.done();

