/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE, AQL_EXECUTEJSON */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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
/// @author Wilfried Goesgens
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var _ = require("lodash");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "optimize-traversals";
  // various choices to control the optimizer: 
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };
  var graphName = "myUnittestGraph";
  var opts = _.clone(paramEnabled);
  var edgeKey;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var graph_module = require("@arangodb/general-graph");
      opts.allPlans = true;
      opts.verbosePlans = true;

      try { graph_module._drop(graphName, true); } catch (x) {}

      var graph = graph_module._create(graphName, [
        graph_module._relation("edges", "circles", ["circles"])]);

      // Add circle circles
      graph.circles.save({"_key": "A", "label": '1'});
      graph.circles.save({"_key": "B", "label": '2'});
      graph.circles.save({"_key": "C", "label": '3'});
      graph.circles.save({"_key": "D", "label": '4'});
      graph.circles.save({"_key": "E", "label": '5'});
      graph.circles.save({"_key": "F", "label": '6'});
      graph.circles.save({"_key": "G", "label": '7'});


      // Add relevant edges
      edgeKey = graph.edges.save("circles/A", "circles/C", {theFalse: false, theTruth: true, "label": 'foo'})._key;
      graph.edges.save("circles/A", "circles/B", {theFalse: false, theTruth: true, "label": 'bar'});
      graph.edges.save("circles/B", "circles/D", {theFalse: false, theTruth: true, "label": 'blarg'});
      graph.edges.save("circles/B", "circles/E", {theFalse: false, theTruth: true, "label": 'blub'});
      graph.edges.save("circles/C", "circles/F", {theFalse: false, theTruth: true, "label": 'schubi'});
      graph.edges.save("circles/C", "circles/G", {theFalse: false, theTruth: true, "label": 'doo'});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      var graph_module = require("@arangodb/general-graph");
      graph_module._drop(graphName, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule removes variables
////////////////////////////////////////////////////////////////////////////////

    testRuleRemoveVariables : function () {
      var queries = [ 
        [ "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN 1", true, [ true, false, false ] ],
        [ "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [v, e]", true, [ true, true, false ] ],
        [ "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [v, p]", true, [ true, false, true ] ],
        [ "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [e, p]", false, [ true, true, true ] ],
        [ "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [v]", true, [ true, false, false ] ],
        [ "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [e]", true, [ true, true, false ] ],
        [ "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [p]", true, [ true, false, true ] ],
        [ "FOR v, e IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [v, e]", false, [ true, true, false ] ],
        [ "FOR v, e IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [v]", true, [ true, false, false ] ],
        [ "FOR v IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' RETURN [v]", false, [ true, false, false ] ]
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { }, paramEnabled);
        assertEqual(query[1], result.plan.rules.indexOf(ruleName) !== -1, query);

        // check that variables were correctly optimized away
        var found = false;
        result.plan.nodes.forEach(function (thisNode) {
          if (thisNode.type === "TraversalNode") {
            assertEqual(query[2][0], thisNode.hasOwnProperty("vertexOutVariable"));
            assertEqual(query[2][1], thisNode.hasOwnProperty("edgeOutVariable"));
            assertEqual(query[2][2], thisNode.hasOwnProperty("pathOutVariable"));
            found = true;
          }
        });
        assertTrue(found);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' LET localScopeVar = NOOPT(true) FILTER p.edges[0].theTruth == localScopeVar return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[-1].theTruth == true return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[*].theTruth == true or p.edges[1].label == 'bar' return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[RAND()].theFalse == false return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[p.edges.length - 1].theFalse == false return {v:v,e:e,p:p}",
        "FOR v, e, p IN 2 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER CONCAT(p.edges[0]._key, '') == " + edgeKey + " SORT v._key RETURN {v:v,e:e,p:p}",
        "FOR v, e, p IN 2 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER NOOPT(CONCAT(p.edges[0]._key, '')) == " + edgeKey + " SORT v._key RETURN {v:v,e:e,p:p}",
        "FOR v, e, p IN 2 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER NOOPT(V8(CONCAT(p.edges[0]._key, ''))) == " + edgeKey + " SORT v._key RETURN {v:v,e:e,p:p}"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[0].theTruth == true RETURN {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[1].theTruth == true RETURN {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[2].theTruth == true RETURN {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[2].theTruth == true AND    p.edges[1].label == 'bar' RETURN {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[0].theTruth == true FILTER p.edges[1].label == 'bar' RETURN {v:v,e:e,p:p}",
        "FOR snippet IN ['a', 'b'] FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[1].label == CONCAT(snippet, 'ar') RETURN {v:v,e:e,p:p}"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result.plan.nodes.forEach(function (thisNode) {
          if (thisNode.type === "TraversalNode") {
            assertTrue(thisNode.hasOwnProperty("condition"), query);
          }
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [ 
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[0].label == 'foo' return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[1].label == 'foo' return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[0].theTruth == true FILTER p.edges[0].label == 'foo' return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[0].theTruth == true FILTER p.edges[1].label == 'foo' return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[0].theTruth == true FILTER p.edges[0].label == 'foo' return {v:v,e:e,p:p}",
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[1].theTruth == true FILTER p.edges[1].label == 'foo' return {v:v,e:e,p:p}",
      ];

      queries.forEach(function(query) {
        var planDisabled   = AQL_EXPLAIN(query, { }, paramDisabled);
        var planEnabled    = AQL_EXPLAIN(query, { }, paramEnabled);
        var resultDisabled = AQL_EXECUTE(query, { }, paramDisabled).json;
        var resultEnabled  = AQL_EXECUTE(query, { }, paramEnabled).json;

        assertTrue(isEqual(resultDisabled, resultEnabled), query);

        assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query);

        var plans = AQL_EXPLAIN(query, {}, opts).plans;
        plans.forEach(function(plan) {
          var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
          assertEqual(jsonResult, resultDisabled, query);
        });

      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test early abort function
////////////////////////////////////////////////////////////////////////////////

    testFunction : function () {
      var queries = [
        "WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol FILTER p.edges[0]._key == CONCAT(@edgeKey, '') SORT v._key RETURN v._key",
        "WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol FILTER p.edges[0]._key == @edgeKey SORT v._key RETURN v._key",
        "WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol FILTER CONCAT(p.edges[0]._key, '') == @edgeKey SORT v._key RETURN v._key",
        "WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol FILTER NOOPT(CONCAT(p.edges[0]._key, '')) == @edgeKey SORT v._key RETURN v._key",
        "WITH circles FOR v, e, p IN 2 OUTBOUND @start @@ecol FILTER NOOPT(V8(CONCAT(p.edges[0]._key, ''))) == @edgeKey SORT v._key RETURN v._key"
      ];
      var bindVars = {
        start: "circles/A",
        "@ecol": "edges",
        edgeKey: edgeKey
      };
      queries.forEach(function (q) {
        var res = AQL_EXECUTE(q, bindVars, paramDisabled).json;
        assertEqual(res.length, 2, "query: " + q);
        assertEqual(res[0], "F", "query: " + q);
        assertEqual(res[1], "G", "query: " + q);
        res = AQL_EXECUTE(q, bindVars, paramEnabled).json;
        assertEqual(res.length, 2, "query (enabled): " + q);
        assertEqual(res[0], "F", "query (enabled): " + q);
        assertEqual(res[1], "G", "query (enabled): " + q);
      });

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test NOOP results
////////////////////////////////////////////////////////////////////////////////

    testNoResults : function () {
      var queries = [ 
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[7].label == 'foo' return {v:v,e:e,p:p}",
        // indexed access starts with 0 - this is also forbidden since it will look for the 6th!
        "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.edges[5].label == 'foo' return {v:v,e:e,p:p}",
        // "FOR v, e, p IN 1..5 OUTBOUND 'circles/A' GRAPH 'myGraph' FILTER p.edges[1].label == 'foo' AND p.edges[1].label == 'bar' return {v:v,e:e,p:p}"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        var simplePlan = helper.getCompactPlan(result);
        
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(simplePlan[2].type, "NoResultsNode");
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple conditions 
////////////////////////////////////////////////////////////////////////////////

    testCondVars1 : function () {
      var queries = [ 
        "LET data = (FOR i IN 1..1 RETURN i) FOR v, e, p IN 1..10 OUTBOUND data GRAPH '" + graphName + "' FILTER p.vertices[0]._id == '123' FILTER p.vertices[1]._id != null FILTER p.edges[0]._id IN data[*].foo.bar RETURN 1"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(0, AQL_EXECUTE(query).json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple conditions 
////////////////////////////////////////////////////////////////////////////////

    testCondVars2 : function () {
      var queries = [ 
        "LET data = (FOR i IN 1..1 RETURN i) FOR v, e, p IN 1..10 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.vertices[0]._id == '123' FILTER p.vertices[1]._id != null FILTER p.edges[0]._id IN data[*].foo.bar RETURN 1"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(0, AQL_EXECUTE(query).json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple conditions 
////////////////////////////////////////////////////////////////////////////////

    testCondVars3 : function () {
      var queries = [ 
        "LET data = (FOR i IN 1..1 RETURN i) FOR v, e, p IN 1..10 OUTBOUND 'circles/A' GRAPH '" + graphName + "' FILTER p.vertices[0]._id == '123' FILTER p.vertices[1]._id != null FILTER p.edges[0]._id IN data[*].foo.bar FILTER p.edges[1]._key IN data[*].bar.baz._id RETURN 1"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(0, AQL_EXECUTE(query).json.length);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

