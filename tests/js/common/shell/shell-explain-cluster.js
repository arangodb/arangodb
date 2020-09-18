/*jshint globalstrict:false, strict:false */
/*global assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the statement class
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var ArangoStatement = require("@arangodb/arango-statement").ArangoStatement;
var db = arangodb.db;
var ERRORS = arangodb.errors;

const options = { optimizer: { rules: ["-cluster-one-shard"] } };

function ExplainSuite () {
  'use strict';
  var cn = "UnitTestsExplain";

  return {

    setUpAll : function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
    },

    tearDownAll : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainError : function () {
      var st = new ArangoStatement(db, { query : "for u in" });
      try {
        st.explain();
        fail();
      } catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_PARSE.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainNoBindError : function () {
      var st = new ArangoStatement(db, { query : "for i in [ 1 ] return @f", options });
      try {
        st.explain();
        fail();
      } catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_BIND_PARAMETER_MISSING.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainWithBind : function () {
      var st = new ArangoStatement(db, { query : "for i in [ 1 ] return @f", bindVars: { f : 99 }, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("CalculationNode", node.type);

      node = nodes[2];
      assertEqual("CalculationNode", node.type);

      node = nodes[3];
      assertEqual("EnumerateListNode", node.type);

      node = nodes[4];
      assertEqual("ReturnNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainWithBindCollection : function () {
      var st = new ArangoStatement(db, { query : "for i in @@cn return i", bindVars: { "@cn": cn } });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("i", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("RemoteNode", node.type);

      node = nodes[3];
      assertEqual("GatherNode", node.type);

      node = nodes[4];
      assertEqual("ReturnNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainOk1 : function () {
      var st = new ArangoStatement(db, { query : "for u in [ 1, 2, 3 ] return u", options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("CalculationNode", node.type);

      node = nodes[2];
      assertEqual("EnumerateListNode", node.type);
      assertEqual("u", node.outVariable.name);

      node = nodes[3];
      assertEqual("ReturnNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainOk2 : function () {
      var st = new ArangoStatement(db, { query : "for u in [ 1, 2, 3 ] filter u != 1 for f in u return f", options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("CalculationNode", node.type);

      node = nodes[2];
      assertEqual("EnumerateListNode", node.type);

      node = nodes[3];
      assertEqual("CalculationNode", node.type);

      node = nodes[4];
      assertEqual("FilterNode", node.type);

      node = nodes[5];
      assertEqual("EnumerateListNode", node.type);

      node = nodes[6];
      assertEqual("ReturnNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainRemove1 : function () {
      var st = new ArangoStatement(db, { query : "for u in " + cn + " remove u in " + cn, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("IndexNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("RemoveNode", node.type);
      assertEqual(cn, node.collection);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainRemove2 : function () {
      var st = new ArangoStatement(db, { query : "for u in @@cn remove u in @@cn", bindVars: { "@cn" : cn }, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("IndexNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("RemoveNode", node.type);
      assertEqual(cn, node.collection);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainInsert1 : function () {
      var st = new ArangoStatement(db, { query : "for u in @@cn insert u in @@cn", bindVars: { "@cn": cn }, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("RemoteNode", node.type);

      node = nodes[3];
      assertEqual("GatherNode", node.type);

      node = nodes[4];
      assertEqual("DistributeNode", node.type);

      node = nodes[5];
      assertEqual("RemoteNode", node.type);

      node = nodes[6];
      assertEqual("InsertNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[7];
      assertEqual("RemoteNode", node.type);

      node = nodes[8];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainInsert2 : function () {
      var st = new ArangoStatement(db, { query : "for u in " + cn + " insert u in " + cn, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("RemoteNode", node.type);

      node = nodes[3];
      assertEqual("GatherNode", node.type);

      node = nodes[4];
      assertEqual("DistributeNode", node.type);

      node = nodes[5];
      assertEqual("RemoteNode", node.type);

      node = nodes[6];
      assertEqual("InsertNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[7];
      assertEqual("RemoteNode", node.type);

      node = nodes[8];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainUpdate1 : function () {
      var st = new ArangoStatement(db, {
        query : "for u in @@cn update u._key with u in @@cn",
        bindVars: { "@cn": cn }, options
      });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("CalculationNode", node.type);

      node = nodes[3];
      assertEqual("UpdateNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[4];
      assertEqual("RemoteNode", node.type);

      node = nodes[5];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainUpdate2 : function () {
      var st = new ArangoStatement(db, { query : "for u in " + cn + " update u._key with u in " + cn, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("CalculationNode", node.type);

      node = nodes[3];
      assertEqual("UpdateNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[4];
      assertEqual("RemoteNode", node.type);

      node = nodes[5];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainUpdate3 : function () {
      var st = new ArangoStatement(db, { query : "for u in @@cn update u in @@cn", bindVars: { "@cn": cn }, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("RemoteNode", node.type);

      node = nodes[3];
      assertEqual("GatherNode", node.type);

      node = nodes[4];
      assertEqual("DistributeNode", node.type);

      node = nodes[5];
      assertEqual("RemoteNode", node.type);

      node = nodes[6];
      assertEqual("UpdateNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[7];
      assertEqual("RemoteNode", node.type);

      node = nodes[8];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainUpdate4 : function () {
      var st = new ArangoStatement(db, { query : "for u in " + cn + " update u in " + cn, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("RemoteNode", node.type);

      node = nodes[3];
      assertEqual("GatherNode", node.type);

      node = nodes[4];
      assertEqual("DistributeNode", node.type);

      node = nodes[5];
      assertEqual("RemoteNode", node.type);

      node = nodes[6];
      assertEqual("UpdateNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[7];
      assertEqual("RemoteNode", node.type);

      node = nodes[8];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainReplace1 : function () {
      var st = new ArangoStatement(db, {
        query : "for u in @@cn replace u._key with u in @@cn",
        bindVars: { "@cn": cn }, options
      });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("CalculationNode", node.type);

      node = nodes[3];
      assertEqual("ReplaceNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[4];
      assertEqual("RemoteNode", node.type);

      node = nodes[5];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainReplace2 : function () {
      var st = new ArangoStatement(db, { query : "for u in " + cn + " replace u._key with u in " + cn, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("CalculationNode", node.type);

      node = nodes[3];
      assertEqual("ReplaceNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[4];
      assertEqual("RemoteNode", node.type);

      node = nodes[5];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainReplace3 : function () {
      var st = new ArangoStatement(db, { query : "for u in @@cn replace u in @@cn", bindVars: { "@cn": cn }, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);
      
      node = nodes[2];
      assertEqual("RemoteNode", node.type);

      node = nodes[3];
      assertEqual("GatherNode", node.type);

      node = nodes[4];
      assertEqual("DistributeNode", node.type);

      node = nodes[5];
      assertEqual("RemoteNode", node.type);

      node = nodes[6];
      assertEqual("ReplaceNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[7];
      assertEqual("RemoteNode", node.type);

      node = nodes[8];
      assertEqual("GatherNode", node.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainReplace4 : function () {
      var st = new ArangoStatement(db, { query : "for u in " + cn + " replace u in " + cn, options });
      var nodes = st.explain().plan.nodes, node;

      node = nodes[0];
      assertEqual("SingletonNode", node.type);

      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual("u", node.outVariable.name);
      assertEqual(cn, node.collection);

      node = nodes[2];
      assertEqual("RemoteNode", node.type);

      node = nodes[3];
      assertEqual("GatherNode", node.type);

      node = nodes[4];
      assertEqual("DistributeNode", node.type);

      node = nodes[5];
      assertEqual("RemoteNode", node.type);

      node = nodes[6];
      assertEqual("ReplaceNode", node.type);
      assertEqual(cn, node.collection);

      node = nodes[7];
      assertEqual("RemoteNode", node.type);

      node = nodes[8];
      assertEqual("GatherNode", node.type);
    }

  };
}

jsunity.run(ExplainSuite);
return jsunity.done();
