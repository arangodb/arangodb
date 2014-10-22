/*jshint strict: false */
/*global require, exports, assertTrue, assertEqual, fail, 
  AQL_EXECUTE, AQL_PARSE, AQL_EXPLAIN, AQL_EXECUTEJSON */

////////////////////////////////////////////////////////////////////////////////
/// @brief aql test helper functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangodb = require("org/arangodb");
var ShapedJson = internal.ShapedJson;
var printYaml = function (plan) { require("internal").print(require("js-yaml").safeDump(plan));};

// -----------------------------------------------------------------------------
// --SECTION--                                         AQL test helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief normalise a single row result
////////////////////////////////////////////////////////////////////////////////

function normalizeRow (row, recursive) {
  if (row !== null &&
      typeof row === 'object' &&
      ! Array.isArray(row)) {
    var keys = Object.keys(row);

    keys.sort();

    var i, n = keys.length, out = { };
    for (i = 0; i < n; ++i) {
      var key = keys[i];

      if (key[0] !== '_') {
        out[key] = row[key];
      }
    }

    return out;
  }

  if (recursive && Array.isArray(row)) {
    row = row.map(normalizeRow);
  }

  return row;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the parse results for a query
////////////////////////////////////////////////////////////////////////////////

function getParseResults (query) {
  return AQL_PARSE(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert a specific error code when parsing a query
////////////////////////////////////////////////////////////////////////////////

function assertParseError (errorCode, query) {
  try {
    getParseResults(query);
    fail();
  }
  catch (e) {
    assertTrue(e.errorNum !== undefined, "unexpected error format");
    assertEqual(errorCode, e.errorNum, "unexpected error code (" + e.errorMessage + "): ");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query explanation
////////////////////////////////////////////////////////////////////////////////

function getQueryExplanation (query, bindVars) {
  return AQL_EXPLAIN(query, bindVars);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a modify-query
////////////////////////////////////////////////////////////////////////////////

function getModifyQueryResults (query, bindVars) {
  var queryResult = internal.AQL_QUERY(query, bindVars);

  if (queryResult instanceof arangodb.ArangoCursor) {
    return queryResult.getExtra();
  }

  return queryResult.extra;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a modify-query; compare aql 1 & 2 results
////////////////////////////////////////////////////////////////////////////////

function getModifyQueryResults2 (query, bindVars) {
  var queryResult = AQL_EXECUTE(query, bindVars);

  if (queryResult instanceof arangodb.ArangoCursor) {
    return queryResult.getExtra();
  }

  return queryResult.stats;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query
////////////////////////////////////////////////////////////////////////////////

function getRawQueryResults (query, bindVars) {
  var queryResult = internal.AQL_QUERY(query, bindVars, {
    count: true,
    batchSize : 3000
  });

  if (! (queryResult instanceof arangodb.ArangoCursor)) {
    if (typeof queryResult === 'object' && queryResult.hasOwnProperty('docs')) {
      return queryResult.docs;
    }

    return queryResult;
  }

  var rows = [ ];
  var func = function (row) {
    rows.push(row);
  };

  while (queryResult.hasNext()) {
    queryResult.toArray().forEach(func);
  }
  return rows;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query, version for AQL2
////////////////////////////////////////////////////////////////////////////////

function getRawQueryResultsAQL2 (query, bindVars) {
  var queryResult;
  if (typeof bindVars === "object") {
    queryResult = AQL_EXECUTE(query, bindVars, { 
      count: true, 
      batchSize : 3000 
    });
  }
  else {
    queryResult = AQL_EXECUTE(query, {}, {
      count: true,
      batchSize : 3000
    });
  }
  return queryResult.json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query in a normalised way
////////////////////////////////////////////////////////////////////////////////

function getQueryResults (query, bindVars, recursive) {
  var result = getRawQueryResults(query, bindVars);

  if (Array.isArray(result)) {
    result = result.map(function (row) {
      return normalizeRow(row, recursive);
    });
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query in a normalised way, AQL2 version
////////////////////////////////////////////////////////////////////////////////

function getQueryResultsAQL2 (query, bindVars, recursive) {
  var result = getRawQueryResultsAQL2(query, bindVars);

  if (Array.isArray(result)) {
    result = result.map(function (row) {
      return normalizeRow(row, recursive);
    });
  }

  return result;
}

function typeName (value) {
  if (value === null) {
    return "null";
  }
  if (value === undefined) {
    return "undefined";
  }
  if (value instanceof ShapedJson) {
    return "object";
  }
  if (Array.isArray(value)) {
    return "array";
  }

  var type = typeof value;

  if (type === "object") {
    return "object";
  }
  if (type === "string") {
    return "string";
  }
  if (type === "boolean") {
    return "boolean";
  }
  if (type === "number") {
    return "number";
  }

  throw "unknown variable type";
}

function isEqual (lhs, rhs) {
  var ltype = typeName(lhs), rtype = typeName(rhs), i;

  if (ltype !== rtype) {
    return false;
  }

  if (ltype === "null" || ltype === "undefined") {
    return true;
  }
  
  if (ltype === "array") {
    if (lhs.length !== rhs.length) {
      return false;
    }
    for (i = 0; i < lhs.length; ++i) {
      if (! isEqual(lhs[i], rhs[i])) {
        return false;
      }
    }
    return true;
  }

  if (ltype === "object") {
    var lkeys = Object.keys(lhs), rkeys = Object.keys(rhs);
    if (lkeys.length !== rkeys.length) {
      return false;
    }
    for (i = 0; i < lkeys.length; ++i) {
      var key = lkeys[i];
      if (! isEqual(lhs[key], rhs[key])) {
        return false;
      }
    }
    return true;
  }

  if (ltype === "boolean") {
    return (lhs === rhs);
  }
  if (ltype === "string") {
    return (lhs === rhs);
  }
  if (ltype === "number") {
    if (isNaN(lhs)) {
      return isNaN(rhs);
    }
    if (! isFinite(lhs)) {
      return (lhs === rhs);
    }
    return (lhs.toFixed(10) === rhs.toFixed(10));
  }
  
  throw "unknown variable type";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the results of a query in a normalised way, version to
/// run both the old and the new (AQL2) query
////////////////////////////////////////////////////////////////////////////////

function getQueryResults2 (query, bindVars, recursive) {
  var result = getQueryResults(query, bindVars, recursive);
  var result2 = getQueryResultsAQL2(query, bindVars, recursive);

  if (! isEqual(result, result2)) { 
    require("internal").print("Old and new AQL return different results!");
    require("internal").print("Old result:\n", result);
    require("internal").print("New result:\n", result2);

    require("internal").print("Failed Query was:\n", query);

    throw "Results between AQL and AQL2 differ";
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert a specific error code when running a query
////////////////////////////////////////////////////////////////////////////////

function assertQueryError (errorCode, query, bindVars) {
  try {
    getQueryResults(query, bindVars);
    fail();
  }
  catch (e) {
    assertTrue(e.errorNum !== undefined, "unexpected error format");
    assertEqual(errorCode, e.errorNum, "unexpected error code (" + e.errorMessage + "): ");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert a specific error code when running a query, case of both
/// new and old AQL
////////////////////////////////////////////////////////////////////////////////

function assertQueryError2 (errorCode, query, bindVars) {
  /* 
    probably not needed here, as we want to check only that an AQL2 query fails
  try {
    getQueryResults(query, bindVars);
    fail();
  }
  catch (e) {
    assertTrue(e.errorNum !== undefined, "unexpected error format [" + query + "]");
    assertEqual(errorCode, e.errorNum, "unexpected error code (" + e.errorMessage + "): ");
  }
  */
  try {
    getQueryResultsAQL2(query, bindVars);
    fail();
  }
  catch (e2) {
    assertTrue(e2.errorNum !== undefined, "unexpected error format while calling [" + query + "]");
    assertEqual(errorCode, e2.errorNum, "unexpected error code (" + e2.errorMessage + "): ");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a linearized version of an execution plan
////////////////////////////////////////////////////////////////////////////////

function getLinearizedPlan (explainResult) {
  var nodes = explainResult.plan.nodes, i;
  var lookup = { }, deps = { };

  for (i = 0; i < nodes.length; ++i) {
    var node = nodes[i];
    lookup[node.id] = node;
    var dependency = -1;
    if (node.dependencies.length > 0) {
      dependency = node.dependencies[0];
    }
    deps[dependency] = node.id;
  }

  var current = -1;
  var out = [ ];
  while (true) {
    if (! deps.hasOwnProperty(current)) {
      break;
    }

    var n = lookup[deps[current]];
    current = n.id;
    out.push(n);
  }

  return out;
}

function getCompactPlan (explainResult) {
  var out = [ ];

  function buildExpression (node) {
    var out = node.type;
    if (node.hasOwnProperty("name")) {
      out += "[" + node.name + "]";
    }
    if (node.hasOwnProperty("value")) {
      out += "[" + node.value + "]";
    }

    if (Array.isArray(node.subNodes)) {
      out += "(";
      node.subNodes.forEach(function (node, i) {
        if (i > 0) {
          out += ", ";
        }

        out += buildExpression(node);
      });

      out += ")";
    }
    return out;
  }

  getLinearizedPlan(explainResult).forEach(function (node) {
    var data = { type: node.type };

    if (node.expression) {
      data.expression = buildExpression(node.expression);
    }
    if (node.outVariable) {
      data.outVariable = node.outVariable.name;
    }

    out.push(data);
  });

  return out;
}

function findExecutionNodes(plan, nodetype) {
  var matches = [];
  plan.plan.nodes.forEach(function(node) {
    if (node.type === nodetype) {

      matches.push(node);
    }
    else if (node.type === "SubqueryNode") {
      var subPlan = {"plan" : node.subquery};
      matches = matches.concat(findExecutionNodes(subPlan, nodetype));
    }
  });
  return matches;
}

function findReferencedNodes(plan, testNode) {
  var matches = [];
  if (testNode.elements) {
    testNode.elements.forEach(function(element) {
      plan.plan.nodes.forEach(function(node) {
        if (node.hasOwnProperty("outVariable") && 
            node.outVariable.id ===
            element.inVariable.id) {
          matches.push(node);
        }
      });
    });
  }
  else {
    plan.plan.nodes.forEach(function(node) {
      if (node.outVariable.id === testNode.inVariable.id) {
        matches.push(node);
      }
    });
  }

  return matches;
}

function getQueryMultiplePlansAndExecutions (query, bindVars, testObject, debug) {
  var i;
  var plans = [];
  var allPlans = [];
  var results = [];
  var resetTest = false;
  var paramNone     = { optimizer: { rules: [ "-all" ]},  verbosePlans: true};
  var paramAllPlans = { allPlans : true, verbosePlans: true};

  if (testObject !== undefined) {
    resetTest = true;
  }

  if (debug === undefined) {
    debug = false;
  }

  // first fetch the unmodified version
  if (debug) {
    require("internal").print("Analysing Query unoptimized: " + query);
  }
  plans [0] = AQL_EXPLAIN(query, bindVars, paramNone);
  // then all of the ones permuted by by the optimizer.
  if (debug) {
    require("internal").print("Unoptimized Plan (0):");
    printYaml(plans [0]);

  }
  allPlans = AQL_EXPLAIN(query, bindVars, paramAllPlans);

  for (i=0; i < allPlans.plans.length; i++) {
    if (debug) {
      require("internal").print("Optimized Plan ["+(i+1)+"]:");
      printYaml(allPlans.plans [i]);
    }
    plans[i + 1] = {'plan': allPlans.plans[i]};
  }
  // Now execute each of these variations.
  for (i = 0; i < plans.length; i++) {
    if (debug) {
      require("internal").print("Executing Plan No: " + i + "\n");
    }
    if (resetTest) {
      if (debug) {
        require("internal").print("\nFLUSHING\n");
      }
      testObject.tearDown();
      testObject.setUp();
      if (debug) {
        require("internal").print("\n" + i + " FLUSH DONE\n");
      }
    }

    results[i] = AQL_EXECUTEJSON(plans[i].plan, paramNone);
    // ignore statistics for comparisons
    delete results[i].stats;

    if (debug) {
      require("internal").print("\n" + i + " DONE\n");
    }
  }

  if (debug) {
    require("internal").print("done\n");
  }
  return {'plans': plans, 'results': results};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    module exports
// -----------------------------------------------------------------------------

exports.isEqual                            = isEqual;
exports.getParseResults                    = getParseResults;
exports.assertParseError                   = assertParseError;
exports.getQueryExplanation                = getQueryExplanation;
exports.getModifyQueryResults              = getModifyQueryResults;
exports.getModifyQueryResults2             = getModifyQueryResults2;
exports.getRawQueryResults                 = getRawQueryResults;
exports.getQueryResults                    = getQueryResults;
exports.getQueryResults2                   = getQueryResults2;
exports.getQueryResultsAQL2                = getQueryResultsAQL2;
exports.assertQueryError                   = assertQueryError;
exports.assertQueryError2                  = assertQueryError2;
exports.getLinearizedPlan                  = getLinearizedPlan;
exports.getCompactPlan                     = getCompactPlan;
exports.findExecutionNodes                 = findExecutionNodes;
exports.findReferencedNodes                = findReferencedNodes;
exports.getQueryMultiplePlansAndExecutions = getQueryMultiplePlansAndExecutions;
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
